//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This provides C++ code generation targeting the Microsoft C++ ABI.  The class
// in this file generates structures that follow the Microsoft C++ ABI, which is
// documented at:
//  https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170
//
//===----------------------------------------------------------------------===//

#include "CIRGenCXXABI.h"
#include "CIRGenFunction.h"
#include "CIRGenFunctionInfo.h"
#include "CIRGenModule.h"

#include "clang/AST/Attr.h"
#include "clang/AST/CXXInheritance.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/VTableBuilder.h"
#include "clang/Basic/TargetInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::CIRGen;

namespace {

static bool isDeletingDtor(GlobalDecl gd) {
  return isa<CXXDestructorDecl>(gd.getDecl()) &&
         (gd.getDtorType() == Dtor_Deleting ||
          gd.getDtorType() == Dtor_VectorDeleting);
}

class CIRGenMicrosoftCXXABI : public CIRGenCXXABI {
public:
  CIRGenMicrosoftCXXABI(CIRGenModule &cgm) : CIRGenCXXABI(cgm) {}

  bool hasThisReturn(GlobalDecl gd) const override {
    return isa<CXXConstructorDecl>(gd.getDecl());
  }

  bool hasMostDerivedReturn(GlobalDecl gd) const override {
    return isDeletingDtor(gd);
  }

  bool classifyReturnType(CIRGenFunctionInfo &fi) const override;

  StringRef getPureVirtualCallName() override { return "_purecall"; }
  StringRef getDeletedVirtualCallName() override { return "_purecall"; }

  bool canSpeculativelyEmitVTable(const CXXRecordDecl *rd) const override {
    return false;
  }

  bool doStructorsInitializeVPtrs(const CXXRecordDecl *vtableClass) override {
    return !vtableClass->hasAttr<MSNoVTableAttr>();
  }

  bool exportThunk() override { return false; }

  bool useThunkForDtorVariant(const CXXDestructorDecl *dtor,
                              CXXDtorType dt) const override {
    return dt != Dtor_Base;
  }

  bool isZeroInitializable(const MemberPointerType *mpt) override {
    if (mpt->isMemberFunctionPointer())
      return true;

    const CXXRecordDecl *rd = mpt->getMostRecentCXXRecordDecl();
    MSInheritanceModel inheritance = rd->getMSInheritanceModel();
    return (!inheritanceModelHasVBTableOffsetField(inheritance) &&
            rd->nullFieldOffsetIsZero());
  }

  bool requiresArrayCookie(const CXXNewExpr *e) override {
    return e->getAllocatedType().isDestructedType();
  }

  CharUnits getArrayCookieSizeImpl(QualType elementType) override {
    ASTContext &ctx = cgm.getASTContext();
    return std::max(ctx.getTypeSizeInChars(ctx.getSizeType()),
                    ctx.getTypeAlignInChars(elementType));
  }

  Address initializeArrayCookie(CIRGenFunction &cgf, Address newPtr,
                                mlir::Value numElements, const CXXNewExpr *e,
                                QualType elementType) override;

  mlir::Value readArrayCookieImpl(CIRGenFunction &cgf, Address allocPtr,
                                  CharUnits cookieSize) override;

  cir::GlobalLinkageKind
  getCXXDestructorLinkage(GVALinkage linkage, const CXXDestructorDecl *dtor,
                          CXXDtorType dt) const override {
    if (linkage == GVA_Internal)
      return cir::GlobalLinkageKind::InternalLinkage;

    switch (dt) {
    case Dtor_Base:
      return cgm.getCIRLinkageForDeclarator(dtor, linkage);
    case Dtor_Complete:
      if (dtor->hasAttr<DLLExportAttr>())
        return cir::GlobalLinkageKind::WeakODRLinkage;
      return linkage == GVA_DiscardableODR
                 ? cir::GlobalLinkageKind::LinkOnceODRLinkage
                 : cir::GlobalLinkageKind::WeakODRLinkage;
    case Dtor_Deleting:
    case Dtor_VectorDeleting:
      return cir::GlobalLinkageKind::LinkOnceODRLinkage;
    case Dtor_Comdat:
      llvm_unreachable("emitting dtor comdat as function?");
    case Dtor_Unified:
      llvm_unreachable("unexpected unified dtor type");
    }
    llvm_unreachable("invalid dtor type");
  }

  void adjustCallArgsForDestructorThunk(CIRGenFunction &cgf, GlobalDecl gd,
                                        CallArgList &callArgs) override {
    assert((gd.getDtorType() == Dtor_VectorDeleting ||
            gd.getDtorType() == Dtor_Deleting) &&
           "Only vector deleting destructor thunks are available in this ABI");
    callArgs.add(RValue::get(getStructorImplicitParamValue(cgf)),
                 cgm.getASTContext().IntTy);
  }

  void setThunkLinkage(cir::FuncOp thunk, bool forVTable, GlobalDecl gd,
                       bool returnAdjustment) override {
    GVALinkage linkage = cgm.getASTContext().GetGVALinkageForFunction(
        cast<FunctionDecl>(gd.getDecl()));

    if (linkage == GVA_Internal)
      thunk.setLinkage(cir::GlobalLinkageKind::InternalLinkage);
    else if (returnAdjustment)
      thunk.setLinkage(cir::GlobalLinkageKind::WeakODRLinkage);
    else
      thunk.setLinkage(cir::GlobalLinkageKind::LinkOnceODRLinkage);
  }

  AddedStructorArgCounts
  buildStructorSignature(GlobalDecl gd,
                         SmallVectorImpl<CanQualType> &argTys) override;

  void addImplicitStructorParams(CIRGenFunction &cgf, QualType &resTy,
                                 FunctionArgList &params) override;

  void emitInstanceFunctionProlog(SourceLocation loc,
                                  CIRGenFunction &cgf) override;

  AddedStructorArgs getImplicitConstructorArgs(CIRGenFunction &cgf,
                                               const CXXConstructorDecl *d,
                                               CXXCtorType type,
                                               bool forVirtualBase,
                                               bool delegating) override;

  mlir::Value getCXXDestructorImplicitParam(CIRGenFunction &cgf,
                                            const CXXDestructorDecl *dd,
                                            CXXDtorType type,
                                            bool forVirtualBase,
                                            bool delegating) override {
    return nullptr;
  }

  void emitCXXConstructors(const CXXConstructorDecl *d) override {
    cgm.emitGlobal(GlobalDecl(d, Ctor_Complete));
  }

  void emitCXXDestructors(const CXXDestructorDecl *d) override;
  void emitCXXStructor(GlobalDecl gd) override;

  void emitDestructorCall(CIRGenFunction &cgf, const CXXDestructorDecl *dd,
                          CXXDtorType type, bool forVirtualBase,
                          bool delegating, Address thisAddr,
                          QualType thisTy) override;

  mlir::Value emitVirtualDestructorCall(CIRGenFunction &cgf,
                                        const CXXDestructorDecl *dtor,
                                        CXXDtorType dtorType, Address thisAddr,
                                        DeleteOrMemberCallExpr e) override;

  void emitVirtualObjectDelete(CIRGenFunction &cgf, const CXXDeleteExpr *de,
                               Address ptr, QualType elementType,
                               const CXXDestructorDecl *dtor) override;

  void emitConditionalArrayDtorCall(CIRGenFunction &cgf,
                                    const CXXDestructorDecl *dd,
                                    mlir::Value shouldDeleteCondition) override;

  size_t getSrcArgforCopyCtor(const CXXConstructorDecl *cd,
                              FunctionArgList &args) const override {
    assert(args.size() >= 2 &&
           "expected the arglist to have at least two args!");
    // The 'most_derived' parameter goes second if the ctor is variadic and
    // has v-bases.
    if (cd->getParent()->getNumVBases() > 0 &&
        cd->getType()->castAs<FunctionProtoType>()->isVariadic())
      return 2;
    return 1;
  }

  const CXXRecordDecl *getThisArgumentTypeForMethod(GlobalDecl gd) override;

  void emitVBPtrStores(CIRGenFunction &cgf, const CXXRecordDecl *rd) override;
  std::vector<CharUnits> getVBPtrOffsets(const CXXRecordDecl *rd) override;

  CharUnits getVirtualFunctionPrologueThisAdjustment(GlobalDecl gd);

  mlir::Value getVBaseOffsetFromVBPtr(CIRGenFunction &cgf, mlir::Location loc,
                                      Address thisAddr, int64_t vbPtrOffset,
                                      unsigned vbTableIndex,
                                      mlir::Value *vbPtrOut = nullptr);

