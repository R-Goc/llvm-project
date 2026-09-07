//===---- LowerMicrosoftCXXABI.cpp - Emit CIR code Microsoft-specific code -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This provides CIR lowering logic targeting the Microsoft C++ ABI.
//
//===----------------------------------------------------------------------===//

#include "CIRCXXABI.h"
#include "LowerModule.h"
#include "llvm/Support/ErrorHandling.h"

namespace cir {

namespace {

static bool recordHasVFPtrAtZero(cir::RecordType rec) {
  if (rec.isIncomplete() || rec.getMembers().empty())
    return false;
  mlir::Type first = rec.getMembers().front();
  if (mlir::isa<cir::VPtrType>(first))
    return true;
  if (auto subRec = mlir::dyn_cast<cir::RecordType>(first))
    return recordHasVFPtrAtZero(subRec);
  return false;
}

static bool nullFieldOffsetIsZero(cir::DataMemberType type) {
  auto model =
      type.getMSInheritance().value_or(cir::MSInheritanceModel::Single);
  if (model >= cir::MSInheritanceModel::Virtual)
    return true;
  return recordHasVFPtrAtZero(type.getClassTy());
}

static cir::IntType getSInt32CIRTy(mlir::MLIRContext *ctx) {
  return cir::IntType::get(ctx, 32, /*is_signed=*/true);
}

static cir::PointerType getVoidPtrCIRTy(mlir::MLIRContext *ctx) {
  return cir::PointerType::get(cir::VoidType::get(ctx));
}

class LowerMicrosoftCXXABI : public CIRCXXABI {
public:
  LowerMicrosoftCXXABI(LowerModule &lm) : CIRCXXABI(lm) {}

  mlir::Type getDataMemberLoweredType(cir::DataMemberType type) const {
    mlir::MLIRContext *ctx = lm.getMLIRContext();
    cir::IntType i32Ty = getSInt32CIRTy(ctx);
    auto model =
        type.getMSInheritance().value_or(cir::MSInheritanceModel::Single);

    switch (model) {
    case cir::MSInheritanceModel::Single:
    case cir::MSInheritanceModel::Multiple:
      return i32Ty;
    case cir::MSInheritanceModel::Virtual: {
      mlir::Type fields[] = {i32Ty, i32Ty};
      return cir::StructType::get(ctx, fields, /*packed=*/false,
                                  /*is_class=*/false,
                                  cir::RecordType::getAllDataKinds(fields));
    }
    case cir::MSInheritanceModel::Unspecified: {
      mlir::Type fields[] = {i32Ty, i32Ty, i32Ty};
      return cir::StructType::get(ctx, fields, /*packed=*/false,
                                  /*is_class=*/false,
                                  cir::RecordType::getAllDataKinds(fields));
    }
    }
    llvm_unreachable("invalid MSInheritanceModel");
  }

  mlir::Type
  lowerDataMemberType(cir::DataMemberType type,
                      const mlir::TypeConverter &typeConverter) const override {
    return getDataMemberLoweredType(type);
  }

  mlir::Type getMethodLoweredType(cir::MethodType type) const {
    mlir::MLIRContext *ctx = lm.getMLIRContext();
    cir::IntType i32Ty = getSInt32CIRTy(ctx);
    cir::PointerType voidPtrTy = getVoidPtrCIRTy(ctx);
    auto model =
        type.getMSInheritance().value_or(cir::MSInheritanceModel::Single);

    switch (model) {
    case cir::MSInheritanceModel::Single:
      return voidPtrTy;
    case cir::MSInheritanceModel::Multiple: {
      mlir::Type fields[] = {voidPtrTy, i32Ty};
      return cir::StructType::get(ctx, fields, /*packed=*/false,
                                  /*is_class=*/false,
                                  cir::RecordType::getAllDataKinds(fields));
    }
    case cir::MSInheritanceModel::Virtual: {
      mlir::Type fields[] = {voidPtrTy, i32Ty, i32Ty};
      return cir::StructType::get(ctx, fields, /*packed=*/false,
                                  /*is_class=*/false,
                                  cir::RecordType::getAllDataKinds(fields));
    }
    case cir::MSInheritanceModel::Unspecified: {
      mlir::Type fields[] = {voidPtrTy, i32Ty, i32Ty, i32Ty};
      return cir::StructType::get(ctx, fields, /*packed=*/false,
                                  /*is_class=*/false,
                                  cir::RecordType::getAllDataKinds(fields));
    }
    }
    llvm_unreachable("invalid MSInheritanceModel");
  }