  struct VBTableGlobals {
    const VPtrInfoVector *VBTables;
    SmallVector<cir::GlobalOp, 2> Globals;
  };
  llvm::DenseMap<const CXXRecordDecl *, VBTableGlobals> vbTablesMap;

  const VBTableGlobals &enumerateVBTables(const CXXRecordDecl *rd);
  cir::GlobalOp getAddrOfVBTable(const VPtrInfo &vbt, const CXXRecordDecl *rd,
                                 cir::GlobalLinkageKind linkage);
  void emitVBTableDefinition(const VPtrInfo &vbt, const CXXRecordDecl *rd,
                             cir::GlobalOp gv) const;

  MicrosoftMangleContext &getMangleContext() {
    return *cast<MicrosoftMangleContext>(mangleContext.get());
  }

  Address adjustThisArgumentForVirtualFunctionCall(CIRGenFunction &cgf,
                                                   GlobalDecl gd,
                                                   Address thisAddr,
                                                   bool virtualCall) override;

  bool isVirtualOffsetNeededForVTableField(CIRGenFunction &cgf,
                                           CIRGenFunction::VPtr vptr) override {
    return vptr.nearestVBase != nullptr;
  }

  using VFTableIdTy = std::pair<const CXXRecordDecl *, CharUnits>;
  llvm::DenseMap<VFTableIdTy, cir::GlobalOp> vftablesMap;
  llvm::SmallPtrSet<const CXXRecordDecl *, 4> deferredVFTables;

  cir::GlobalOp getAddrOfVTable(const CXXRecordDecl *rd,
                                CharUnits vptrOffset) override;

  mlir::Value getVTableAddressPoint(BaseSubobject base,
                                    const CXXRecordDecl *vtableClass) override;

  cir::GlobalViewAttr
  getVTableAddressPointAttr(BaseSubobject base,
                            const CXXRecordDecl *vtableClass) override;

  mlir::Value getVTableAddressPointInStructor(
      CIRGenFunction &cgf, const CXXRecordDecl *vtableClass, BaseSubobject base,
      const CXXRecordDecl *nearestVBase) override;

  CIRGenCallee getVirtualFunctionPointer(CIRGenFunction &cgf, GlobalDecl gd,
                                         Address thisAddr, mlir::Type ty,
                                         SourceLocation loc) override;

  void emitVTableDefinitions(CIRGenVTables &cgvt,
                             const CXXRecordDecl *rd) override;

  void emitVirtualInheritanceTables(const CXXRecordDecl *rd) override;

  mlir::Value
  getVirtualBaseClassOffset(mlir::Location loc, CIRGenFunction &cgf,
                            Address thisAddr, const CXXRecordDecl *classDecl,
                            const CXXRecordDecl *baseClassDecl) override;

  cir::MethodAttr buildVirtualMethodAttr(cir::MethodType methodTy,
                                         const CXXMethodDecl *md) override;

  mlir::Value performThisAdjustment(CIRGenFunction &cgf, Address thisAddr,
                                    const CXXRecordDecl *unadjustedClass,
                                    const ThunkInfo &ti) override;

  mlir::Value performReturnAdjustment(CIRGenFunction &cgf, Address ret,
                                      const CXXRecordDecl *unadjustedClass,
                                      const ReturnAdjustment &ra) override;

  bool shouldTypeidBeNullChecked(QualType srcTy) override { return false; }

  mlir::Value emitTypeid(CIRGenFunction &cgf, QualType srcTy, Address thisPtr,
                         mlir::Type typeInfoPtrTy) override {
    cgf.cgm.errorNYI("emitTypeid: MSVC ABI");
    return nullptr;
  }

  void emitBadTypeidCall(CIRGenFunction &cgf, mlir::Location loc) override {
    cgf.cgm.errorNYI(loc, "emitBadTypeidCall: MSVC ABI");
  }

  void emitBadCastCall(CIRGenFunction &cgf, mlir::Location loc) override {
    cgf.cgm.errorNYI(loc, "emitBadCastCall: MSVC ABI");
  }

  mlir::Value emitDynamicCast(CIRGenFunction &cgf, mlir::Location loc,
                              QualType srcRecordTy, QualType destRecordTy,
                              cir::PointerType destCIRTy, bool isRefCast,
                              Address src) override {
    cgf.cgm.errorNYI(loc, "emitDynamicCast: MSVC ABI");
    return cgf.getBuilder().getNullPtr(destCIRTy, loc);
  }

  mlir::Attribute getAddrOfRTTIDescriptor(mlir::Location loc,
                                          QualType ty) override;

  CatchTypeInfo
  getAddrOfCXXCatchHandlerType(mlir::Location loc, QualType ty,
                               QualType catchHandlerType) override {
    cgm.errorNYI(loc, "getAddrOfCXXCatchHandlerType: MSVC ABI");
    return {};
  }

  void emitRethrow(CIRGenFunction &cgf, bool isNoReturn) override {
    cgf.cgm.errorNYI("emitRethrow: MSVC ABI");
  }

  void emitThrow(CIRGenFunction &cgf, const CXXThrowExpr *e) override {
    cgf.cgm.errorNYI(e->getSourceRange(), "emitThrow: MSVC ABI");
  }

  void registerGlobalDtor(const VarDecl *vd, cir::FuncOp dtor,
                          mlir::Value addr) override;
};

} // namespace

Address CIRGenMicrosoftCXXABI::initializeArrayCookie(CIRGenFunction &cgf,
                                                     Address newPtr,
                                                     mlir::Value numElements,
                                                     const CXXNewExpr *e,
                                                     QualType elementType) {
  assert(requiresArrayCookie(e));
  CharUnits cookieSize = getArrayCookieSizeImpl(elementType);
  mlir::Location loc = cgf.getLoc(e->getSourceRange());

  mlir::Type u8Ty = cgf.getBuilder().getUInt8Ty();
  cir::PointerType u8PtrTy = cgf.getBuilder().getUInt8PtrTy();
  mlir::Value baseBytePtr =
      cgf.getBuilder().createBitcast(newPtr.getPointer(), u8PtrTy);

  // In the MSVC ABI, the cookie is located at offset 0 of newPtr.
  CharUnits baseAlignment = newPtr.getAlignment();
  Address cookiePtr(baseBytePtr, u8Ty, baseAlignment);
  Address numElementsPtr =
      cookiePtr.withElementType(cgf.getBuilder(), cgf.sizeTy);
  cgf.getBuilder().createStore(loc, numElements, numElementsPtr);

  // Skip over the cookie to return the pointer to the element array.
  mlir::Value dataOffset =
      cgf.getBuilder().getSInt32(cookieSize.getQuantity(), loc);
  mlir::Value dataPtr =
      cgf.getBuilder().createPtrStride(loc, baseBytePtr, dataOffset);
  mlir::Value finalPtr =
      cgf.getBuilder().createPtrBitcast(dataPtr, newPtr.getElementType());
  CharUnits finalAlignment = baseAlignment.alignmentAtOffset(cookieSize);
  return Address(finalPtr, newPtr.getElementType(), finalAlignment);
}

mlir::Value CIRGenMicrosoftCXXABI::readArrayCookieImpl(CIRGenFunction &cgf,
                                                       Address allocPtr,
                                                       CharUnits cookieSize) {
  Address numElementsPtr =
      allocPtr.withElementType(cgf.getBuilder(), cgf.sizeTy);
  return cgf.getBuilder().createLoad(cgf.getLoc(SourceLocation()),
                                     numElementsPtr);
}

CIRGenCXXABI::AddedStructorArgCounts
CIRGenMicrosoftCXXABI::buildStructorSignature(
    GlobalDecl gd, SmallVectorImpl<CanQualType> &argTys) {
  AddedStructorArgCounts added;
  if (isDeletingDtor(gd)) {
    argTys.push_back(cgm.getASTContext().IntTy);
    ++added.suffix;
  }
  auto *cd = dyn_cast<CXXConstructorDecl>(gd.getDecl());
  if (!cd)
    return added;

  const CXXRecordDecl *classDecl = cd->getParent();
  const FunctionProtoType *fpt = cd->getType()->castAs<FunctionProtoType>();
  if (classDecl->getNumVBases()) {
    if (fpt->isVariadic()) {
      argTys.insert(argTys.begin() + 1, cgm.getASTContext().IntTy);
      ++added.prefix;
    } else {
      argTys.push_back(cgm.getASTContext().IntTy);
      ++added.suffix;
    }
  }
  return added;
}

void CIRGenMicrosoftCXXABI::addImplicitStructorParams(CIRGenFunction &cgf,
                                                      QualType &resTy,
                                                      FunctionArgList &params) {
  ASTContext &context = cgm.getASTContext();
  const auto *md = cast<CXXMethodDecl>(cgf.curGD.getDecl());
  assert(isa<CXXConstructorDecl>(md) || isa<CXXDestructorDecl>(md));

  if (isa<CXXConstructorDecl>(md) && md->getParent()->getNumVBases()) {
    auto *isMostDerived = ImplicitParamDecl::Create(
        context, /*DC=*/nullptr, cgf.curGD.getDecl()->getLocation(),
        &context.Idents.get("is_most_derived"), context.IntTy,
        ImplicitParamKind::Other);
    const FunctionProtoType *fpt = md->getType()->castAs<FunctionProtoType>();
    if (fpt->isVariadic())
      params.insert(params.begin() + 1, isMostDerived);
    else
      params.push_back(isMostDerived);
    getStructorImplicitParamDecl(cgf) = isMostDerived;
  }

  if (isDeletingDtor(cgf.curGD)) {
    auto *shouldDelete = ImplicitParamDecl::Create(
        context, /*DC=*/nullptr, cgf.curGD.getDecl()->getLocation(),
        &context.Idents.get("should_call_delete"), context.IntTy,
        ImplicitParamKind::Other);
    params.push_back(shouldDelete);
    getStructorImplicitParamDecl(cgf) = shouldDelete;
  }
}

CharUnits
CIRGenMicrosoftCXXABI::getVirtualFunctionPrologueThisAdjustment(GlobalDecl gd) {
  const auto *md = cast<CXXMethodDecl>(gd.getDecl());

  if (const auto *dd = dyn_cast<CXXDestructorDecl>(md)) {
    if (gd.getDtorType() == Dtor_Complete)
      return CharUnits();

    gd = GlobalDecl(dd,
                    cgm.getASTContext().getTargetInfo().emitVectorDeletingDtors(
                        cgm.getASTContext().getLangOpts())
                        ? Dtor_VectorDeleting
                        : Dtor_Deleting);
  }

  MethodVFTableLocation ml =
      cgm.getMicrosoftVTableContext().getMethodVFTableLocation(gd);
  CharUnits adjustment = ml.VFPtrOffset;

  if (isa<CXXDestructorDecl>(md))
    adjustment = CharUnits::Zero();

  if (ml.VBase) {
    const ASTRecordLayout &derivedLayout =
        cgm.getASTContext().getASTRecordLayout(md->getParent());
    adjustment += derivedLayout.getVBaseClassOffset(ml.VBase);
  }

  return adjustment;
}

mlir::Value CIRGenMicrosoftCXXABI::getVBaseOffsetFromVBPtr(
    CIRGenFunction &cgf, mlir::Location loc, Address thisAddr,
    int64_t vbPtrOffset, unsigned vbTableIndex, mlir::Value *vbPtrOut) {
  CIRGenBuilderTy &builder = cgf.getBuilder();
  cir::PointerType u8PtrTy = builder.getUInt8PtrTy();
  mlir::Value thisBytePtr =
      builder.createBitcast(thisAddr.getPointer(), u8PtrTy);

  mlir::Value vbPtrOffsetVal = builder.getSInt32(vbPtrOffset, loc);
  mlir::Value vbPtr = cir::PtrStrideOp::create(builder, loc, u8PtrTy,
                                               thisBytePtr, vbPtrOffsetVal);
  if (vbPtrOut)
    *vbPtrOut = vbPtr;

  CharUnits vbPtrAlign = cgf.getPointerAlign();
  Address vbPtrAddr(builder.createBitcast(vbPtr, builder.getPointerTo(u8PtrTy)),
                    u8PtrTy, vbPtrAlign);
  mlir::Value vbTable = builder.createLoad(loc, vbPtrAddr);

  mlir::Type i32Ty = builder.getSInt32Ty();
  cir::PointerType i32PtrTy = builder.getPointerTo(i32Ty);
  mlir::Value vbTableI32Ptr = builder.createBitcast(vbTable, i32PtrTy);
  mlir::Value vbTableIndexVal = builder.getSInt32(vbTableIndex, loc);
  mlir::Value vbaseOffsPtr = cir::PtrStrideOp::create(
      builder, loc, i32PtrTy, vbTableI32Ptr, vbTableIndexVal);
  Address vbaseOffsAddr(vbaseOffsPtr, i32Ty, CharUnits::fromQuantity(4));
  return builder.createLoad(loc, vbaseOffsAddr);
}

void CIRGenMicrosoftCXXABI::emitInstanceFunctionProlog(SourceLocation loc,
                                                       CIRGenFunction &cgf) {
  if (cgf.curFuncDecl && cgf.curFuncDecl->hasAttr<NakedAttr>())
    return;

  mlir::Value thisVal = loadIncomingCXXThis(cgf);
  CIRGenBuilderTy &builder = cgf.getBuilder();
  const auto *md = cast<CXXMethodDecl>(cgf.curGD.getDecl());
  if (!cgf.curFuncIsThunk && md->isVirtual()) {
    CharUnits adjustment = getVirtualFunctionPrologueThisAdjustment(cgf.curGD);
    if (!adjustment.isZero()) {
      assert(adjustment.isPositive());
      mlir::Location mlirLoc = cgf.getLoc(loc);
      cir::PointerType u8PtrTy = builder.getUInt8PtrTy();
      mlir::Value u8This = builder.createBitcast(thisVal, u8PtrTy);
      mlir::Value negAdj =
          builder.getSInt32(-adjustment.getQuantity(), mlirLoc);
      mlir::Value adjusted =
          cir::PtrStrideOp::create(builder, mlirLoc, u8PtrTy, u8This, negAdj);
      thisVal = adjusted;
    }
  }
  cir::PointerType classPtrTy =
      builder.getPointerTo(cgf.convertType(md->getParent()));
  if (thisVal.getType() != classPtrTy)
    thisVal = builder.createBitcast(thisVal, classPtrTy);

  setCXXABIThisValue(cgf, thisVal);

  if (hasThisReturn(cgf.curGD) || hasMostDerivedReturn(cgf.curGD)) {
    if (cgf.returnValue.isValid())
      cgf.getBuilder().createStore(cgf.getLoc(loc), thisVal, cgf.returnValue);
  }

  if (isa<CXXConstructorDecl>(cgf.curGD.getDecl()) &&
      cast<CXXMethodDecl>(cgf.curGD.getDecl())->getParent()->getNumVBases()) {
    assert(getStructorImplicitParamDecl(cgf) &&
           "no implicit parameter for a constructor with virtual bases?");
    Address addr = cgf.getAddrOfLocalVar(getStructorImplicitParamDecl(cgf));
    setStructorImplicitParamValue(
        cgf, cgf.getBuilder().createLoad(cgf.getLoc(loc), addr));
  }

  if (isDeletingDtor(cgf.curGD)) {
    assert(getStructorImplicitParamDecl(cgf) &&
           "no implicit parameter for a deleting destructor?");
    Address addr = cgf.getAddrOfLocalVar(getStructorImplicitParamDecl(cgf));
    setStructorImplicitParamValue(
        cgf, cgf.getBuilder().createLoad(cgf.getLoc(loc), addr));
  }
}

CIRGenCXXABI::AddedStructorArgs
CIRGenMicrosoftCXXABI::getImplicitConstructorArgs(CIRGenFunction &cgf,
                                                  const CXXConstructorDecl *d,
                                                  CXXCtorType type,
                                                  bool forVirtualBase,
                                                  bool delegating) {
  assert(type == Ctor_Complete || type == Ctor_Base);

  if (!d->getParent()->getNumVBases())
    return AddedStructorArgs{};

  const FunctionProtoType *fpt = d->getType()->castAs<FunctionProtoType>();
  mlir::Value mostDerivedArg;
  CIRGenBuilderTy &builder = cgf.getBuilder();
  mlir::Location loc = cgf.getLoc(d->getLocation());
  if (delegating) {
    mostDerivedArg = getStructorImplicitParamValue(cgf);
  } else {
    mostDerivedArg = builder.getSInt32(type == Ctor_Complete ? 1 : 0, loc);
  }
  if (fpt->isVariadic()) {
    return AddedStructorArgs::withPrefix(
        {{mostDerivedArg, cgm.getASTContext().IntTy}});
  }
  return AddedStructorArgs::withSuffix(
      {{mostDerivedArg, cgm.getASTContext().IntTy}});
}