  mlir::Type
  lowerMethodType(cir::MethodType type,
                  const mlir::TypeConverter &typeConverter) const override {
    return getMethodLoweredType(type);
  }

  mlir::TypedAttr lowerDataMemberConstant(
      cir::DataMemberAttr attr, const mlir::DataLayout &layout,
      const mlir::TypeConverter &typeConverter) const override {
    mlir::MLIRContext *ctx = lm.getMLIRContext();
    cir::IntType i32Ty = getSInt32CIRTy(ctx);
    cir::DataMemberType dmTy = attr.getType();
    auto model =
        dmTy.getMSInheritance().value_or(cir::MSInheritanceModel::Single);

    int64_t memberOffset;
    if (attr.isNullPtr()) {
      memberOffset = nullFieldOffsetIsZero(dmTy) ? 0 : -1;
    } else {
      memberOffset = 0;
      mlir::Type currentTy = dmTy.getClassTy();
      for (int32_t idx : attr.getPath()) {
        auto recTy = mlir::cast<cir::RecordType>(currentTy);
        memberOffset +=
            static_cast<int64_t>(recTy.getElementOffset(layout, idx));
        currentTy = recTy.getMembers()[idx];
      }
    }

    auto firstFieldAttr = cir::IntAttr::get(i32Ty, memberOffset);
    if (model == cir::MSInheritanceModel::Single ||
        model == cir::MSInheritanceModel::Multiple)
      return firstFieldAttr;

    auto zeroAttr = cir::IntAttr::get(i32Ty, 0);
    auto minusOneAttr = cir::IntAttr::get(i32Ty, -1);
    auto structTy =
        mlir::cast<cir::StructType>(getDataMemberLoweredType(dmTy));

    if (model == cir::MSInheritanceModel::Virtual) {
      auto vbaseOffsetAttr = attr.isNullPtr() ? minusOneAttr : zeroAttr;
      return cir::ConstRecordAttr::get(
          structTy,
          mlir::ArrayAttr::get(ctx, {firstFieldAttr, vbaseOffsetAttr}));
    }

    auto vbaseOffsetAttr = attr.isNullPtr() ? minusOneAttr : zeroAttr;
    return cir::ConstRecordAttr::get(
        structTy, mlir::ArrayAttr::get(
                      ctx, {firstFieldAttr, zeroAttr, vbaseOffsetAttr}));
  }

  mlir::TypedAttr lowerDataMemberOffsetConstant(
      cir::DataMemberOffsetAttr attr, const mlir::DataLayout &layout,
      const mlir::TypeConverter &typeConverter) const override {
    mlir::MLIRContext *ctx = lm.getMLIRContext();
    cir::IntType i32Ty = getSInt32CIRTy(ctx);
    cir::DataMemberType dmTy = attr.getType();
    auto model =
        dmTy.getMSInheritance().value_or(cir::MSInheritanceModel::Single);

    auto firstFieldAttr =
        cir::IntAttr::get(i32Ty, static_cast<int64_t>(attr.getOffset()));
    if (model == cir::MSInheritanceModel::Single ||
        model == cir::MSInheritanceModel::Multiple)
      return firstFieldAttr;

    auto zeroAttr = cir::IntAttr::get(i32Ty, 0);
    auto structTy =
        mlir::cast<cir::StructType>(getDataMemberLoweredType(dmTy));
    if (model == cir::MSInheritanceModel::Virtual) {
      return cir::ConstRecordAttr::get(
          structTy, mlir::ArrayAttr::get(ctx, {firstFieldAttr, zeroAttr}));
    }
    return cir::ConstRecordAttr::get(
        structTy,
        mlir::ArrayAttr::get(ctx, {firstFieldAttr, zeroAttr, zeroAttr}));
  }