void CIRGenMicrosoftCXXABI::emitCXXDestructors(const CXXDestructorDecl *d) {
  cgm.emitGlobal(GlobalDecl(d, Dtor_Base));

  if (d->getParent()->getNumVBases() > 0 && d->hasAttr<DLLExportAttr>())
    cgm.emitGlobal(GlobalDecl(d, Dtor_Complete));
}

void CIRGenMicrosoftCXXABI::emitCXXStructor(GlobalDecl gd) {
  if (auto *ctor = dyn_cast<CXXConstructorDecl>(gd.getDecl())) {
    auto fn = cgm.codegenCXXStructor(gd.getWithCtorType(Ctor_Complete));
    cgm.maybeSetTrivialComdat(*ctor, fn);
    return;
  }

  auto *dtor = cast<CXXDestructorDecl>(gd.getDecl());
  if (gd.getDtorType() == Dtor_Complete &&
      dtor->getParent()->getNumVBases() == 0)
    gd = gd.getWithDtorType(Dtor_Base);

  if (gd.getDtorType() == Dtor_VectorDeleting) {
    if (!cgm.classNeedsVectorDestructor(dtor->getParent())) {
      GlobalDecl scalarDtorGD(dtor, Dtor_Deleting);
      auto aliasee = cast<cir::FuncOp>(cgm.getAddrOfGlobal(scalarDtorGD));
      StringRef mangledName = cgm.getMangledName(gd);
      auto entry = cast_or_null<cir::FuncOp>(cgm.getGlobalValue(mangledName));
      cgm.emitAliasForGlobal(mangledName, entry, gd, aliasee,
                             cir::GlobalLinkageKind::LinkOnceODRLinkage);
      return;
    }
  }

  auto fn = cgm.codegenCXXStructor(gd);
  if (cgm.supportsCOMDAT() && cir::isWeakForLinker(fn.getLinkage()))
    fn.setSelfComdat();
  else
    cgm.maybeSetTrivialComdat(*dtor, fn);
}

void CIRGenMicrosoftCXXABI::emitDestructorCall(
    CIRGenFunction &cgf, const CXXDestructorDecl *dd, CXXDtorType type,
    bool forVirtualBase, bool delegating, Address thisAddr, QualType thisTy) {
  if (type == Dtor_Complete && dd->getParent()->getNumVBases() == 0)
    type = Dtor_Base;

  GlobalDecl gd(dd, type);
  CIRGenCallee callee =
      CIRGenCallee::forDirect(cgm.getAddrOfCXXStructor(gd).getOperation(), gd);

  if (dd->isVirtual()) {
    assert(type != CXXDtorType::Dtor_Deleting &&
           "The deleting destructor should only be called via a virtual call");
    thisAddr = adjustThisArgumentForVirtualFunctionCall(
        cgf, GlobalDecl(dd, type), thisAddr, false);
  }

  cgf.emitCXXDestructorCall(gd, callee, thisAddr.emitRawPointer(), thisTy,
                            /*implicitParam=*/nullptr,
                            /*implicitParamTy=*/QualType(), /*e=*/nullptr);
}

mlir::Value CIRGenMicrosoftCXXABI::emitVirtualDestructorCall(
    CIRGenFunction &cgf, const CXXDestructorDecl *dtor, CXXDtorType dtorType,
    Address thisAddr, DeleteOrMemberCallExpr e) {
  auto *ce = dyn_cast<const CXXMemberCallExpr *>(e);
  auto *de = dyn_cast<const CXXDeleteExpr *>(e);
  assert((ce != nullptr) ^ (de != nullptr));
  assert(ce == nullptr || ce->arg_begin() == ce->arg_end());

  ASTContext &context = cgm.getASTContext();
  bool vectorDeletingDtorsEnabled =
      context.getTargetInfo().emitVectorDeletingDtors(context.getLangOpts());
  GlobalDecl gd(dtor, vectorDeletingDtorsEnabled ? Dtor_VectorDeleting
                                                 : Dtor_Deleting);
  const CIRGenFunctionInfo *fInfo =
      &cgm.getTypes().arrangeCXXStructorDeclaration(gd);
  cir::FuncType ty = cgm.getTypes().getFunctionType(*fInfo);
  CIRGenCallee callee = CIRGenCallee::forVirtual(ce, gd, thisAddr, ty);

  bool isDeleting = dtorType == Dtor_Deleting;
  bool isArrayDelete = de && de->isArrayForm() && vectorDeletingDtorsEnabled;
  bool isGlobalDelete = de && de->isGlobalDelete() &&
                        context.getTargetInfo().callGlobalDeleteInDeletingDtor(
                            context.getLangOpts());
  int32_t flags =
      (isDeleting ? 1 : 0) | (isGlobalDelete ? 4 : 0) | (isArrayDelete ? 2 : 0);
  mlir::Value implicitParam =
      cgf.getBuilder().getSInt32(flags, cgf.getLoc(dtor->getLocation()));

  QualType thisTy;
  if (ce)
    thisTy = ce->getObjectType();
  else
    thisTy = de->getDestroyedType();

  while (const ArrayType *aTy = context.getAsArrayType(thisTy))
    thisTy = aTy->getElementType();

  thisAddr = adjustThisArgumentForVirtualFunctionCall(cgf, gd, thisAddr, true);
  cgf.emitCXXDestructorCall(gd, callee, thisAddr.emitRawPointer(), thisTy,
                            implicitParam, context.IntTy, ce);
  return nullptr;
}

void CIRGenMicrosoftCXXABI::emitVirtualObjectDelete(
    CIRGenFunction &cgf, const CXXDeleteExpr *de, Address ptr,
    QualType elementType, const CXXDestructorDecl *dtor) {
  if (!cgm.getASTContext().getTargetInfo().callGlobalDeleteInDeletingDtor(
          cgm.getASTContext().getLangOpts())) {
    cgf.cgm.errorNYI(de->getSourceRange(),
                     "emitVirtualObjectDelete: legacy global delete without "
                     "deleting dtor flag");
    return;
  }

  if (de && de->isArrayForm()) {
    mlir::Value numElements = nullptr;
    mlir::Value allocatedPtr = nullptr;
    CharUnits cookieSize;
    readArrayCookie(cgf, ptr, elementType, numElements, allocatedPtr,
                    cookieSize);

    CIRGenBuilderTy &builder = cgf.getBuilder();
    mlir::Location loc = cgf.getLoc(de->getSourceRange());
    mlir::Value zero = builder.getConstInt(loc, numElements.getType(), 0);
    mlir::Value isEmpty =
        builder.createCompare(loc, cir::CmpOpKind::eq, numElements, zero);

    cir::IfOp::create(
        builder, loc, isEmpty, /*withElseRegion=*/true,
        /*thenBuilder=*/
        [&](mlir::OpBuilder &b, mlir::Location l) {
          cgf.emitDeleteCall(de->getOperatorDelete(), allocatedPtr, elementType,
                             numElements, cookieSize);
          builder.createYield(l);
        },
        /*elseBuilder=*/
        [&](mlir::OpBuilder &b, mlir::Location l) {
          emitVirtualDestructorCall(cgf, dtor, Dtor_Deleting, ptr, de);
          builder.createYield(l);
        });
    return;
  }

  emitVirtualDestructorCall(cgf, dtor, Dtor_Deleting, ptr, de);
}

void CIRGenMicrosoftCXXABI::emitConditionalArrayDtorCall(
    CIRGenFunction &cgf, const CXXDestructorDecl *dd,
    mlir::Value shouldDeleteCondition) {
  CIRGenBuilderTy &builder = cgf.getBuilder();
  mlir::Location loc = cgf.getLoc(dd->getLocation());
  Address thisPtr = cgf.loadCXXThisAddress();

  // Condition bit 2 (value 2) indicates an array delete.
  mlir::Value two = builder.getSInt32(2, loc);
  mlir::Value bit2 =
      cir::AndOp::create(builder, loc, shouldDeleteCondition, two);
  mlir::Value zero = builder.getSInt32(0, loc);
  mlir::Value shouldDestroyArray =
      builder.createCompare(loc, cir::CmpOpKind::ne, bit2, zero);

  cir::IfOp::create(
      builder, loc, shouldDestroyArray, /*withElseRegion=*/true,
      /*thenBuilder=*/
      [&](mlir::OpBuilder &b, mlir::Location l) {
        QualType eltTy = dd->getThisType()->getPointeeType();
        mlir::Value numElements = nullptr;
        mlir::Value allocatedPtr = nullptr;
        CharUnits cookieSize;
        readArrayCookie(cgf, thisPtr, eltTy, numElements, allocatedPtr,
                        cookieSize);

        QualType::DestructionKind dtorKind = eltTy.isDestructedType();
        assert(dtorKind);
        assert(numElements && "no element count for a type with a destructor!");

        CharUnits elementSize = cgf.getContext().getTypeSizeInChars(eltTy);
        CharUnits elementAlign =
            thisPtr.getAlignment().alignmentOfArrayElement(elementSize);

        cgf.emitArrayDestroy(thisPtr.getPointer(), numElements, eltTy,
                             elementAlign, cgf.getDestroyer(dtorKind));

        // Bit 1 (value 1) indicates whether to call operator delete[].
        mlir::Value one = builder.getSInt32(1, l);
        mlir::Value bit1 =
            cir::AndOp::create(builder, l, shouldDeleteCondition, one);
        mlir::Value shouldCallDelete =
            builder.createCompare(l, cir::CmpOpKind::ne, bit1, zero);

        cir::IfOp::create(
            builder, l, shouldCallDelete, /*withElseRegion=*/false,
            /*thenBuilder=*/
            [&](mlir::OpBuilder &b2, mlir::Location l2) {
              const CXXRecordDecl *classDecl = dd->getParent();
              if (const FunctionDecl *arrOD = dd->getArrayOperatorDelete()) {
                const FunctionDecl *globArrOD =
                    dd->getGlobalArrayOperatorDelete();
                if (globArrOD && isa<CXXMethodDecl>(arrOD)) {
                  mlir::Value four = builder.getSInt32(4, l2);
                  mlir::Value bit3 = cir::AndOp::create(
                      builder, l2, shouldDeleteCondition, four);
                  mlir::Value isGlobalDelete =
                      builder.createCompare(l2, cir::CmpOpKind::ne, bit3, zero);
                  cir::IfOp::create(
                      builder, l2, isGlobalDelete, /*withElseRegion=*/true,
                      /*thenBuilder=*/
                      [&](mlir::OpBuilder &b3, mlir::Location gloc) {
                        cir::FuncOp wrapper =
                            cgm.getOrCreateMSVCGlobalDeleteWrapper(globArrOD);
                        if (dd->hasAttr<DLLExportAttr>())
                          cgm.noteDirectGlobalDelete();
                        cgf.emitDeleteCall(
                            globArrOD, allocatedPtr,
                            cgf.getContext().getCanonicalTagType(classDecl),
                            numElements, cookieSize,
                            /*calleeOverride=*/wrapper);
                        builder.createYield(gloc);
                      },
                      /*elseBuilder=*/
                      [&](mlir::OpBuilder &b3, mlir::Location cloc) {
                        cgf.emitDeleteCall(
                            arrOD, allocatedPtr,
                            cgf.getContext().getCanonicalTagType(classDecl),
                            numElements, cookieSize);
                        builder.createYield(cloc);
                      });
                } else {
                  cgf.emitDeleteCall(
                      arrOD, allocatedPtr,
                      cgf.getContext().getCanonicalTagType(classDecl),
                      numElements, cookieSize);
                }
              } else {
                cgf.emitTrap(l2, true);
              }
              builder.createYield(l2);
            });

        builder.createYield(l);
      },
      /*elseBuilder=*/
      [&](mlir::OpBuilder &b, mlir::Location l) {
        {
          CIRGenFunction::RunCleanupsScope dtorEpilogue(cgf);
          cgf.enterDtorCleanups(dd, Dtor_Deleting);
          if (cgf.haveInsertPoint()) {
            QualType thisTy = dd->getFunctionObjectParameterType();
            cgf.emitCXXDestructorCall(dd, Dtor_Complete,
                                      /*forVirtualBase=*/false,
                                      /*delegating=*/false,
                                      cgf.loadCXXThisAddress(), thisTy);
          }
        }
        builder.createYield(l);
      });
}

void CIRGenMicrosoftCXXABI::registerGlobalDtor(const VarDecl *vd,
                                               cir::FuncOp dtor,
                                               mlir::Value addr) {
  if (vd->isNoDestroy(cgm.getASTContext()))
    return;

  if (cgm.getLangOpts().HLSL) {
    cgm.errorNYI(vd->getSourceRange(), "registerGlobalDtor: HLSL");
    return;
  }
}

static bool isTrivialForMSVC(const CXXRecordDecl *rd) {
  if (rd->hasProtectedFields() || rd->hasPrivateFields())
    return false;
  if (rd->getNumBases() > 0)
    return false;
  if (rd->isPolymorphic())
    return false;
  if (rd->hasNonTrivialCopyAssignment())
    return false;
  if (rd->needsImplicitCopyAssignment() && !rd->hasSimpleCopyAssignment())
    return false;
  for (const Decl *d : rd->decls()) {
    if (const auto *ctor = dyn_cast<CXXConstructorDecl>(d)) {
      if (ctor->isUserProvided())
        return false;
    } else if (const auto *tpl = dyn_cast<FunctionTemplateDecl>(d)) {
      if (isa<CXXConstructorDecl>(tpl->getTemplatedDecl()))
        return false;
    } else if (const auto *methodDecl = dyn_cast<CXXMethodDecl>(d)) {
      if (methodDecl->isCopyAssignmentOperator() && methodDecl->isDeleted())
        return false;
    }
  }
  if (rd->hasNonTrivialDestructor())
    return false;
  return true;
}

bool CIRGenMicrosoftCXXABI::classifyReturnType(CIRGenFunctionInfo &fi) const {
  const CXXRecordDecl *rd = fi.getReturnType()->getAsCXXRecordDecl();
  if (!rd)
    return false;

  bool isTrivialForABI = rd->canPassInRegisters() && isTrivialForMSVC(rd);
  bool isIndirectReturn = !isTrivialForABI || fi.isInstanceMethod();

  if (isIndirectReturn) {
    CharUnits align =
        cgm.getASTContext().getTypeAlignInChars(fi.getReturnType());
    auto returnInfo =
        cir::ABIArgInfo::getIndirect(align, /*addrSpace=*/0, /*byVal=*/false);
    returnInfo.setSRetAfterThis(fi.isInstanceMethod());
    fi.setReturnInfo(returnInfo);
    return true;
  }
  return false;
}

const CXXRecordDecl *
CIRGenMicrosoftCXXABI::getThisArgumentTypeForMethod(GlobalDecl gd) {
  auto *md = cast<CXXMethodDecl>(gd.getDecl());
  if (md->isVirtual()) {
    GlobalDecl lookupGD = gd;
    if (const auto *dd = dyn_cast<CXXDestructorDecl>(md)) {
      if (gd.getDtorType() == Dtor_Complete)
        return md->getParent();

      lookupGD = GlobalDecl(
          dd, cgm.getASTContext().getTargetInfo().emitVectorDeletingDtors(
                  cgm.getASTContext().getLangOpts())
                  ? Dtor_VectorDeleting
                  : Dtor_Deleting);
    }
    MethodVFTableLocation ml =
        cgm.getMicrosoftVTableContext().getMethodVFTableLocation(lookupGD);
    if (ml.VBase || !ml.VFPtrOffset.isZero())
      return nullptr;
  }
  return md->getParent();
}

mlir::Value CIRGenMicrosoftCXXABI::getVirtualBaseClassOffset(
    mlir::Location loc, CIRGenFunction &cgf, Address thisAddr,
    const CXXRecordDecl *classDecl, const CXXRecordDecl *baseClassDecl) {
  const ASTContext &context = cgm.getASTContext();
  CIRGenBuilderTy &builder = cgf.getBuilder();

  int64_t vbPtrChars =
      context.getASTRecordLayout(classDecl).getVBPtrOffset().getQuantity();
  unsigned vbIndex =
      cgm.getMicrosoftVTableContext().getVBTableIndex(classDecl, baseClassDecl);

  mlir::Value vbaseOffset =
      getVBaseOffsetFromVBPtr(cgf, loc, thisAddr, vbPtrChars, vbIndex);

  mlir::Type ptrDiffTy = cgm.ptrDiffTy;
  mlir::Value vbaseOffsetCast = builder.createIntCast(vbaseOffset, ptrDiffTy);
  mlir::Value vbPtrOffsetCast =
      builder.getConstantInt(loc, ptrDiffTy, vbPtrChars);
  return builder.createAdd(loc, vbPtrOffsetCast, vbaseOffsetCast);
}