  mlir::TypedAttr
  lowerMethodConstant(cir::MethodAttr attr, const mlir::DataLayout &layout,
                      const mlir::TypeConverter &typeConverter) const override {
    mlir::MLIRContext *ctx = lm.getMLIRContext();
    cir::IntType i32Ty = getSInt32CIRTy(ctx);
    cir::PointerType voidPtrTy = getVoidPtrCIRTy(ctx);
    cir::MethodType methodTy = attr.getType();
    auto model =
        methodTy.getMSInheritance().value_or(cir::MSInheritanceModel::Single);

    mlir::TypedAttr fnPtrAttr;
    if (attr.isNull()) {
      fnPtrAttr = cir::ConstPtrAttr::get(
          voidPtrTy, mlir::Builder(ctx).getI64IntegerAttr(0));
    } else if (attr.getSymbol()) {
      fnPtrAttr =
          cir::GlobalViewAttr::get(voidPtrTy, attr.getSymbol().value());
    } else {
      fnPtrAttr = cir::ConstPtrAttr::get(
          voidPtrTy, mlir::Builder(ctx).getI64IntegerAttr(0));
    }

    if (model == cir::MSInheritanceModel::Single)
      return fnPtrAttr;

    auto zeroInt = cir::IntAttr::get(i32Ty, 0);
    auto minusOneInt = cir::IntAttr::get(i32Ty, -1);
    auto vbTableOffsetAttr = attr.isNull() ? minusOneInt : zeroInt;
    auto structTy =
        mlir::cast<cir::StructType>(getMethodLoweredType(methodTy));

    if (model == cir::MSInheritanceModel::Multiple) {
      return cir::ConstRecordAttr::get(
          structTy, mlir::ArrayAttr::get(ctx, {fnPtrAttr, zeroInt}));
    }
    if (model == cir::MSInheritanceModel::Virtual) {
      return cir::ConstRecordAttr::get(
          structTy,
          mlir::ArrayAttr::get(ctx, {fnPtrAttr, zeroInt, vbTableOffsetAttr}));
    }

    return cir::ConstRecordAttr::get(
        structTy, mlir::ArrayAttr::get(
                      ctx, {fnPtrAttr, zeroInt, zeroInt, vbTableOffsetAttr}));
  }

  mlir::Operation *
  lowerGetRuntimeMember(cir::GetRuntimeMemberOp op, mlir::Type loweredResultTy,
                        mlir::Value loweredAddr, mlir::Value loweredMember,
                        mlir::OpBuilder &builder) const override {
    mlir::Location loc = op.getLoc();
    mlir::MLIRContext *ctx = op.getContext();
    cir::IntType i32Ty = getSInt32CIRTy(ctx);
    auto byteTy = cir::IntType::get(ctx, 8, /*is_signed=*/false);
    auto bytePtrTy = cir::PointerType::get(
        byteTy,
        mlir::cast<cir::PointerType>(op.getAddr().getType()).getAddrSpace());

    cir::DataMemberType dmTy = op.getMember().getType();
    auto model =
        dmTy.getMSInheritance().value_or(cir::MSInheritanceModel::Single);

    mlir::Value fieldOffset;
    if (model == cir::MSInheritanceModel::Single ||
        model == cir::MSInheritanceModel::Multiple) {
      fieldOffset = loweredMember;
    } else {
      fieldOffset =
          cir::ExtractMemberOp::create(builder, loc, i32Ty, loweredMember, 0);
    }

    auto objectBytesPtr = cir::CastOp::create(
        builder, loc, bytePtrTy, cir::CastKind::bitcast, loweredAddr);
    auto memberBytesPtr = cir::PtrStrideOp::create(
        builder, loc, bytePtrTy, objectBytesPtr, fieldOffset);
    return cir::CastOp::create(builder, loc, loweredResultTy,
                               cir::CastKind::bitcast, memberBytesPtr);
  }

  void lowerGetMethod(cir::GetMethodOp op, mlir::Value &callee,
                      mlir::Value &thisArg, mlir::Value loweredMethod,
                      mlir::Value loweredObjectPtr,
                      mlir::ConversionPatternRewriter &rewriter) const override {
    mlir::Location loc = op.getLoc();
    mlir::MLIRContext *ctx = op.getContext();
    cir::IntType i32Ty = getSInt32CIRTy(ctx);
    cir::PointerType voidPtrTy = getVoidPtrCIRTy(ctx);

    cir::MethodType methodTy = op.getMethod().getType();
    auto model =
        methodTy.getMSInheritance().value_or(cir::MSInheritanceModel::Single);

    mlir::Value fnPtrField;
    mlir::Value thisVal = loweredObjectPtr;

    if (model == cir::MSInheritanceModel::Single) {
      fnPtrField = loweredMethod;
    } else {
      fnPtrField = cir::ExtractMemberOp::create(rewriter, loc, voidPtrTy,
                                               loweredMethod, 0);
      mlir::Value thisAdj =
          cir::ExtractMemberOp::create(rewriter, loc, i32Ty, loweredMethod, 1);

      auto byteTy = cir::IntType::get(ctx, 8, /*is_signed=*/false);
      auto bytePtrTy = cir::PointerType::get(
          byteTy, mlir::cast<cir::PointerType>(loweredObjectPtr.getType())
                      .getAddrSpace());
      auto objectBytesPtr = cir::CastOp::create(
          rewriter, loc, bytePtrTy, cir::CastKind::bitcast, loweredObjectPtr);
      auto adjustedBytesPtr = cir::PtrStrideOp::create(
          rewriter, loc, bytePtrTy, objectBytesPtr, thisAdj);
      thisVal = cir::CastOp::create(rewriter, loc,
                                    loweredObjectPtr.getType(),
                                    cir::CastKind::bitcast, adjustedBytesPtr);
    }

    callee = cir::CastOp::create(rewriter, loc, op.getCallee().getType(),
                                 cir::CastKind::bitcast, fnPtrField);
    if (thisVal.getType() != op.getAdjustedThis().getType())
      thisArg = cir::CastOp::create(rewriter, loc, op.getAdjustedThis().getType(),
                                    cir::CastKind::bitcast, thisVal);
    else
      thisArg = thisVal;
  }