Address CIRGenMicrosoftCXXABI::adjustThisArgumentForVirtualFunctionCall(
    CIRGenFunction &cgf, GlobalDecl gd, Address thisAddr, bool virtualCall) {
  if (!virtualCall) {
    CharUnits adjustment = getVirtualFunctionPrologueThisAdjustment(gd);
    if (adjustment.isZero())
      return thisAddr;

    CIRGenBuilderTy &builder = cgf.getBuilder();
    mlir::Location loc = cgf.getLoc(gd.getDecl()->getLocation());
    cir::PointerType u8PtrTy = builder.getUInt8PtrTy();
    mlir::Value u8This = builder.createBitcast(thisAddr.getPointer(), u8PtrTy);
    assert(adjustment.isPositive());
    mlir::Value adjConst = builder.getSInt32(adjustment.getQuantity(), loc);
    mlir::Value adjusted =
        cir::PtrStrideOp::create(builder, loc, u8PtrTy, u8This, adjConst);
    return Address(adjusted, builder.getUInt8Ty(), thisAddr.getAlignment())
        .withElementType(builder, thisAddr.getElementType());
  }

  const auto *md = cast<CXXMethodDecl>(gd.getDecl());
  GlobalDecl lookupGD = gd;
  if (const auto *dd = dyn_cast<CXXDestructorDecl>(md)) {
    if (gd.getDtorType() == Dtor_Complete)
      return thisAddr;

    lookupGD = GlobalDecl(
        dd, cgm.getASTContext().getTargetInfo().emitVectorDeletingDtors(
                cgm.getASTContext().getLangOpts())
                ? Dtor_VectorDeleting
                : Dtor_Deleting);
  }
  MethodVFTableLocation ml =
      cgm.getMicrosoftVTableContext().getMethodVFTableLocation(lookupGD);

  CharUnits staticOffset = ml.VFPtrOffset;
  if (isa<CXXDestructorDecl>(md) && gd.getDtorType() == Dtor_Base)
    staticOffset = CharUnits::Zero();

  Address result = thisAddr;
  CIRGenBuilderTy &builder = cgf.getBuilder();
  mlir::Location loc = cgf.getLoc(gd.getDecl()->getLocation());
  cir::PointerType u8PtrTy = builder.getUInt8PtrTy();

  if (ml.VBase) {
    const CXXRecordDecl *derived = md->getParent();
    const CXXRecordDecl *vbase = ml.VBase;
    mlir::Value vbaseOffset =
        getVirtualBaseClassOffset(loc, cgf, result, derived, vbase);
    mlir::Value rawPtr = builder.createBitcast(result.getPointer(), u8PtrTy);
    mlir::Value vbasePtr =
        cir::PtrStrideOp::create(builder, loc, u8PtrTy, rawPtr, vbaseOffset);
    CharUnits vbaseAlign =
        cgm.getVBaseAlignment(result.getAlignment(), derived, vbase);
    result = Address(vbasePtr, builder.getUInt8Ty(), vbaseAlign);
  }

  if (!staticOffset.isZero()) {
    assert(staticOffset.isPositive());
    mlir::Value rawPtr = builder.createBitcast(result.getPointer(), u8PtrTy);
    mlir::Value offsetVal = builder.getSInt32(staticOffset.getQuantity(), loc);
    mlir::Value adjustedPtr =
        cir::PtrStrideOp::create(builder, loc, u8PtrTy, rawPtr, offsetVal);
    result = Address(adjustedPtr, builder.getUInt8Ty(),
                     result.getAlignment().alignmentAtOffset(staticOffset));
  }

  if (result.getElementType() != thisAddr.getElementType())
    result = result.withElementType(builder, thisAddr.getElementType());

  return result;
}

cir::GlobalOp CIRGenMicrosoftCXXABI::getAddrOfVTable(const CXXRecordDecl *rd,
                                                     CharUnits vptrOffset) {
  VFTableIdTy id(rd, vptrOffset);
  auto [it, inserted] = vftablesMap.try_emplace(id);
  if (!inserted)
    return it->second;

  MicrosoftVTableContext &vtContext = cgm.getMicrosoftVTableContext();
  const VPtrInfoVector &vfptrs = vtContext.getVFPtrOffsets(rd);

  const std::unique_ptr<VPtrInfo> *vfptrI =
      llvm::find_if(vfptrs, [&](const std::unique_ptr<VPtrInfo> &vpi) {
        return vpi->FullOffsetInMDC == vptrOffset;
      });
  if (vfptrI == vfptrs.end()) {
    it->second = nullptr;
    return nullptr;
  }
  const std::unique_ptr<VPtrInfo> &vfptr = *vfptrI;

  if (deferredVFTables.insert(rd).second)
    cgm.addDeferredVTable(rd);

  SmallString<256> vftableName;
  {
    llvm::raw_svector_ostream out(vftableName);
    getMangleContext().mangleCXXVFTable(rd, vfptr->MangledPath, out);
  }

  if (auto gv = cgm.getGlobalValue(vftableName)) {
    auto globalOp = dyn_cast<cir::GlobalOp>(gv);
    it->second = globalOp;
    return globalOp;
  }

  cir::GlobalLinkageKind linkage =
      rd->hasAttr<DLLImportAttr>() ? cir::GlobalLinkageKind::LinkOnceODRLinkage
                                   : cgm.getVTableLinkage(rd);

  cir::GlobalOp vtable =
      cgm.createGlobalOp(cgm.getLoc(rd->getLocation()), vftableName,
                         cgm.getVTableComponentType(), /*isConstant=*/true);
  vtable.setLinkage(linkage);

  if (rd->hasAttr<DLLExportAttr>())
    vtable.setVisibility(mlir::SymbolTable::Visibility::Public);

  it->second = vtable;
  return vtable;
}

mlir::Value
CIRGenMicrosoftCXXABI::getVTableAddressPoint(BaseSubobject base,
                                             const CXXRecordDecl *vtableClass) {
  cir::GlobalOp vtable = getAddrOfVTable(vtableClass, base.getBaseOffset());
  if (!vtable)
    return nullptr;

  mlir::OpBuilder &builder = cgm.getBuilder();
  auto vtablePtrTy = cir::VPtrType::get(builder.getContext());

  return cir::VTableAddrPointOp::create(
      builder, cgm.getLoc(vtableClass->getSourceRange()), vtablePtrTy,
      mlir::FlatSymbolRefAttr::get(vtable.getSymNameAttr()),
      cir::AddressPointAttr::get(cgm.getBuilder().getContext(),
                                 /*vtableIndex=*/0,
                                 /*addressPointIndex=*/0));
}

cir::GlobalViewAttr CIRGenMicrosoftCXXABI::getVTableAddressPointAttr(
    BaseSubobject base, const CXXRecordDecl *vtableClass) {
  cir::GlobalOp vtable = getAddrOfVTable(vtableClass, base.getBaseOffset());
  if (!vtable)
    return {};

  CIRGenBuilderTy &builder = cgm.getBuilder();
  mlir::ArrayAttr indices = builder.getArrayAttr(
      {builder.getI32IntegerAttr(0), builder.getI32IntegerAttr(0)});
  return cir::GlobalViewAttr::get(
      cir::VPtrType::get(builder.getContext()),
      mlir::FlatSymbolRefAttr::get(vtable.getSymNameAttr()), indices,
      /*addressPoint=*/true);
}

mlir::Value CIRGenMicrosoftCXXABI::getVTableAddressPointInStructor(
    CIRGenFunction &cgf, const CXXRecordDecl *vtableClass, BaseSubobject base,
    const CXXRecordDecl *nearestVBase) {
  return getVTableAddressPoint(base, vtableClass);
}

cir::MethodAttr
CIRGenMicrosoftCXXABI::buildVirtualMethodAttr(cir::MethodType methodTy,
                                              const CXXMethodDecl *md) {
  assert(md->isVirtual() && "only deal with virtual member functions");

  MethodVFTableLocation ml =
      cgm.getMicrosoftVTableContext().getMethodVFTableLocation(md);
  const ASTContext &astContext = cgm.getASTContext();
  CharUnits pointerWidth = astContext.toCharUnitsFromBits(
      astContext.getTargetInfo().getPointerWidth(LangAS::Default));
  uint64_t vtableOffset = ml.Index * pointerWidth.getQuantity();

  return cir::MethodAttr::get(methodTy, vtableOffset);
}