  mlir::Value lowerDataMemberAdjustment(mlir::Location loc,
                                        cir::DataMemberType srcTy,
                                        cir::DataMemberType dstTy,
                                        mlir::Value loweredSrc, int64_t offset,
                                        mlir::OpBuilder &builder) const {
    auto srcModel =
        srcTy.getMSInheritance().value_or(cir::MSInheritanceModel::Single);
    auto dstModel =
        dstTy.getMSInheritance().value_or(cir::MSInheritanceModel::Single);
    bool srcNullZero = nullFieldOffsetIsZero(srcTy);
    bool dstNullZero = nullFieldOffsetIsZero(dstTy);

    if (offset == 0 && srcModel == dstModel && srcNullZero == dstNullZero)
      return loweredSrc;

    mlir::MLIRContext *ctx = builder.getContext();
    cir::IntType i32Ty = getSInt32CIRTy(ctx);

    mlir::Value fieldOffset;
    mlir::Value isNull;

    if (srcModel == cir::MSInheritanceModel::Single ||
        srcModel == cir::MSInheritanceModel::Multiple) {
      fieldOffset = loweredSrc;
      int32_t srcNullVal = srcNullZero ? 0 : -1;
      auto srcNullConst = cir::ConstantOp::create(
          builder, loc, cir::IntAttr::get(i32Ty, srcNullVal));
      isNull = cir::CmpOp::create(builder, loc, cir::CmpOpKind::eq, loweredSrc,
                                  srcNullConst);
    } else {
      fieldOffset =
          cir::ExtractMemberOp::create(builder, loc, i32Ty, loweredSrc, 0);
      unsigned numSrcFields =
          (srcModel == cir::MSInheritanceModel::Virtual) ? 2 : 3;
      auto lastField = cir::ExtractMemberOp::create(
          builder, loc, i32Ty, loweredSrc, numSrcFields - 1);
      auto minusOneConst =
          cir::ConstantOp::create(builder, loc, cir::IntAttr::get(i32Ty, -1));
      isNull = cir::CmpOp::create(builder, loc, cir::CmpOpKind::eq, lastField,
                                  minusOneConst);
    }

    int32_t dstNullVal = dstNullZero ? 0 : -1;
    auto dstNullConst = cir::ConstantOp::create(
        builder, loc, cir::IntAttr::get(i32Ty, dstNullVal));

    auto offsetConst =
        cir::ConstantOp::create(builder, loc, cir::IntAttr::get(i32Ty, offset));
    auto adjusted =
        cir::AddOp::create(builder, loc, i32Ty, fieldOffset, offsetConst);
    adjusted.setNoSignedWrap(true);

    auto newOffset = cir::SelectOp::create(builder, loc, i32Ty, isNull,
                                           dstNullConst, adjusted);

    if (dstModel == cir::MSInheritanceModel::Single ||
        dstModel == cir::MSInheritanceModel::Multiple)
      return newOffset;

    auto zeroConst =
        cir::ConstantOp::create(builder, loc, cir::IntAttr::get(i32Ty, 0));
    auto minusOneConst =
        cir::ConstantOp::create(builder, loc, cir::IntAttr::get(i32Ty, -1));
    auto lastFieldVal = cir::SelectOp::create(builder, loc, i32Ty, isNull,
                                              minusOneConst, zeroConst);

    auto dstStructTy =
        mlir::cast<cir::StructType>(getDataMemberLoweredType(dstTy));

    if (dstModel == cir::MSInheritanceModel::Virtual) {
      mlir::Value result = cir::ConstantOp::create(
          builder, loc, cir::ZeroAttr::get(dstStructTy));
      result = cir::InsertMemberOp::create(builder, loc, result, 0, newOffset);
      result =
          cir::InsertMemberOp::create(builder, loc, result, 1, lastFieldVal);
      return result;
    }

    // Unspecified
    mlir::Value result =
        cir::ConstantOp::create(builder, loc, cir::ZeroAttr::get(dstStructTy));
    result = cir::InsertMemberOp::create(builder, loc, result, 0, newOffset);
    result = cir::InsertMemberOp::create(builder, loc, result, 1, zeroConst);
    result = cir::InsertMemberOp::create(builder, loc, result, 2, lastFieldVal);
    return result;
  }

  mlir::Value lowerMethodAdjustment(mlir::Location loc, cir::MethodType srcTy,
                                    cir::MethodType dstTy,
                                    mlir::Value loweredSrc, int64_t offset,
                                    mlir::OpBuilder &builder) const {
    auto srcModel =
        srcTy.getMSInheritance().value_or(cir::MSInheritanceModel::Single);
    auto dstModel =
        dstTy.getMSInheritance().value_or(cir::MSInheritanceModel::Single);

    if (offset == 0 && srcModel == dstModel)
      return loweredSrc;

    mlir::MLIRContext *ctx = builder.getContext();
    cir::IntType i32Ty = getSInt32CIRTy(ctx);
    cir::PointerType voidPtrTy = getVoidPtrCIRTy(ctx);

    mlir::Value fnPtr;
    mlir::Value oldAdj;
    auto zeroInt =
        cir::ConstantOp::create(builder, loc, cir::IntAttr::get(i32Ty, 0));

    if (srcModel == cir::MSInheritanceModel::Single) {
      fnPtr = loweredSrc;
      oldAdj = zeroInt;
    } else {
      fnPtr = cir::ExtractMemberOp::create(builder, loc, voidPtrTy,
                                           loweredSrc, 0);
      oldAdj = cir::ExtractMemberOp::create(builder, loc, i32Ty,
                                            loweredSrc, 1);
    }

    auto nullPtr = cir::ConstantOp::create(
        builder, loc,
        cir::ConstPtrAttr::get(voidPtrTy, builder.getI64IntegerAttr(0)));
    auto isNull =
        cir::CmpOp::create(builder, loc, cir::CmpOpKind::eq, fnPtr, nullPtr);

    auto offsetConst =
        cir::ConstantOp::create(builder, loc, cir::IntAttr::get(i32Ty, offset));
    auto adjusted =
        cir::AddOp::create(builder, loc, i32Ty, oldAdj, offsetConst);
    adjusted.setNoSignedWrap(true);

    auto newAdj =
        cir::SelectOp::create(builder, loc, i32Ty, isNull, zeroInt, adjusted);

    if (dstModel == cir::MSInheritanceModel::Single)
      return fnPtr;

    auto dstStructTy =
        mlir::cast<cir::StructType>(getMethodLoweredType(dstTy));
    mlir::Value result =
        cir::ConstantOp::create(builder, loc, cir::ZeroAttr::get(dstStructTy));
    result = cir::InsertMemberOp::create(builder, loc, result, 0, fnPtr);
    result = cir::InsertMemberOp::create(builder, loc, result, 1, newAdj);
    if (dstModel == cir::MSInheritanceModel::Virtual) {
      auto minusOneConst =
          cir::ConstantOp::create(builder, loc, cir::IntAttr::get(i32Ty, -1));
      auto lastFieldVal = cir::SelectOp::create(builder, loc, i32Ty, isNull,
                                                minusOneConst, zeroInt);
      result = cir::InsertMemberOp::create(builder, loc, result, 2, lastFieldVal);
    } else if (dstModel == cir::MSInheritanceModel::Unspecified) {
      auto minusOneConst =
          cir::ConstantOp::create(builder, loc, cir::IntAttr::get(i32Ty, -1));
      auto lastFieldVal = cir::SelectOp::create(builder, loc, i32Ty, isNull,
                                                minusOneConst, zeroInt);
      result = cir::InsertMemberOp::create(builder, loc, result, 3, lastFieldVal);
    }
    return result;
  }