CIRGenCallee CIRGenMicrosoftCXXABI::getVirtualFunctionPointer(
    CIRGenFunction &cgf, GlobalDecl gd, Address thisAddr, mlir::Type ty,
    SourceLocation loc) {
  CIRGenBuilderTy &builder = cgm.getBuilder();
  mlir::Location mlirLoc = cgf.getLoc(loc);
  cir::PointerType tyPtr = builder.getPointerTo(ty);
  auto *methodDecl = cast<CXXMethodDecl>(gd.getDecl());

  Address vptr = adjustThisArgumentForVirtualFunctionCall(cgf, gd, thisAddr,
                                                          /*virtualCall=*/true);
  mlir::Value vtable = cgf.getVTablePtr(mlirLoc, vptr, methodDecl->getParent());

  MicrosoftVTableContext &vftContext = cgm.getMicrosoftVTableContext();
  MethodVFTableLocation ml = vftContext.getMethodVFTableLocation(gd);

  mlir::Value vtableSlotPtr = cir::VTableGetVirtualFnAddrOp::create(
      builder, mlirLoc, builder.getPointerTo(tyPtr), vtable, ml.Index);
  mlir::Value vfunc = builder.createAlignedLoad(mlirLoc, tyPtr, vtableSlotPtr,
                                                cgf.getPointerAlign());

  return CIRGenCallee(gd, vfunc.getDefiningOp());
}

void CIRGenMicrosoftCXXABI::emitVTableDefinitions(CIRGenVTables &cgvt,
                                                  const CXXRecordDecl *rd) {
  MicrosoftVTableContext &vftContext = cgm.getMicrosoftVTableContext();
  const VPtrInfoVector &vfptrs = vftContext.getVFPtrOffsets(rd);

  for (const auto &info : vfptrs) {
    cir::GlobalOp vtable = getAddrOfVTable(rd, info->FullOffsetInMDC);
    if (!vtable || vtable.hasInitializer())
      continue;

    const VTableLayout &vtLayout =
        vftContext.getVFTableLayout(rd, info->FullOffsetInMDC);
    cir::GlobalLinkageKind linkage =
        rd->hasAttr<DLLImportAttr>()
            ? cir::GlobalLinkageKind::LinkOnceODRLinkage
            : cgm.getVTableLinkage(rd);

    mlir::Attribute rtti = nullptr;
    if (cgm.getASTContext().getLangOpts().RTTIData &&
        llvm::any_of(
            vtLayout.vtable_components(),
            [](const VTableComponent &vtc) { return vtc.isRTTIKind(); })) {
      rtti =
          getAddrOfRTTIDescriptor(cgm.getLoc(rd->getLocation()),
                                  cgm.getASTContext().getCanonicalTagType(rd));
    }

    cgvt.createVTableInitializer(vtable, vtLayout, rtti,
                                 cir::isLocalLinkage(linkage));
    vtable.setLinkage(linkage);
    if (cgm.supportsCOMDAT() && cir::isWeakForLinker(linkage))
      vtable.setSelfComdat();
    cgm.setGVProperties(vtable, rd);
  }
}

mlir::Attribute
CIRGenMicrosoftCXXABI::getAddrOfRTTIDescriptor(mlir::Location loc,
                                               QualType ty) {
  SmallString<256> mangledName;
  {
    llvm::raw_svector_ostream out(mangledName);
    getMangleContext().mangleCXXRTTI(ty, out);
  }

  CIRGenBuilderTy &builder = cgm.getBuilder();
  cir::PointerType voidPtrTy = builder.getVoidPtrTy();

  if (auto gv = cgm.getGlobalValue(mangledName))
    return builder.getGlobalViewAttr(voidPtrTy, cast<cir::GlobalOp>(gv));

  cir::GlobalOp gv =
      cgm.createGlobalOp(loc, mangledName, voidPtrTy, /*isConstant=*/true);
  gv.setLinkage(cir::GlobalLinkageKind::ExternalLinkage);
  return builder.getGlobalViewAttr(voidPtrTy, gv);
}

const CIRGenMicrosoftCXXABI::VBTableGlobals &
CIRGenMicrosoftCXXABI::enumerateVBTables(const CXXRecordDecl *rd) {
  auto [entry, added] = vbTablesMap.try_emplace(rd);
  VBTableGlobals &vbGlobals = entry->second;
  if (!added)
    return vbGlobals;

  MicrosoftVTableContext &context = cgm.getMicrosoftVTableContext();
  vbGlobals.VBTables = &context.enumerateVBTables(rd);

  cir::GlobalLinkageKind linkage =
      rd->hasAttr<DLLImportAttr>() ? cir::GlobalLinkageKind::LinkOnceODRLinkage
                                   : cgm.getVTableLinkage(rd);
  for (const auto &vbt : *vbGlobals.VBTables)
    vbGlobals.Globals.push_back(getAddrOfVBTable(*vbt, rd, linkage));

  return vbGlobals;
}

cir::GlobalOp
CIRGenMicrosoftCXXABI::getAddrOfVBTable(const VPtrInfo &vbt,
                                        const CXXRecordDecl *rd,
                                        cir::GlobalLinkageKind linkage) {
  SmallString<256> outName;
  llvm::raw_svector_ostream out(outName);
  getMangleContext().mangleCXXVBTable(rd, vbt.MangledPath, out);
  StringRef name = outName.str();

  if (auto gv = cgm.getGlobalValue(name))
    return cast<cir::GlobalOp>(gv);

  CIRGenBuilderTy &builder = cgm.getBuilder();
  cir::ArrayType vbTableTy =
      cir::ArrayType::get(&cgm.getMLIRContext(), builder.getSInt32Ty(),
                          1 + vbt.ObjectWithVPtr->getNumVBases());

  cir::GlobalOp gv = cgm.createGlobalOp(cgm.getLoc(rd->getLocation()), name,
                                        vbTableTy, /*isConstant=*/true);
  gv.setLinkage(linkage);
  CharUnits alignment =
      cgm.getASTContext().getTypeAlignInChars(cgm.getASTContext().IntTy);
  gv.setAlignment(alignment.getAsAlign().value());

  if (rd->hasAttr<DLLExportAttr>())
    gv.setVisibility(mlir::SymbolTable::Visibility::Public);

  if (!gv.hasExternalLinkage())
    emitVBTableDefinition(vbt, rd, gv);

  return gv;
}

void CIRGenMicrosoftCXXABI::emitVBTableDefinition(const VPtrInfo &vbt,
                                                  const CXXRecordDecl *rd,
                                                  cir::GlobalOp gv) const {
  const CXXRecordDecl *objectWithVPtr = vbt.ObjectWithVPtr;

  assert(rd->getNumVBases() && objectWithVPtr->getNumVBases() &&
         "should only emit vbtables for classes with vbtables");

  const ASTRecordLayout &baseLayout =
      cgm.getASTContext().getASTRecordLayout(vbt.IntroducingObject);
  const ASTRecordLayout &derivedLayout =
      cgm.getASTContext().getASTRecordLayout(rd);

  SmallVector<mlir::Attribute, 4> offsets(1 + objectWithVPtr->getNumVBases(),
                                          nullptr);

  CIRGenBuilderTy &builder = cgm.getBuilder();
  cir::IntType s32Ty = builder.getSInt32Ty();

  // The offset from ObjectWithVPtr's vbptr to itself always leads.
  CharUnits vbPtrOffset = baseLayout.getVBPtrOffset();
  offsets[0] = cir::IntAttr::get(s32Ty, -vbPtrOffset.getQuantity());

  MicrosoftVTableContext &context = cgm.getMicrosoftVTableContext();
  for (const auto &i : objectWithVPtr->vbases()) {
    const CXXRecordDecl *vbase = i.getType()->getAsCXXRecordDecl();
    CharUnits offset = derivedLayout.getVBaseClassOffset(vbase);
    assert(!offset.isNegative());

    // Make it relative to the subobject vbptr.
    CharUnits completeVBPtrOffset = vbt.NonVirtualOffset + vbPtrOffset;
    if (vbt.getVBaseWithVPtr())
      completeVBPtrOffset +=
          derivedLayout.getVBaseClassOffset(vbt.getVBaseWithVPtr());
    offset -= completeVBPtrOffset;

    unsigned vbIndex = context.getVBTableIndex(objectWithVPtr, vbase);
    assert(offsets[vbIndex] == nullptr && "The same vbindex seen twice?");
    offsets[vbIndex] = cir::IntAttr::get(s32Ty, offset.getQuantity());
  }

  auto vbTableTy = mlir::cast<cir::ArrayType>(gv.getSymType());
  assert(offsets.size() == vbTableTy.getSize());
  auto init = cir::ConstArrayAttr::get(
      vbTableTy, mlir::ArrayAttr::get(&cgm.getMLIRContext(), offsets));
  gv.setInitialValueAttr(init);

  if (rd->hasAttr<DLLImportAttr>())
    gv.setLinkage(cir::GlobalLinkageKind::AvailableExternallyLinkage);

  mlir::SymbolTable::setSymbolVisibility(gv,
                                         CIRGenModule::getMLIRVisibility(gv));

  if (cgm.supportsCOMDAT() && gv.isWeakForLinker())
    gv.setSelfComdat();
  cgm.setGVProperties(gv, rd);
}