  mlir::Value lowerBaseDataMember(cir::BaseDataMemberOp op,
                                  mlir::Value loweredSrc,
                                  mlir::OpBuilder &builder) const override {
    return lowerDataMemberAdjustment(op.getLoc(), op.getSrc().getType(),
                                     op.getType(), loweredSrc,
                                     -op.getOffset().getSExtValue(), builder);
  }

  mlir::Value lowerDerivedDataMember(cir::DerivedDataMemberOp op,
                                     mlir::Value loweredSrc,
                                     mlir::OpBuilder &builder) const override {
    return lowerDataMemberAdjustment(op.getLoc(), op.getSrc().getType(),
                                     op.getType(), loweredSrc,
                                     op.getOffset().getSExtValue(), builder);
  }

  mlir::Value lowerBaseMethod(cir::BaseMethodOp op, mlir::Value loweredSrc,
                              mlir::OpBuilder &builder) const override {
    return lowerMethodAdjustment(op.getLoc(), op.getSrc().getType(),
                                 op.getType(), loweredSrc,
                                 -op.getOffset().getSExtValue(), builder);
  }

  mlir::Value lowerDerivedMethod(cir::DerivedMethodOp op,
                                 mlir::Value loweredSrc,
                                 mlir::OpBuilder &builder) const override {
    return lowerMethodAdjustment(op.getLoc(), op.getSrc().getType(),
                                 op.getType(), loweredSrc,
                                 op.getOffset().getSExtValue(), builder);
  }

  mlir::Value lowerDataMemberCmp(cir::CmpOp op, mlir::Value loweredLhs,
                                 mlir::Value loweredRhs,
                                 mlir::OpBuilder &builder) const override {
    cir::DataMemberType dmTy =
        mlir::cast<cir::DataMemberType>(op.getLhs().getType());
    auto model =
        dmTy.getMSInheritance().value_or(cir::MSInheritanceModel::Single);

    if (model == cir::MSInheritanceModel::Single ||
        model == cir::MSInheritanceModel::Multiple)
      return cir::CmpOp::create(builder, op.getLoc(), op.getKind(), loweredLhs,
                                loweredRhs);

    mlir::Location loc = op.getLoc();
    cir::IntType i32Ty = getSInt32CIRTy(op.getContext());
    unsigned numFields = (model == cir::MSInheritanceModel::Virtual) ? 2 : 3;

    mlir::Value allFieldsEqual;
    for (unsigned i = 0; i < numFields; ++i) {
      auto lhsField =
          cir::ExtractMemberOp::create(builder, loc, i32Ty, loweredLhs, i);
      auto rhsField =
          cir::ExtractMemberOp::create(builder, loc, i32Ty, loweredRhs, i);
      auto cmp = cir::CmpOp::create(builder, loc, cir::CmpOpKind::eq,
                                    lhsField, rhsField);
      if (i == 0) {
        allFieldsEqual = cmp.getResult();
      } else {
        allFieldsEqual = cir::AndOp::create(builder, loc, cmp.getType(),
                                            allFieldsEqual, cmp.getResult())
                             .getResult();
      }
    }

    if (op.getKind() == cir::CmpOpKind::eq)
      return allFieldsEqual;
    return cir::NotOp::create(builder, loc, allFieldsEqual.getType(),
                              allFieldsEqual);
  }