void CIRGenMicrosoftCXXABI::emitVirtualInheritanceTables(
    const CXXRecordDecl *rd) {
  const VBTableGlobals &vbGlobals = enumerateVBTables(rd);
  for (unsigned i = 0, e = vbGlobals.VBTables->size(); i != e; ++i) {
    const std::unique_ptr<VPtrInfo> &vbt = (*vbGlobals.VBTables)[i];
    cir::GlobalOp gv = vbGlobals.Globals[i];
    if (gv.isDeclaration())
      emitVBTableDefinition(*vbt, rd, gv);
  }
}

void CIRGenMicrosoftCXXABI::emitVBPtrStores(CIRGenFunction &cgf,
                                            const CXXRecordDecl *rd) {
  Address thisAddr = cgf.loadCXXThisAddress();
  CIRGenBuilderTy &builder = cgf.getBuilder();
  cir::PointerType u8PtrTy = builder.getUInt8PtrTy();
  mlir::Value u8This = builder.createBitcast(thisAddr.getPointer(), u8PtrTy);

  const ASTContext &context = cgm.getASTContext();
  const ASTRecordLayout &layout = context.getASTRecordLayout(rd);

  const VBTableGlobals &vbGlobals = enumerateVBTables(rd);
  for (unsigned i = 0, e = vbGlobals.VBTables->size(); i != e; ++i) {
    const std::unique_ptr<VPtrInfo> &vbt = (*vbGlobals.VBTables)[i];
    cir::GlobalOp gv = vbGlobals.Globals[i];
    const ASTRecordLayout &subobjectLayout =
        context.getASTRecordLayout(vbt->IntroducingObject);
    CharUnits offs = vbt->NonVirtualOffset;
    offs += subobjectLayout.getVBPtrOffset();
    if (vbt->getVBaseWithVPtr())
      offs += layout.getVBaseClassOffset(vbt->getVBaseWithVPtr());

    mlir::Location loc = cgf.getLoc(rd->getLocation());
    mlir::Value offsVal = builder.getSInt32(offs.getQuantity(), loc);
    mlir::Value vbPtr =
        cir::PtrStrideOp::create(builder, loc, u8PtrTy, u8This, offsVal);

    cir::PointerType s32PtrTy = builder.getPointerTo(builder.getSInt32Ty());
    cir::PointerType vbTablePtrTy = builder.getPointerTo(gv.getSymType());
    mlir::Value gvAddr =
        cir::GetGlobalOp::create(builder, loc, vbTablePtrTy, gv.getSymName());
    mlir::Value gvDecayed =
        builder.createCast(cir::CastKind::array_to_ptrdecay, gvAddr, s32PtrTy);

    cir::PointerType vbPtrSlotTy = builder.getPointerTo(s32PtrTy);
    mlir::Value vbPtrSlot = builder.createBitcast(vbPtr, vbPtrSlotTy);
    Address vbPtrAddr(vbPtrSlot, s32PtrTy, cgf.getPointerAlign());
    builder.createStore(loc, gvDecayed, vbPtrAddr);
  }
}

std::vector<CharUnits>
CIRGenMicrosoftCXXABI::getVBPtrOffsets(const CXXRecordDecl *rd) {
  std::vector<CharUnits> vbPtrOffsets;
  const VBTableGlobals &vbGlobals = enumerateVBTables(rd);
  for (const std::unique_ptr<VPtrInfo> &vbt : *vbGlobals.VBTables) {
    const ASTRecordLayout &subobjectLayout =
        cgm.getASTContext().getASTRecordLayout(vbt->IntroducingObject);
    CharUnits offs = vbt->NonVirtualOffset;
    offs += subobjectLayout.getVBPtrOffset();
    if (vbt->getVBaseWithVPtr())
      offs += cgm.getASTContext().getASTRecordLayout(rd).getVBaseClassOffset(
          vbt->getVBaseWithVPtr());
    vbPtrOffsets.push_back(offs);
  }
  llvm::sort(vbPtrOffsets);
  return vbPtrOffsets;
}

mlir::Value CIRGenMicrosoftCXXABI::performThisAdjustment(
    CIRGenFunction &cgf, Address thisAddr, const CXXRecordDecl *unadjustedClass,
    const ThunkInfo &ti) {
  const ThisAdjustment &ta = ti.This;
  if (ta.isEmpty())
    return thisAddr.emitRawPointer();

  CIRGenBuilderTy &builder = cgf.getBuilder();
  mlir::Location loc = cgf.getLoc(
      unadjustedClass ? unadjustedClass->getLocation() : SourceLocation());
  cir::PointerType u8PtrTy = builder.getUInt8PtrTy();
  mlir::Value v = builder.createBitcast(thisAddr.getPointer(), u8PtrTy);

  if (!ta.Virtual.isEmpty()) {
    assert(ta.Virtual.Microsoft.VtordispOffset < 0);
    mlir::Value vtorDispOffset =
        builder.getSInt32(ta.Virtual.Microsoft.VtordispOffset, loc);
    mlir::Value vtorDispPtr =
        cir::PtrStrideOp::create(builder, loc, u8PtrTy, v, vtorDispOffset);
    mlir::Type i32Ty = builder.getSInt32Ty();
    mlir::Value vtorDispI32Ptr =
        builder.createBitcast(vtorDispPtr, builder.getPointerTo(i32Ty));
    mlir::Value vtorDisp = builder.createLoad(
        loc, Address(vtorDispI32Ptr, i32Ty, CharUnits::fromQuantity(4)));
    mlir::Value negVtorDisp = builder.createNeg(loc, vtorDisp);
    v = cir::PtrStrideOp::create(builder, loc, u8PtrTy, v, negVtorDisp);

    if (ta.Virtual.Microsoft.VBPtrOffset) {
      assert(ta.Virtual.Microsoft.VBPtrOffset > 0);
      assert(ta.Virtual.Microsoft.VBOffsetOffset >= 0);
      mlir::Value vbPtr;
      unsigned vbTableIndex = ta.Virtual.Microsoft.VBOffsetOffset / 4;
      mlir::Value vbaseOffset = getVBaseOffsetFromVBPtr(
          cgf, loc, Address(v, builder.getUInt8Ty(), cgf.getPointerAlign()),
          -ta.Virtual.Microsoft.VBPtrOffset, vbTableIndex, &vbPtr);
      v = cir::PtrStrideOp::create(builder, loc, u8PtrTy, vbPtr, vbaseOffset);
    }
  }

  if (ta.NonVirtual) {
    mlir::Value nonVirt = builder.getSInt32(ta.NonVirtual, loc);
    v = cir::PtrStrideOp::create(builder, loc, u8PtrTy, v, nonVirt);
  }

  return builder.createBitcast(v, thisAddr.getPointer().getType());
}

mlir::Value CIRGenMicrosoftCXXABI::performReturnAdjustment(
    CIRGenFunction &cgf, Address ret, const CXXRecordDecl *unadjustedClass,
    const ReturnAdjustment &ra) {
  if (ra.isEmpty())
    return ret.emitRawPointer();

  CIRGenBuilderTy &builder = cgf.getBuilder();
  mlir::Location loc = cgf.getLoc(
      unadjustedClass ? unadjustedClass->getLocation() : SourceLocation());
  cir::PointerType u8PtrTy = builder.getUInt8PtrTy();
  mlir::Value v = builder.createBitcast(ret.getPointer(), u8PtrTy);

  if (ra.Virtual.Microsoft.VBIndex) {
    assert(ra.Virtual.Microsoft.VBIndex > 0);
    mlir::Value vbPtr;
    mlir::Value vbaseOffset =
        getVBaseOffsetFromVBPtr(cgf, loc, ret, ra.Virtual.Microsoft.VBPtrOffset,
                                ra.Virtual.Microsoft.VBIndex, &vbPtr);
    v = cir::PtrStrideOp::create(builder, loc, u8PtrTy, vbPtr, vbaseOffset);
  }

  if (ra.NonVirtual) {
    mlir::Value nonVirt = builder.getSInt32(ra.NonVirtual, loc);
    v = cir::PtrStrideOp::create(builder, loc, u8PtrTy, v, nonVirt);
  }

  return builder.createBitcast(v, ret.getPointer().getType());
}

CIRGenCXXABI *clang::CIRGen::CreateCIRGenMicrosoftCXXABI(CIRGenModule &cgm) {
  return new CIRGenMicrosoftCXXABI(cgm);
}