  mlir::Value lowerMethodCmp(cir::CmpOp op, mlir::Value loweredLhs,
                             mlir::Value loweredRhs,
                             mlir::OpBuilder &builder) const override {
    cir::MethodType methodTy =
        mlir::cast<cir::MethodType>(op.getLhs().getType());
    auto model =
        methodTy.getMSInheritance().value_or(cir::MSInheritanceModel::Single);

    if (model == cir::MSInheritanceModel::Single)
      return cir::CmpOp::create(builder, op.getLoc(), op.getKind(), loweredLhs,
                                loweredRhs);

    mlir::Location loc = op.getLoc();
    cir::PointerType voidPtrTy = getVoidPtrCIRTy(op.getContext());
    cir::IntType i32Ty = getSInt32CIRTy(op.getContext());
    unsigned numFields =
        (model == cir::MSInheritanceModel::Multiple)   ? 2
        : (model == cir::MSInheritanceModel::Virtual)  ? 3
                                                       : 4;

    auto lhsFn = cir::ExtractMemberOp::create(builder, loc, voidPtrTy,
                                             loweredLhs, 0);
    auto rhsFn = cir::ExtractMemberOp::create(builder, loc, voidPtrTy,
                                             loweredRhs, 0);
    auto fnCmp = cir::CmpOp::create(builder, loc, cir::CmpOpKind::eq, lhsFn,
                                    rhsFn);

    auto nullPtr = cir::ConstantOp::create(
        builder, loc,
        cir::ConstPtrAttr::get(voidPtrTy, builder.getI64IntegerAttr(0)));
    auto lhsIsNull = cir::CmpOp::create(builder, loc, cir::CmpOpKind::eq,
                                        lhsFn, nullPtr);

    mlir::Value otherFieldsEqual;
    for (unsigned i = 1; i < numFields; ++i) {
      auto lhsField =
          cir::ExtractMemberOp::create(builder, loc, i32Ty, loweredLhs, i);
      auto rhsField =
          cir::ExtractMemberOp::create(builder, loc, i32Ty, loweredRhs, i);
      auto cmp = cir::CmpOp::create(builder, loc, cir::CmpOpKind::eq,
                                    lhsField, rhsField);
      if (i == 1) {
        otherFieldsEqual = cmp.getResult();
      } else {
        otherFieldsEqual = cir::AndOp::create(builder, loc, cmp.getType(),
                                              otherFieldsEqual, cmp.getResult())
                               .getResult();
      }
    }

    auto nullOrEqual = cir::OrOp::create(builder, loc, lhsIsNull.getType(),
                                         lhsIsNull, otherFieldsEqual);
    auto isEq = cir::AndOp::create(builder, loc, fnCmp.getType(), fnCmp,
                                   nullOrEqual);

    if (op.getKind() == cir::CmpOpKind::eq)
      return isEq;
    return cir::NotOp::create(builder, loc, isEq.getType(), isEq);
  }

  mlir::Value lowerDataMemberBitcast(cir::CastOp op, mlir::Type loweredDstTy,
                                     mlir::Value loweredSrc,
                                     mlir::OpBuilder &builder) const override {
    cir::DataMemberType srcTy =
        mlir::cast<cir::DataMemberType>(op.getSrc().getType());
    cir::DataMemberType dstTy =
        mlir::cast<cir::DataMemberType>(op.getType());
    bool srcNullZero = nullFieldOffsetIsZero(srcTy);
    bool dstNullZero = nullFieldOffsetIsZero(dstTy);

    if (srcNullZero != dstNullZero &&
        (srcTy.getMSInheritance().value_or(cir::MSInheritanceModel::Single) <=
         cir::MSInheritanceModel::Multiple)) {
      mlir::Location loc = op.getLoc();
      cir::IntType i32Ty = getSInt32CIRTy(op.getContext());
      int32_t srcNullVal = srcNullZero ? 0 : -1;
      int32_t dstNullVal = dstNullZero ? 0 : -1;
      auto srcNullConst = cir::ConstantOp::create(
          builder, loc, cir::IntAttr::get(i32Ty, srcNullVal));
      auto dstNullConst = cir::ConstantOp::create(
          builder, loc, cir::IntAttr::get(i32Ty, dstNullVal));
      auto isNull = cir::CmpOp::create(builder, loc, cir::CmpOpKind::eq,
                                       loweredSrc, srcNullConst);
      return cir::SelectOp::create(builder, loc, i32Ty, isNull, dstNullConst,
                                   loweredSrc);
    }

    if (loweredSrc.getType() == loweredDstTy)
      return loweredSrc;
    return cir::CastOp::create(builder, op.getLoc(), loweredDstTy,
                               cir::CastKind::bitcast, loweredSrc);
  }

  mlir::Value
  lowerDataMemberToBoolCast(cir::CastOp op, mlir::Value loweredSrc,
                            mlir::OpBuilder &builder) const override {
    mlir::Location loc = op.getLoc();
    cir::DataMemberType dmTy =
        mlir::cast<cir::DataMemberType>(op.getSrc().getType());
    auto model =
        dmTy.getMSInheritance().value_or(cir::MSInheritanceModel::Single);
    cir::IntType i32Ty = getSInt32CIRTy(op.getContext());

    if (model == cir::MSInheritanceModel::Single ||
        model == cir::MSInheritanceModel::Multiple) {
      int32_t nullVal = nullFieldOffsetIsZero(dmTy) ? 0 : -1;
      auto nullConst = cir::ConstantOp::create(
          builder, loc, cir::IntAttr::get(i32Ty, nullVal));
      return cir::CmpOp::create(builder, loc, cir::CmpOpKind::ne, loweredSrc,
                                nullConst);
    }

    unsigned numFields = (model == cir::MSInheritanceModel::Virtual) ? 2 : 3;
    auto zeroConst =
        cir::ConstantOp::create(builder, loc, cir::IntAttr::get(i32Ty, 0));
    auto minusOneConst =
        cir::ConstantOp::create(builder, loc, cir::IntAttr::get(i32Ty, -1));

    mlir::Value isNonZeroField0 = cir::CmpOp::create(
        builder, loc, cir::CmpOpKind::ne,
        cir::ExtractMemberOp::create(builder, loc, i32Ty, loweredSrc, 0),
        zeroConst);
    mlir::Value result = isNonZeroField0;

    if (numFields == 3) {
      mlir::Value isNonZeroField1 = cir::CmpOp::create(
          builder, loc, cir::CmpOpKind::ne,
          cir::ExtractMemberOp::create(builder, loc, i32Ty, loweredSrc, 1),
          zeroConst);
      result = cir::OrOp::create(builder, loc, result.getType(), result,
                                 isNonZeroField1);
    }

    mlir::Value isNonMinusOneLast = cir::CmpOp::create(
        builder, loc, cir::CmpOpKind::ne,
        cir::ExtractMemberOp::create(builder, loc, i32Ty, loweredSrc,
                                     numFields - 1),
        minusOneConst);
    result = cir::OrOp::create(builder, loc, result.getType(), result,
                               isNonMinusOneLast);
    return result;
  }

  mlir::Value lowerMethodBitcast(cir::CastOp op, mlir::Type loweredDstTy,
                                 mlir::Value loweredSrc,
                                 mlir::OpBuilder &builder) const override {
    if (loweredSrc.getType() == loweredDstTy)
      return loweredSrc;
    return cir::CastOp::create(builder, op.getLoc(), loweredDstTy,
                               cir::CastKind::bitcast, loweredSrc);
  }

  mlir::Value lowerMethodToBoolCast(cir::CastOp op, mlir::Value loweredSrc,
                                    mlir::OpBuilder &builder) const override {
    mlir::Location loc = op.getLoc();
    cir::MethodType methodTy =
        mlir::cast<cir::MethodType>(op.getSrc().getType());
    auto model =
        methodTy.getMSInheritance().value_or(cir::MSInheritanceModel::Single);
    cir::PointerType voidPtrTy = getVoidPtrCIRTy(op.getContext());
    auto nullPtr = cir::ConstantOp::create(
        builder, loc,
        cir::ConstPtrAttr::get(voidPtrTy, builder.getI64IntegerAttr(0)));

    mlir::Value fnPtr =
        (model == cir::MSInheritanceModel::Single)
            ? loweredSrc
            : cir::ExtractMemberOp::create(builder, loc, voidPtrTy,
                                           loweredSrc, 0)
                  .getResult();
    return cir::CmpOp::create(builder, loc, cir::CmpOpKind::ne, fnPtr,
                              nullPtr);
  }

  mlir::Value lowerDynamicCast(cir::DynamicCastOp op,
                               mlir::OpBuilder &builder) const override {
    llvm_unreachable("Microsoft ABI dynamic cast lowering NYI");
  }

  mlir::Value lowerVTableGetTypeInfo(cir::VTableGetTypeInfoOp op,
                                     mlir::OpBuilder &builder) const override {
    llvm_unreachable("Microsoft ABI vtable get type info lowering NYI");
  }

  clang::CharUnits
  getArrayCookieSizeImpl(mlir::Type elementType,
                         const mlir::DataLayout &dataLayout) const override {
    unsigned ptrSize = getPtrSizeInBits() / 8;
    unsigned elemAlign = dataLayout.getTypeABIAlignment(elementType);
    return clang::CharUnits::fromQuantity(std::max(ptrSize, elemAlign));
  }

  mlir::Value readArrayCookieImpl(mlir::Location loc, mlir::Value allocPtr,
                                  clang::CharUnits cookieSize,
                                  clang::CharUnits cookieAlignment,
                                  const mlir::DataLayout &dataLayout,
                                  CIRBaseBuilderTy &builder) const override {
    mlir::Type sizeTy = builder.getUIntNTy(getPtrSizeInBits());
    mlir::Value cookiePtr =
        builder.createBitcast(allocPtr, builder.getPointerTo(sizeTy));
    return builder.createLoad(loc, cookiePtr);
  }
};

} // namespace

std::unique_ptr<CIRCXXABI> createMicrosoftCXXABI(LowerModule &lm) {
  return std::make_unique<LowerMicrosoftCXXABI>(lm);
}

} // namespace cir
