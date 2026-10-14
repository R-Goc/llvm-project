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
#include "clang/CIR/MissingFeatures.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/SaveAndRestore.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::CIRGen;

namespace {

bool isDeletingDtor(GlobalDecl gd) {
  return isa<CXXDestructorDecl>(gd.getDecl()) &&
         (gd.getDtorType() == Dtor_Deleting ||
          gd.getDtorType() == Dtor_VectorDeleting);
}

QualType decomposeTypeForEH(ASTContext &context, QualType t, bool &isConst,
                            bool &isVolatile, bool &isUnaligned) {
  t = context.getExceptionObjectType(t);
  isConst = false;
  isVolatile = false;
  isUnaligned = false;
  QualType pointeeType = t->getPointeeType();
  if (!pointeeType.isNull()) {
    isConst = pointeeType.isConstQualified();
    isVolatile = pointeeType.isVolatileQualified();
    isUnaligned = pointeeType.getQualifiers().hasUnaligned();
  }
  if (const auto *mpty = t->getAs<MemberPointerType>())
    t = context.getMemberPointerType(pointeeType.getUnqualifiedType(),
                                     mpty->getQualifier(),
                                     mpty->getMostRecentCXXRecordDecl());
  if (t->isPointerType())
    t = context.getPointerType(pointeeType.getUnqualifiedType());
  return t;
}

struct MSRTTIClass {
  enum {
    IsPrivateOnPath = 1 | 8,
    IsAmbiguous = 2,
    IsPrivate = 4,
    IsVirtual = 16,
    HasHierarchyDescriptor = 64
  };
  MSRTTIClass(const CXXRecordDecl *RD) : RD(RD) {}
  uint32_t initialize(const MSRTTIClass *Parent,
                      const CXXBaseSpecifier *Specifier);

  MSRTTIClass *getFirstChild() { return this + 1; }
  static MSRTTIClass *getNextChild(MSRTTIClass *Child) {
    return Child + 1 + Child->NumBases;
  }

  const CXXRecordDecl *RD, *VirtualRoot;
  uint32_t Flags, NumBases, OffsetInVBase;
};

uint32_t MSRTTIClass::initialize(const MSRTTIClass *Parent,
                                 const CXXBaseSpecifier *Specifier) {
  Flags = HasHierarchyDescriptor;
  if (!Parent) {
    VirtualRoot = nullptr;
    OffsetInVBase = 0;
  } else {
    if (Specifier->getAccessSpecifier() != AS_public)
      Flags |= IsPrivate | IsPrivateOnPath;
    if (Specifier->isVirtual()) {
      Flags |= IsVirtual;
      VirtualRoot = RD;
      OffsetInVBase = 0;
    } else {
      if (Parent->Flags & IsPrivateOnPath)
        Flags |= IsPrivateOnPath;
      VirtualRoot = Parent->VirtualRoot;
      OffsetInVBase =
          Parent->OffsetInVBase + RD->getASTContext()
                                      .getASTRecordLayout(Parent->RD)
                                      .getBaseClassOffset(RD)
                                      .getQuantity();
    }
  }
  NumBases = 0;
  MSRTTIClass *Child = getFirstChild();
  for (const CXXBaseSpecifier &Base : RD->bases()) {
    NumBases += Child->initialize(this, &Base) + 1;
    Child = getNextChild(Child);
  }
  return NumBases;
}

static void serializeClassHierarchy(SmallVectorImpl<MSRTTIClass> &Classes,
                                    const CXXRecordDecl *RD) {
  Classes.push_back(MSRTTIClass(RD));
  for (const CXXBaseSpecifier &Base : RD->bases())
    serializeClassHierarchy(Classes, Base.getType()->getAsCXXRecordDecl());
}

static void detectAmbiguousBases(SmallVectorImpl<MSRTTIClass> &Classes) {
  llvm::SmallPtrSet<const CXXRecordDecl *, 8> VirtualBases;
  llvm::SmallPtrSet<const CXXRecordDecl *, 8> UniqueBases;
  llvm::SmallPtrSet<const CXXRecordDecl *, 8> AmbiguousBases;
  for (MSRTTIClass *Class = &Classes.front(); Class <= &Classes.back();) {
    if ((Class->Flags & MSRTTIClass::IsVirtual) &&
        !VirtualBases.insert(Class->RD).second) {
      Class = MSRTTIClass::getNextChild(Class);
      continue;
    }
    if (!UniqueBases.insert(Class->RD).second)
      AmbiguousBases.insert(Class->RD);
    Class++;
  }
  if (AmbiguousBases.empty())
    return;
  for (MSRTTIClass &Class : Classes)
    if (AmbiguousBases.count(Class.RD))
      Class.Flags |= MSRTTIClass::IsAmbiguous;
}

static bool hasDefaultCXXMethodCC(ASTContext &Context,
                                  const CXXMethodDecl *MD) {
  CallingConv ExpectedCallingConv = Context.getDefaultCallingConvention(
      /*IsVariadic=*/false, /*IsCXXMethod=*/true);
  CallingConv ActualCallingConv =
      MD->getType()->castAs<FunctionProtoType>()->getCallConv();
  return ExpectedCallingConv == ActualCallingConv;
}

static cir::GlobalLinkageKind getLinkageForRTTI(QualType Ty) {
  switch (Ty->getLinkage()) {
  case Linkage::Invalid:
    llvm_unreachable("Linkage hasn't been computed!");

  case Linkage::None:
  case Linkage::Internal:
  case Linkage::UniqueExternal:
    return cir::GlobalLinkageKind::InternalLinkage;

  case Linkage::VisibleNone:
  case Linkage::Module:
  case Linkage::External:
    return cir::GlobalLinkageKind::LinkOnceODRLinkage;
  }
  llvm_unreachable("Invalid linkage!");
}

static void insertThrowAndSplit(mlir::OpBuilder &builder, mlir::Location loc,
                                mlir::Value exceptionPtr = {},
                                mlir::FlatSymbolRefAttr typeInfo = {},
                                mlir::FlatSymbolRefAttr dtor = {}) {
  mlir::Block *currentBlock = builder.getInsertionBlock();
  mlir::Region *region = currentBlock->getParent();

  if (currentBlock->empty()) {
    cir::ThrowOp::create(builder, loc, exceptionPtr, typeInfo, dtor);
    cir::UnreachableOp::create(builder, loc);
  } else {
    mlir::Block *throwBlock = builder.createBlock(region);

    cir::ThrowOp::create(builder, loc, exceptionPtr, typeInfo, dtor);
    cir::UnreachableOp::create(builder, loc);

    builder.setInsertionPointToEnd(currentBlock);
    cir::BrOp::create(builder, loc, throwBlock);
  }

  (void)builder.createBlock(region);
}

class CIRGenMicrosoftCXXABI : public CIRGenCXXABI {
  using VFTableIdTy = std::pair<const CXXRecordDecl *, CharUnits>;
  llvm::DenseMap<VFTableIdTy, cir::GlobalOp> vftablesMap;
  llvm::SmallPtrSet<const CXXRecordDecl *, 4> deferredVFTables;

  struct VBTableGlobals {
    const VPtrInfoVector *VBTables = nullptr;
    SmallVector<cir::GlobalOp, 2> Globals;
  };
  llvm::DenseMap<const CXXRecordDecl *, VBTableGlobals> vbTablesMap;
  llvm::DenseMap<const DeclContext *, unsigned> threadSafeGuardNumMap;
  cir::RecordType catchableTypeType;
  cir::RecordType throwInfoType;
  llvm::DenseMap<QualType, cir::GlobalOp> throwInfos;
  llvm::DenseMap<QualType, cir::GlobalOp> catchableTypeArrays;

  const VBTableGlobals &enumerateVBTables(const CXXRecordDecl *rd);
  cir::GlobalOp getAddrOfVBTable(const VPtrInfo &vbt, const CXXRecordDecl *rd,
                                 cir::GlobalLinkageKind linkage);
  void emitVBTableDefinition(const VPtrInfo &vbt, const CXXRecordDecl *rd,
                             cir::GlobalOp gv) const;

  MicrosoftMangleContext &getMangleContext() {
    return *cast<MicrosoftMangleContext>(mangleContext.get());
  }

  CharUnits getVirtualFunctionPrologueThisAdjustment(GlobalDecl gd);

  mlir::Value getVBaseOffsetFromVBPtr(CIRGenFunction &cgf, mlir::Location loc,
                                      Address thisAddr, int64_t vbPtrOffset,
                                      unsigned vbTableIndex,
                                      mlir::Value *vbPtrOut = nullptr);

public:
  CIRGenMicrosoftCXXABI(CIRGenModule &cgm) : CIRGenCXXABI(cgm) {}

  bool hasThisReturn(clang::GlobalDecl gd) const override {
    return isa<CXXConstructorDecl>(gd.getDecl());
  }

  bool hasMostDerivedReturn(clang::GlobalDecl gd) const override {
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

  void setCXXDestructorDLLStorage(cir::CIRGlobalValueInterface gv,
                                  const CXXDestructorDecl *dtor,
                                  CXXDtorType dt) const override {
    if (dt == Dtor_Deleting || dt == Dtor_VectorDeleting)
      gv.setDLLStorageClass(cir::DLLStorageClass::DefaultStorageClass);
    else
      cgm.setDLLImportDLLExport(gv, dtor);
  }

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
      if (dtor->hasAttr<DLLImportAttr>())
        return cir::GlobalLinkageKind::AvailableExternallyLinkage;
      return cir::GlobalLinkageKind::LinkOnceODRLinkage;
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

  std::vector<CharUnits> getVBPtrOffsets(const CXXRecordDecl *rd) override;

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

  CatchTypeInfo getCatchAllTypeInfo() override {
    if (cgm.getASTContext().getLangOpts().EHAsynch)
      return CatchTypeInfo{nullptr, 0};
    else
      return CatchTypeInfo{nullptr, 0x40};
  }

  void adjustCallArgsForDestructorThunk(CIRGenFunction &cgf, GlobalDecl gd,
                                        CallArgList &callArgs) override {
    assert((gd.getDtorType() == Dtor_VectorDeleting ||
            gd.getDtorType() == Dtor_Deleting) &&
           "Only vector deleting destructor thunks are available in this ABI");
    callArgs.add(RValue::get(getStructorImplicitParamValue(cgf)),
                 cgm.getASTContext().IntTy);
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

  void emitCXXConstructors(const CXXConstructorDecl *d) override;
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

  Address adjustThisArgumentForVirtualFunctionCall(CIRGenFunction &cgf,
                                                   GlobalDecl gd,
                                                   Address thisPtr,
                                                   bool virtualCall) override;

  bool isVirtualOffsetNeededForVTableField(CIRGenFunction &cgf,
                                           CIRGenFunction::VPtr vptr) override {
    return vptr.nearestVBase != nullptr;
  }

  cir::GlobalOp getAddrOfVTable(const CXXRecordDecl *rd,
                                CharUnits vptrOffset) override;

  mlir::Value getVTableAddressPoint(BaseSubobject base,
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
  void emitVBPtrStores(CIRGenFunction &cgf, const CXXRecordDecl *rd) override;

  void
  initializeHiddenVirtualInheritanceMembers(CIRGenFunction &cgf,
                                            const CXXRecordDecl *rd) override;

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

  std::tuple<Address, mlir::Value, const CXXRecordDecl *>
  performBaseAdjustment(CIRGenFunction &cgf, mlir::Location loc, Address value,
                        QualType srcRecordTy);
  mlir::Value getRTTIDescriptorPtr(CIRGenFunction &cgf, mlir::Location loc,
                                   QualType ty);

  bool shouldTypeidBeNullChecked(QualType srcTy) override;
  mlir::Value emitTypeid(CIRGenFunction &cgf, QualType srcTy, Address thisPtr,
                         mlir::Type typeInfoPtrTy) override;
  void emitBadTypeidCall(CIRGenFunction &cgf, mlir::Location loc) override;
  void emitBadCastCall(CIRGenFunction &cgf, mlir::Location loc) override;

  mlir::Value emitDynamicCast(CIRGenFunction &cgf, mlir::Location loc,
                              QualType srcRecordTy, QualType destRecordTy,
                              cir::PointerType destCIRTy, bool isRefCast,
                              Address src) override;

  mlir::Attribute getAddrOfRTTIDescriptor(mlir::Location loc,
                                          QualType ty) override;
  CatchTypeInfo
  getAddrOfCXXCatchHandlerType(mlir::Location loc, QualType ty,
                               QualType catchHandlerType) override;

  bool isImageRelative() const {
    return cgm.getASTContext().getTargetInfo().getPointerWidth(LangAS::Default) ==
           64;
  }
  cir::GlobalOp getImageBase(mlir::Location loc);
  mlir::Type getImageRelativeType(mlir::Type ptrTy);
  mlir::Attribute getImageRelativeAttr(mlir::Attribute ptrAttr,
                                       mlir::Location loc);

  cir::RecordType getCatchableTypeType();
  cir::RecordType getCatchableTypeArrayType(uint32_t numEntries);
  cir::RecordType getThrowInfoType();

  mlir::Attribute getCatchableType(mlir::Location loc, QualType T,
                                  uint32_t NVOffset = 0,
                                  int32_t VBPtrOffset = -1,
                                  uint32_t VBIndex = 0);
  cir::GlobalOp getCatchableTypeArray(mlir::Location loc, QualType T);
  cir::GlobalOp getThrowInfo(mlir::Location loc, QualType throwType) override;
  cir::FuncOp getAddrOfCXXCtorClosure(const CXXConstructorDecl *CD,
                                      CXXCtorType CT);

  void emitRethrow(CIRGenFunction &cgf, bool isNoReturn) override;
  void emitThrow(CIRGenFunction &cgf, const CXXThrowExpr *e) override;
  void registerGlobalDtor(const VarDecl *vd, cir::FuncOp dtor,
                          mlir::Value addr) override;
  void getStaticLocalGuardName(const VarDecl &varDecl,
                               SmallVectorImpl<char> &out) override;
};

} // namespace

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
  if (isa<CXXDestructorDecl>(gd.getDecl()) &&
      (gd.getDtorType() == Dtor_Deleting ||
       gd.getDtorType() == Dtor_VectorDeleting)) {
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
  } else if (isDeletingDtor(cgf.curGD)) {
    auto *shouldDelete = ImplicitParamDecl::Create(
        context, /*DC=*/nullptr, cgf.curGD.getDecl()->getLocation(),
        &context.Idents.get("should_call_delete"), context.IntTy,
        ImplicitParamKind::Other);
    params.push_back(shouldDelete);
    getStructorImplicitParamDecl(cgf) = shouldDelete;
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
    if (cgf.returnValue.isValid()) {
      mlir::Value retVal = thisVal;
      if (hasMostDerivedReturn(cgf.curGD))
        retVal = cgf.getBuilder().createBitcast(
            retVal, cgf.getBuilder().getVoidPtrTy());
      cgf.getBuilder().createStore(cgf.getLoc(loc), retVal, cgf.returnValue);
    }
  }

  if (isa<CXXConstructorDecl>(md) && md->getParent()->getNumVBases()) {
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

void CIRGenMicrosoftCXXABI::emitCXXConstructors(const CXXConstructorDecl *d) {
  cgm.emitGlobal(GlobalDecl(d, Ctor_Complete));

  if (d->hasAttr<DLLExportAttr>() && d->isDefaultConstructor()) {
    if (!hasDefaultCXXMethodCC(cgm.getASTContext(), d) || d->getNumParams() != 0) {
      getAddrOfCXXCtorClosure(d, Ctor_DefaultClosure);
    }
  }
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

  if (gd.getDtorType() == Dtor_Base && !cgm.tryEmitBaseDestructorAsAlias(dtor))
    return;

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
  auto it = vftablesMap.find(id);
  if (it != vftablesMap.end())
    return it->second;

  MicrosoftVTableContext &vtContext = cgm.getMicrosoftVTableContext();
  const VPtrInfoVector &vfptrs = vtContext.getVFPtrOffsets(rd);

  const std::unique_ptr<VPtrInfo> *vfptrI =
      llvm::find_if(vfptrs, [&](const std::unique_ptr<VPtrInfo> &vpi) {
        return vpi->FullOffsetInMDC == vptrOffset;
      });
  if (vfptrI == vfptrs.end()) {
    vftablesMap[id] = nullptr;
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
    vftablesMap[id] = globalOp;
    return globalOp;
  }

  cir::GlobalLinkageKind linkage =
      rd->hasAttr<DLLImportAttr>() ? cir::GlobalLinkageKind::LinkOnceODRLinkage
                                   : cgm.getVTableLinkage(rd);

  const VTableLayout &vtLayout =
      vtContext.getVFTableLayout(rd, vfptr->FullOffsetInMDC);
  mlir::Type vtableType = cgm.getVTables().getVTableType(vtLayout);

  cir::GlobalOp vtable =
      cgm.createGlobalOp(cgm.getLoc(rd->getLocation()), vftableName,
                         vtableType, /*isConstant=*/true);
  vtable.setLinkage(linkage);

  if (rd->hasAttr<DLLExportAttr>())
    vtable.setDLLStorageClass(cir::DLLStorageClass::DLLExportStorageClass);

  vftablesMap[id] = vtable;
  return vtable;
}

mlir::Value
CIRGenMicrosoftCXXABI::getVTableAddressPoint(BaseSubobject base,
                                             const CXXRecordDecl *vtableClass) {
  cir::GlobalOp vtable = getAddrOfVTable(vtableClass, base.getBaseOffset());
  if (!vtable)
    return nullptr;

  MicrosoftVTableContext &vtContext = cgm.getMicrosoftVTableContext();
  const VTableLayout &vtLayout =
      vtContext.getVFTableLayout(vtableClass, base.getBaseOffset());
  unsigned addressPointIndex =
      (cgm.getASTContext().getLangOpts().RTTIData &&
       llvm::any_of(vtLayout.vtable_components(),
                    [](const VTableComponent &vtc) { return vtc.isRTTIKind(); }))
          ? 1
          : 0;

  mlir::OpBuilder &builder = cgm.getBuilder();
  auto vtablePtrTy = cir::VPtrType::get(builder.getContext());

  return cir::VTableAddrPointOp::create(
      builder, cgm.getLoc(vtableClass->getSourceRange()), vtablePtrTy,
      mlir::FlatSymbolRefAttr::get(vtable.getSymNameAttr()),
      cir::AddressPointAttr::get(cgm.getBuilder().getContext(),
                                 /*vtableIndex=*/0,
                                 addressPointIndex));
}

mlir::Value CIRGenMicrosoftCXXABI::getVTableAddressPointInStructor(
    CIRGenFunction &cgf, const CXXRecordDecl *vtableClass, BaseSubobject base,
    const CXXRecordDecl *nearestVBase) {
  return getVTableAddressPoint(base, vtableClass);
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
    if (rd->hasAttr<DLLExportAttr>())
      vtable.setDLLStorageClass(cir::DLLStorageClass::DLLExportStorageClass);
    else
      vtable.setDLLStorageClass(cir::DLLStorageClass::DefaultStorageClass);
    cgm.setDSOLocal(static_cast<mlir::Operation *>(vtable));
  }
}

const CIRGenMicrosoftCXXABI::VBTableGlobals &
CIRGenMicrosoftCXXABI::enumerateVBTables(const CXXRecordDecl *rd) {
  auto [entry, added] = vbTablesMap.try_emplace(rd);
  VBTableGlobals &vbGlobals = entry->second;
  if (!added)
    return vbGlobals;

  MicrosoftVTableContext &context = cgm.getMicrosoftVTableContext();
  vbGlobals.VBTables = &context.enumerateVBTables(rd);

  cir::GlobalLinkageKind linkage = cgm.getVTableLinkage(rd);
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

  if (rd->hasAttr<DLLImportAttr>())
    gv.setDLLStorageClass(cir::DLLStorageClass::DLLImportStorageClass);
  else if (rd->hasAttr<DLLExportAttr>())
    gv.setDLLStorageClass(cir::DLLStorageClass::DLLExportStorageClass);

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
  const ASTContext &context = cgm.getASTContext();
  const ASTRecordLayout &layout = context.getASTRecordLayout(rd);

  const VBTableGlobals &vbGlobals = enumerateVBTables(rd);
  for (const std::unique_ptr<VPtrInfo> &vbt : *vbGlobals.VBTables) {
    const ASTRecordLayout &subobjectLayout =
        context.getASTRecordLayout(vbt->IntroducingObject);
    CharUnits offs = vbt->NonVirtualOffset;
    offs += subobjectLayout.getVBPtrOffset();
    if (vbt->getVBaseWithVPtr())
      offs += layout.getVBaseClassOffset(vbt->getVBaseWithVPtr());
    vbPtrOffsets.push_back(offs);
  }
  llvm::sort(vbPtrOffsets);
  return vbPtrOffsets;
}

void CIRGenMicrosoftCXXABI::initializeHiddenVirtualInheritanceMembers(
    CIRGenFunction &cgf, const CXXRecordDecl *rd) {
  const ASTRecordLayout &layout = cgm.getASTContext().getASTRecordLayout(rd);
  const ASTRecordLayout::VBaseOffsetsMapTy &vbaseMap =
      layout.getVBaseOffsetsMap();
  CIRGenBuilderTy &builder = cgf.getBuilder();
  mlir::Location loc = cgf.getLoc(rd->getLocation());

  cir::PointerType u8PtrTy = builder.getUInt8PtrTy();
  mlir::Value u8This = nullptr;

  for (const CXXBaseSpecifier &s : rd->vbases()) {
    const CXXRecordDecl *vbase = s.getType()->getAsCXXRecordDecl();
    auto it = vbaseMap.find(vbase);
    assert(it != vbaseMap.end());
    if (!it->second.hasVtorDisp())
      continue;

    mlir::Value vbaseOffset = getVirtualBaseClassOffset(
        loc, cgf, cgf.loadCXXThisAddress(), rd, vbase);
    int64_t constantVBaseOffset = it->second.VBaseOffset.getQuantity();

    mlir::Type ptrDiffTy = cgm.ptrDiffTy;
    mlir::Value constVBaseOffsetVal =
        builder.getConstantInt(loc, ptrDiffTy, constantVBaseOffset);
    mlir::Value vtorDispVal =
        builder.createSub(loc, vbaseOffset, constVBaseOffsetVal);
    mlir::Value vtorDispI32 =
        builder.createIntCast(vtorDispVal, builder.getSInt32Ty());

    if (!u8This)
      u8This =
          builder.createBitcast(cgf.loadCXXThisAddress().getPointer(), u8PtrTy);

    mlir::Value vbasePtr =
        cir::PtrStrideOp::create(builder, loc, u8PtrTy, u8This, vbaseOffset);
    mlir::Value minusFour = builder.getSInt32(-4, loc);
    mlir::Value vtorDispPtr =
        cir::PtrStrideOp::create(builder, loc, u8PtrTy, vbasePtr, minusFour);

    cir::PointerType s32PtrTy = builder.getPointerTo(builder.getSInt32Ty());
    mlir::Value vtorDispI32Ptr = builder.createBitcast(vtorDispPtr, s32PtrTy);
    Address vtorDispAddr(vtorDispI32Ptr, builder.getSInt32Ty(),
                         CharUnits::fromQuantity(4));
    builder.createStore(loc, vtorDispI32, vtorDispAddr);
  }
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

cir::MethodAttr
CIRGenMicrosoftCXXABI::buildVirtualMethodAttr(cir::MethodType methodTy,
                                              const CXXMethodDecl *md) {
  assert(md->isVirtual() && "only deal with virtual member functions");

  MethodVFTableLocation ml =
      cgm.getMicrosoftVTableContext().getMethodVFTableLocation(md);

  // Mangle the virtual member pointer thunk name (e.g. ??_9Method@@$BA@AE)
  SmallString<256> thunkName;
  llvm::raw_svector_ostream out(thunkName);
  getMangleContext().mangleVirtualMemPtrThunk(md, ml, out);

  auto thunkFuncOp = cgm.getModule().lookupSymbol<cir::FuncOp>(thunkName);
  if (!thunkFuncOp) {
    mlir::Location loc = cgm.getLoc(md->getLocation());
    const CIRGenFunctionInfo &thunkFnInfo =
        cgm.getTypes().arrangeUnprototypedMustTailThunk(md);
    cir::FuncType thunkTy = cgm.getTypes().getFunctionType(thunkFnInfo);

    thunkFuncOp = cgm.createCIRFunction(loc, thunkName, thunkTy, md);
    thunkFuncOp.setLinkage(md->isExternallyVisible()
                               ? cir::GlobalLinkageKind::LinkOnceODRLinkage
                               : cir::GlobalLinkageKind::InternalLinkage);
    thunkFuncOp.setVisibility(md->isExternallyVisible()
                                  ? mlir::SymbolTable::Visibility::Public
                                  : mlir::SymbolTable::Visibility::Private);
    if (md->isExternallyVisible() && cgm.supportsCOMDAT())
      thunkFuncOp.setSelfComdat();

    cgm.setGVProperties(thunkFuncOp, md);
    if (!exportThunk()) {
      thunkFuncOp.setDLLStorageClass(cir::DLLStorageClass::DefaultStorageClass);
      cgm.setDSOLocal(static_cast<mlir::Operation *>(thunkFuncOp));
    }

    cgm.setCIRFunctionAttributes(GlobalDecl(md), thunkFnInfo, thunkFuncOp,
                                 /*isThunk=*/false);
    cgm.setCIRFunctionAttributesForDefinition(md, thunkFuncOp);
    thunkFuncOp->setAttr("thunk",
                         mlir::UnitAttr::get(cgm.getBuilder().getContext()));

    mlir::OpBuilder::InsertionGuard guard(cgm.getBuilder());
    CIRGenFunction cgf(cgm, cgm.getBuilder());
    cgf.curFn = thunkFuncOp;
    cgf.curFnInfo = &thunkFnInfo;
    cgf.curGD = GlobalDecl(md);
    cgf.curFuncDecl = md;

    mlir::Block *entryBlock = thunkFuncOp.addEntryBlock();
    cgf.getBuilder().setInsertionPointToStart(entryBlock);
    mlir::Value thisVal = entryBlock->getArgument(0);

    auto voidPtrTy = cgf.getBuilder().getVoidPtrTy();
    auto voidPtrPtrTy = cgf.getBuilder().getPointerTo(voidPtrTy);
    mlir::Value thisVoidPtrPtr =
        cgf.getBuilder().createBitcast(loc, thisVal, voidPtrPtrTy);
    mlir::Value vtablePtr =
        cgf.getBuilder().createAlignedLoad(loc, voidPtrTy, thisVoidPtrPtr);

    mlir::Value slotOffset = cgf.getBuilder().getConstantInt(
        loc, cgf.getBuilder().getUInt64Ty(), ml.Index);
    mlir::Value vfnSlotPtr = cir::PtrStrideOp::create(
        cgf.getBuilder(), loc, voidPtrTy, vtablePtr, slotOffset);
    mlir::Value vfnSlotPtrPtr =
        cgf.getBuilder().createBitcast(loc, vfnSlotPtr, voidPtrPtrTy);
    mlir::Value calleeFnPtr =
        cgf.getBuilder().createAlignedLoad(loc, voidPtrTy, vfnSlotPtrPtr);

    auto fnPtrTy = cgf.getBuilder().getPointerTo(thunkTy);
    mlir::Value calleeTyped =
        cgf.getBuilder().createBitcast(loc, calleeFnPtr, fnPtrTy);

    cgf.emitMustTailThunk(GlobalDecl(md), thisVal, calleeTyped, thunkTy);
  }

  const CXXRecordDecl *rd = md->getParent()->getMostRecentDecl();
  CharUnits nvAdjustment = ml.VFPtrOffset;
  unsigned vbTableIndex = 0;
  if (ml.VBase)
    vbTableIndex =
        cgm.getMicrosoftVTableContext().getVBTableIndex(rd, ml.VBase) * 4;
  if (vbTableIndex == 0 &&
      rd->getMSInheritanceModel() == MSInheritanceModel::Virtual)
    nvAdjustment -= cgm.getASTContext().getOffsetOfBaseWithVBPtr(rd);
  CharUnits vbPtrOffset = CharUnits::Zero();
  if (vbTableIndex)
    vbPtrOffset = cgm.getASTContext().getASTRecordLayout(rd).getVBPtrOffset();

  return cgm.getBuilder().getMethodAttr(
      methodTy, thunkFuncOp, nvAdjustment.getQuantity(),
      vbPtrOffset.getQuantity(), vbTableIndex);
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

bool CIRGenMicrosoftCXXABI::shouldTypeidBeNullChecked(QualType srcTy) {
  const CXXRecordDecl *srcDecl = srcTy->getAsCXXRecordDecl();
  return !cgm.getASTContext().getASTRecordLayout(srcDecl).hasExtendableVFPtr();
}

std::tuple<Address, mlir::Value, const CXXRecordDecl *>
CIRGenMicrosoftCXXABI::performBaseAdjustment(CIRGenFunction &cgf,
                                             mlir::Location loc, Address value,
                                             QualType srcRecordTy) {
  CIRGenBuilderTy &builder = cgf.getBuilder();
  cir::PointerType u8PtrTy = builder.getUInt8PtrTy();
  mlir::Type u8Ty = builder.getUInt8Ty();
  mlir::Type ptrDiffTy = cgm.ptrDiffTy;

  Address byteVal = Address(builder.createBitcast(value.getPointer(), u8PtrTy),
                            u8Ty, value.getAlignment());
  const CXXRecordDecl *srcDecl = srcRecordTy->getAsCXXRecordDecl();
  const ASTContext &context = cgm.getASTContext();

  if (context.getASTRecordLayout(srcDecl).hasExtendableVFPtr())
    return std::make_tuple(byteVal, builder.getConstantInt(loc, ptrDiffTy, 0),
                           srcDecl);

  const CXXRecordDecl *polymorphicBase = nullptr;
  for (const auto &base : srcDecl->vbases()) {
    const CXXRecordDecl *baseDecl = base.getType()->getAsCXXRecordDecl();
    if (context.getASTRecordLayout(baseDecl).hasExtendableVFPtr()) {
      polymorphicBase = baseDecl;
      break;
    }
  }
  assert(polymorphicBase && "polymorphic class has no apparent vfptr?");

  mlir::Value offset =
      getVirtualBaseClassOffset(loc, cgf, value, srcDecl, polymorphicBase);
  mlir::Value adjusted = cir::PtrStrideOp::create(builder, loc, u8PtrTy,
                                                  byteVal.getPointer(), offset);
  CharUnits vbaseAlign =
      cgm.getVBaseAlignment(value.getAlignment(), srcDecl, polymorphicBase);
  return std::make_tuple(Address(adjusted, u8Ty, vbaseAlign), offset,
                         polymorphicBase);
}

static mlir::Value emitRTtypeidCall(CIRGenFunction &cgf, mlir::Location loc,
                                    mlir::Value argument,
                                    mlir::NamedAttrList attrs = {}) {
  CIRGenBuilderTy &builder = cgf.getBuilder();
  cir::FuncType fnTy =
      builder.getFuncType({builder.getVoidPtrTy()}, builder.getVoidPtrTy());
  cir::FuncOp fn = cgf.cgm.createRuntimeFunction(fnTy, "__RTtypeid", attrs);
  return cgf.emitRuntimeCall(loc, fn, {argument}, attrs);
}

void CIRGenMicrosoftCXXABI::emitBadTypeidCall(CIRGenFunction &cgf,
                                              mlir::Location loc) {
  mlir::NamedAttrList attrs;
  attrs.set(cir::CIRDialect::getNoReturnAttrName(),
            mlir::UnitAttr::get(&cgf.cgm.getMLIRContext()));
  mlir::Value nullVal =
      cgf.getBuilder().getNullPtr(cgf.getBuilder().getVoidPtrTy(), loc);
  emitRTtypeidCall(cgf, loc, nullVal, attrs);
  cir::UnreachableOp::create(cgf.getBuilder(), loc);
}

mlir::Value CIRGenMicrosoftCXXABI::emitTypeid(CIRGenFunction &cgf,
                                              QualType srcTy, Address thisPtr,
                                              mlir::Type typeInfoPtrTy) {
  const CXXRecordDecl *srcDecl = srcTy->getAsCXXRecordDecl();
  mlir::Location loc = cgf.getLoc(srcDecl->getLocation());
  std::tie(thisPtr, std::ignore, std::ignore) =
      performBaseAdjustment(cgf, loc, thisPtr, srcTy);
  CIRGenBuilderTy &builder = cgf.getBuilder();
  mlir::Value rawPtr =
      builder.createBitcast(thisPtr.getPointer(), builder.getVoidPtrTy());
  mlir::Value typeidCall = emitRTtypeidCall(cgf, loc, rawPtr);
  return builder.createBitcast(typeidCall, typeInfoPtrTy);
}

void CIRGenMicrosoftCXXABI::emitBadCastCall(CIRGenFunction &cgf,
                                            mlir::Location loc) {
  cgm.errorNYI(loc, "emitBadCastCall: MSVC ABI");
}

mlir::Value CIRGenMicrosoftCXXABI::getRTTIDescriptorPtr(CIRGenFunction &cgf,
                                                        mlir::Location loc,
                                                        QualType ty) {
  CIRGenBuilderTy &builder = cgf.getBuilder();
  auto rttiAttr = cast<cir::GlobalViewAttr>(getAddrOfRTTIDescriptor(loc, ty));
  auto gv =
      cast<cir::GlobalOp>(cgm.getGlobalValue(rttiAttr.getSymbol().getValue()));
  mlir::Value gvAddr = cir::GetGlobalOp::create(
      builder, loc, builder.getPointerTo(gv.getSymType()), gv.getSymName());
  return builder.createBitcast(gvAddr, builder.getVoidPtrTy());
}

mlir::Value CIRGenMicrosoftCXXABI::emitDynamicCast(
    CIRGenFunction &cgf, mlir::Location loc, QualType srcRecordTy,
    QualType destRecordTy, cir::PointerType destCIRTy, bool isRefCast,
    Address src) {
  CIRGenBuilderTy &builder = cgf.getBuilder();
  bool isCastToVoid = destRecordTy.isNull();

  auto emitCastToVoid = [&]() -> mlir::Value {
    Address value = src;
    std::tie(value, std::ignore, std::ignore) =
        performBaseAdjustment(cgf, loc, value, srcRecordTy);
    cir::FuncType fnTy =
        builder.getFuncType({builder.getVoidPtrTy()}, builder.getVoidPtrTy());
    cir::FuncOp fn = cgm.createRuntimeFunction(fnTy, "__RTCastToVoid");
    mlir::Value rawPtr =
        builder.createBitcast(value.getPointer(), builder.getVoidPtrTy());
    mlir::Value res = cgf.emitRuntimeCall(loc, fn, {rawPtr});
    return builder.createBitcast(res, destCIRTy);
  };

  auto emitDynCast = [&]() -> mlir::Value {
    Address value = src;
    mlir::Value offset;
    std::tie(value, offset, std::ignore) =
        performBaseAdjustment(cgf, loc, value, srcRecordTy);
    mlir::Value rawPtr =
        builder.createBitcast(value.getPointer(), builder.getVoidPtrTy());
    mlir::Value vfDelta = builder.createIntCast(offset, builder.getSInt32Ty());

    mlir::Value srcRtti =
        getRTTIDescriptorPtr(cgf, loc, srcRecordTy.getUnqualifiedType());
    mlir::Value destRtti =
        getRTTIDescriptorPtr(cgf, loc, destRecordTy.getUnqualifiedType());

    mlir::Value isRefVal =
        builder.getConstantInt(loc, builder.getSInt32Ty(), isRefCast ? 1 : 0);

    cir::FuncType fnTy = builder.getFuncType(
        {builder.getVoidPtrTy(), builder.getSInt32Ty(), builder.getVoidPtrTy(),
         builder.getVoidPtrTy(), builder.getSInt32Ty()},
        builder.getVoidPtrTy());
    cir::FuncOp fn = cgm.createRuntimeFunction(fnTy, "__RTDynamicCast");
    mlir::Value res = cgf.emitRuntimeCall(
        loc, fn, {rawPtr, vfDelta, srcRtti, destRtti, isRefVal});
    return builder.createBitcast(res, destCIRTy);
  };

  auto doCast = [&]() -> mlir::Value {
    if (isCastToVoid)
      return emitCastToVoid();
    return emitDynCast();
  };

  bool shouldNullCheck =
      !isRefCast && !cgm.getASTContext()
                         .getASTRecordLayout(srcRecordTy->getAsCXXRecordDecl())
                         .hasExtendableVFPtr();

  if (shouldNullCheck) {
    mlir::Value isNull = builder.createPtrIsNull(src.getPointer());
    return cir::TernaryOp::create(
               builder, loc, isNull,
               [&](mlir::OpBuilder &, mlir::Location) {
                 mlir::Value nullVal = builder.getNullPtr(destCIRTy, loc);
                 builder.createYield(loc, nullVal);
               },
               [&](mlir::OpBuilder &, mlir::Location) {
                 mlir::Value castVal = doCast();
                 builder.createYield(loc, castVal);
               })
        .getResult();
  }

  return doCast();
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

CatchTypeInfo CIRGenMicrosoftCXXABI::getAddrOfCXXCatchHandlerType(
    mlir::Location loc, QualType ty, QualType catchHandlerType) {
  bool isConst, isVolatile, isUnaligned;
  ty = decomposeTypeForEH(cgm.getASTContext(), ty, isConst, isVolatile,
                          isUnaligned);

  bool isReference = catchHandlerType->isReferenceType();

  uint32_t flags = 0;
  if (isConst)
    flags |= 1;
  if (isVolatile)
    flags |= 2;
  if (isUnaligned)
    flags |= 4;
  if (isReference)
    flags |= 8;

  auto rtti = dyn_cast<cir::GlobalViewAttr>(getAddrOfRTTIDescriptor(loc, ty));
  assert(rtti && "expected GlobalViewAttr");
  return CatchTypeInfo{rtti, flags};
}

cir::GlobalOp CIRGenMicrosoftCXXABI::getImageBase(mlir::Location loc) {
  if (auto gv = cgm.getGlobalValue("__ImageBase"))
    return cast<cir::GlobalOp>(gv);
  CIRGenBuilderTy &builder = cgm.getBuilder();
  cir::GlobalOp gv = cgm.createGlobalOp(loc, "__ImageBase",
                                        builder.getUInt8Ty(),
                                        /*isConstant=*/true);
  gv.setLinkage(cir::GlobalLinkageKind::ExternalLinkage);
  return gv;
}

mlir::Type CIRGenMicrosoftCXXABI::getImageRelativeType(mlir::Type ptrTy) {
  if (!isImageRelative())
    return ptrTy;
  return cgm.getBuilder().getSInt32Ty();
}

mlir::Attribute
CIRGenMicrosoftCXXABI::getImageRelativeAttr(mlir::Attribute ptrAttr,
                                            mlir::Location loc) {
  CIRGenBuilderTy &builder = cgm.getBuilder();
  cir::PointerType voidPtrTy = builder.getVoidPtrTy();

  bool isNull = !ptrAttr;
  if (!isNull) {
    if (auto constPtr = mlir::dyn_cast<cir::ConstPtrAttr>(ptrAttr))
      if (constPtr.isNullValue())
        isNull = true;
  }

  if (!isImageRelative()) {
    if (isNull)
      return builder.getConstNullPtrAttr(voidPtrTy);
    if (auto sym = mlir::dyn_cast<mlir::FlatSymbolRefAttr>(ptrAttr))
      return cir::GlobalViewAttr::get(voidPtrTy, sym);
    return ptrAttr;
  }

  cir::IntType s32Ty = builder.getSInt32Ty();
  if (isNull)
    return cir::IntAttr::get(s32Ty, 0);

  getImageBase(loc);

  mlir::FlatSymbolRefAttr sym;
  if (auto gvAttr = mlir::dyn_cast<cir::GlobalViewAttr>(ptrAttr)) {
    sym = gvAttr.getSymbol();
  } else if (auto symAttr = mlir::dyn_cast<mlir::FlatSymbolRefAttr>(ptrAttr)) {
    sym = symAttr;
  } else {
    llvm_unreachable("unexpected attribute for image-relative reference");
  }

  return cir::ImageRelativeAttr::get(s32Ty, sym);
}

cir::RecordType CIRGenMicrosoftCXXABI::getCatchableTypeType() {
  if (catchableTypeType)
    return catchableTypeType;
  CIRGenBuilderTy &builder = cgm.getBuilder();
  cir::IntType s32Ty = builder.getSInt32Ty();
  mlir::Type imgRelVoidPtrTy = getImageRelativeType(builder.getVoidPtrTy());
  mlir::Type fieldTypes[] = {
      s32Ty,           // Flags
      imgRelVoidPtrTy, // TypeDescriptor
      s32Ty,           // NonVirtualAdjustment
      s32Ty,           // OffsetToVBPtr
      s32Ty,           // VBTableIndex
      s32Ty,           // Size
      imgRelVoidPtrTy  // CopyCtor
  };
  catchableTypeType = builder.getCompleteNamedRecordType(
      fieldTypes, /*packed=*/false, "eh.CatchableType",
      cir::RecordType::getAllDataKinds(fieldTypes));
  return catchableTypeType;
}

cir::RecordType
CIRGenMicrosoftCXXABI::getCatchableTypeArrayType(uint32_t numEntries) {
  CIRGenBuilderTy &builder = cgm.getBuilder();
  cir::IntType s32Ty = builder.getSInt32Ty();
  mlir::Type ctType = getImageRelativeType(builder.getVoidPtrTy());
  cir::ArrayType at = cir::ArrayType::get(ctType, numEntries);
  mlir::Type fieldTypes[] = {
      s32Ty, // NumEntries
      at     // CatchableTypes
  };
  std::string name = ("eh.CatchableTypeArray." + Twine(numEntries)).str();
  return builder.getCompleteNamedRecordType(
      fieldTypes, /*packed=*/false, name,
      cir::RecordType::getAllDataKinds(fieldTypes));
}

cir::RecordType CIRGenMicrosoftCXXABI::getThrowInfoType() {
  if (throwInfoType)
    return throwInfoType;
  CIRGenBuilderTy &builder = cgm.getBuilder();
  cir::IntType s32Ty = builder.getSInt32Ty();
  mlir::Type imgRelVoidPtrTy = getImageRelativeType(builder.getVoidPtrTy());
  mlir::Type fieldTypes[] = {
      s32Ty,           // Flags
      imgRelVoidPtrTy, // CleanupFn
      imgRelVoidPtrTy, // ForwardCompat
      imgRelVoidPtrTy  // CatchableTypeArray
  };
  throwInfoType = builder.getCompleteNamedRecordType(
      fieldTypes, /*packed=*/false, "eh.ThrowInfo",
      cir::RecordType::getAllDataKinds(fieldTypes));
  return throwInfoType;
}

cir::FuncOp
CIRGenMicrosoftCXXABI::getAddrOfCXXCtorClosure(const CXXConstructorDecl *cd,
                                              CXXCtorType ct) {
  assert(ct == Ctor_CopyingClosure || ct == Ctor_DefaultClosure);

  SmallString<256> thunkName;
  {
    llvm::raw_svector_ostream out(thunkName);
    getMangleContext().mangleName(GlobalDecl(cd, ct), out);
  }

  if (auto gv = cgm.getGlobalValue(thunkName))
    return cast<cir::FuncOp>(gv);

  const CIRGenFunctionInfo &fnInfo = cgm.getTypes().arrangeMSCtorClosure(cd, ct);
  cir::FuncType thunkTy = cgm.getTypes().getFunctionType(fnInfo);
  const CXXRecordDecl *rd = cd->getParent();
  CanQualType recordTy = cgm.getASTContext().getCanonicalTagType(rd);
  cir::GlobalLinkageKind linkage = getLinkageForRTTI(recordTy);

  mlir::Location loc = cgm.getLoc(cd->getLocation());
  cir::FuncOp thunkFn =
      cgm.createCIRFunction(loc, thunkName, thunkTy, nullptr);
  thunkFn.setCallingConv(fnInfo.getCallingConvention());
  thunkFn.setLinkage(linkage);
  if (cgm.supportsCOMDAT() && cir::isWeakForLinker(linkage))
    thunkFn.setSelfComdat();

  bool isCopy = ct == Ctor_CopyingClosure;

  CIRGenBuilderTy builder(cgm.getMLIRContext(), cgm);
  CIRGenFunction cgf(cgm, builder);
  cgf.curGD = GlobalDecl(cd, Ctor_Complete);
  cgf.curFuncIsThunk = true;

  FunctionArgList functionArgs;
  buildThisParam(cgf, functionArgs);

  auto *srcParam = ImplicitParamDecl::Create(
      cgm.getASTContext(), /*DC=*/nullptr, SourceLocation(),
      &cgm.getASTContext().Idents.get("src"),
      cgm.getASTContext().getLValueReferenceType(recordTy,
                                                 /*SpelledAsLValue=*/true),
      ImplicitParamKind::Other);
  if (isCopy)
    functionArgs.push_back(srcParam);

  auto *isMostDerived =
      ImplicitParamDecl::Create(cgm.getASTContext(), /*DC=*/nullptr,
                                SourceLocation(),
                                &cgm.getASTContext().Idents.get("is_most_derived"),
                                cgm.getASTContext().IntTy,
                                ImplicitParamKind::Other);
  if (rd->getNumVBases() > 0)
    functionArgs.push_back(isMostDerived);

  mlir::Block *entryBb = thunkFn.addEntryBlock();
  builder.setInsertionPointToStart(entryBb);

  CIRGenFunction::SymTableScopeTy varScope(cgf.symbolTable);
  CIRGenFunction::SourceLocRAIIObject locRAII(cgf, cd->getSourceRange());
  CIRGenFunction::LexicalScope lexScope{cgf, thunkFn.getLoc(), entryBb};

  cgf.curFnInfo = &fnInfo;
  cgf.startFunction(GlobalDecl(), cgm.getASTContext().VoidTy, thunkFn, thunkTy,
                    functionArgs, cd->getLocation(), SourceLocation());
  setCXXABIThisValue(cgf, loadIncomingCXXThis(cgf));
  cgf.cxxThisValue = cgf.cxxabiThisValue;
  mlir::Value thisVal = cgf.loadCXXThis();

  mlir::Value srcVal =
      isCopy ? cgf.getBuilder().createLoad(
                   loc, cgf.getAddrOfLocalVar(srcParam))
             : mlir::Value();

  CallArgList args;
  args.add(RValue::get(thisVal), cd->getThisType());
  if (srcVal)
    args.add(RValue::get(srcVal), srcParam->getType());

  SmallVector<const Stmt *, 4> argVec;
  for (const CXXDefaultArgExpr *expr :
       cd->getCtorClosureDefaultArgs().drop_front(isCopy ? 1 : 0))
    argVec.push_back(expr);
  assert(argVec.size() == cd->getNumParams() - isCopy);

  CIRGenFunction::RunCleanupsScope cleanups(cgf);

  const auto *fpt = cd->getType()->castAs<FunctionProtoType>();
  cgf.emitCallArgs(args, fpt,
                   llvm::iterator_range<CallExpr::const_arg_iterator>(
                       argVec.begin(), argVec.end()),
                   cd, isCopy ? 1 : 0);

  AddedStructorArgCounts extraArgs =
      addImplicitConstructorArgs(cgf, cd, Ctor_Complete,
                                 /*forVirtualBase=*/false,
                                 /*delegating=*/false, args);
  cir::FuncOp calleePtr =
      cgm.getAddrOfCXXStructor(GlobalDecl(cd, Ctor_Complete));
  CIRGenCallee callee =
      CIRGenCallee::forDirect(calleePtr, GlobalDecl(cd, Ctor_Complete));
  const CIRGenFunctionInfo &calleeInfo = cgm.getTypes().arrangeCXXConstructorCall(
      args, cd, Ctor_Complete, extraArgs.prefix, extraArgs.suffix,
      /*passProtoArgs=*/true);
  cgf.emitCall(calleeInfo, callee, ReturnValueSlot(), args,
               /*callOrTryCall=*/nullptr, /*isMustTail=*/false,
               cd->getSourceRange());

  cleanups.forceCleanup();

  cir::ReturnOp::create(cgf.getBuilder(), loc);
  cgf.finishFunction(SourceLocation());

  return thunkFn;
}

mlir::Attribute
CIRGenMicrosoftCXXABI::getCatchableType(mlir::Location loc, QualType T,
                                        uint32_t NVOffset,
                                        int32_t VBPtrOffset,
                                        uint32_t VBIndex) {
  assert(!T->isReferenceType());

  CXXRecordDecl *RD = T->getAsCXXRecordDecl();
  const CXXConstructorDecl *CD =
      RD ? cgm.getASTContext().getCopyConstructorForExceptionObject(
               const_cast<CXXRecordDecl *>(RD))
         : nullptr;
  CXXCtorType CT = Ctor_Complete;
  if (CD)
    if (!hasDefaultCXXMethodCC(cgm.getASTContext(), CD) ||
        CD->getNumParams() != 1)
      CT = Ctor_CopyingClosure;

  uint32_t Size = cgm.getASTContext().getTypeSizeInChars(T).getQuantity();
  SmallString<256> MangledName;
  {
    llvm::raw_svector_ostream Out(MangledName);
    getMangleContext().mangleCXXCatchableType(T, CD, CT, Size, NVOffset,
                                              VBPtrOffset, VBIndex, Out);
  }

  CIRGenBuilderTy &builder = cgm.getBuilder();
  cir::PointerType voidPtrTy = builder.getVoidPtrTy();
  if (auto gv = cgm.getGlobalValue(MangledName)) {
    auto globalOp = cast<cir::GlobalOp>(gv);
    return getImageRelativeAttr(
        builder.getGlobalViewAttr(voidPtrTy, globalOp), loc);
  }

  mlir::Attribute TD =
      getImageRelativeAttr(getAddrOfRTTIDescriptor(loc, T), loc);

  mlir::Attribute CopyCtorAttr;
  if (CD) {
    if (CT == Ctor_CopyingClosure) {
      cir::FuncOp closureFn = getAddrOfCXXCtorClosure(CD, Ctor_CopyingClosure);
      CopyCtorAttr = mlir::FlatSymbolRefAttr::get(closureFn.getSymNameAttr());
    } else {
      cir::FuncOp structorFn =
          cgm.getAddrOfCXXStructor(GlobalDecl(CD, Ctor_Complete));
      CopyCtorAttr = mlir::FlatSymbolRefAttr::get(structorFn.getSymNameAttr());
    }
  }
  CopyCtorAttr = getImageRelativeAttr(CopyCtorAttr, loc);

  bool IsScalar = !RD;
  bool HasVirtualBases = false;
  bool IsStdBadAlloc = false;
  QualType PointeeType = T;
  if (T->isPointerType())
    PointeeType = T->getPointeeType();
  if (const CXXRecordDecl *PointeeRD = PointeeType->getAsCXXRecordDecl()) {
    HasVirtualBases = PointeeRD->getNumVBases() > 0;
    if (IdentifierInfo *II = PointeeRD->getIdentifier())
      IsStdBadAlloc = II->isStr("bad_alloc") && PointeeRD->isInStdNamespace();
  }

  uint32_t Flags = 0;
  if (IsScalar)
    Flags |= 1;
  if (HasVirtualBases)
    Flags |= 4;
  if (IsStdBadAlloc)
    Flags |= 16;

  cir::IntType s32Ty = builder.getSInt32Ty();
  cir::RecordType ctType = getCatchableTypeType();
  mlir::Attribute fields[] = {
      cir::IntAttr::get(s32Ty, Flags),
      TD,
      cir::IntAttr::get(s32Ty, NVOffset),
      cir::IntAttr::get(s32Ty, VBPtrOffset),
      cir::IntAttr::get(s32Ty, VBIndex),
      cir::IntAttr::get(s32Ty, Size),
      CopyCtorAttr
  };

  cir::GlobalOp gv =
      cgm.createGlobalOp(loc, MangledName, ctType, /*isConstant=*/true);
  gv.setInitialValueAttr(cir::ConstRecordAttr::get(
      ctType, mlir::ArrayAttr::get(&cgm.getMLIRContext(), fields)));
  cir::GlobalLinkageKind linkage = getLinkageForRTTI(T);
  gv.setLinkage(linkage);
  gv.setSectionAttr(builder.getStringAttr(".xdata"));
  if (cgm.supportsCOMDAT() && cir::isWeakForLinker(linkage))
    gv.setSelfComdat();
  mlir::SymbolTable::setSymbolVisibility(gv,
                                         CIRGenModule::getMLIRVisibility(gv));

  return getImageRelativeAttr(builder.getGlobalViewAttr(voidPtrTy, gv), loc);
}

cir::GlobalOp CIRGenMicrosoftCXXABI::getCatchableTypeArray(mlir::Location loc,
                                                           QualType T) {
  assert(!T->isReferenceType());

  auto &CTA = catchableTypeArrays[T];
  if (CTA)
    return CTA;

  llvm::SmallSetVector<mlir::Attribute, 2> catchableTypes;

  const CXXRecordDecl *mostDerivedClass = nullptr;
  bool isPointer = T->isPointerType();
  if (isPointer)
    mostDerivedClass = T->getPointeeType()->getAsCXXRecordDecl();
  else
    mostDerivedClass = T->getAsCXXRecordDecl();

  if (mostDerivedClass) {
    const ASTContext &context = cgm.getASTContext();
    const ASTRecordLayout &mostDerivedLayout =
        context.getASTRecordLayout(mostDerivedClass);
    MicrosoftVTableContext &vtableContext = cgm.getMicrosoftVTableContext();
    SmallVector<MSRTTIClass, 8> classes;
    serializeClassHierarchy(classes, mostDerivedClass);
    classes.front().initialize(/*Parent=*/nullptr, /*Specifier=*/nullptr);
    detectAmbiguousBases(classes);
    for (const MSRTTIClass &cl : classes) {
      if (cl.Flags & (MSRTTIClass::IsPrivateOnPath | MSRTTIClass::IsAmbiguous))
        continue;
      uint32_t offsetInVBTable = 0;
      int32_t vbPtrOffset = -1;
      if (cl.VirtualRoot) {
        offsetInVBTable =
            vtableContext.getVBTableIndex(mostDerivedClass, cl.VirtualRoot) * 4;
        vbPtrOffset = mostDerivedLayout.getVBPtrOffset().getQuantity();
      }

      CanQualType rttiTy = context.getCanonicalTagType(cl.RD);
      if (isPointer)
        rttiTy = context.getPointerType(rttiTy);
      catchableTypes.insert(getCatchableType(loc, rttiTy, cl.OffsetInVBase,
                                             vbPtrOffset, offsetInVBTable));
    }
  }

  catchableTypes.insert(getCatchableType(loc, T));

  if (isPointer && T->getPointeeType()->isObjectType())
    catchableTypes.insert(getCatchableType(loc, cgm.getASTContext().VoidPtrTy));

  if (T->isNullPtrType())
    catchableTypes.insert(getCatchableType(loc, cgm.getASTContext().VoidPtrTy));

  uint32_t numEntries = catchableTypes.size();
  CIRGenBuilderTy &builder = cgm.getBuilder();
  cir::IntType s32Ty = builder.getSInt32Ty();
  mlir::Type ctType = getImageRelativeType(builder.getVoidPtrTy());
  cir::ArrayType at = cir::ArrayType::get(ctType, numEntries);
  cir::RecordType ctaType = getCatchableTypeArrayType(numEntries);

  mlir::Attribute fields[] = {
      cir::IntAttr::get(s32Ty, numEntries),
      cir::ConstArrayAttr::get(
          at, mlir::ArrayAttr::get(&cgm.getMLIRContext(),
                                   catchableTypes.getArrayRef()))
  };

  SmallString<256> mangledName;
  {
    llvm::raw_svector_ostream out(mangledName);
    getMangleContext().mangleCXXCatchableTypeArray(T, numEntries, out);
  }

  if (auto gv = cgm.getGlobalValue(mangledName)) {
    CTA = cast<cir::GlobalOp>(gv);
    return CTA;
  }

  CTA = cgm.createGlobalOp(loc, mangledName, ctaType, /*isConstant=*/true);
  CTA.setInitialValueAttr(cir::ConstRecordAttr::get(
      ctaType, mlir::ArrayAttr::get(&cgm.getMLIRContext(), fields)));
  cir::GlobalLinkageKind linkage = getLinkageForRTTI(T);
  CTA.setLinkage(linkage);
  CTA.setSectionAttr(builder.getStringAttr(".xdata"));
  if (cgm.supportsCOMDAT() && cir::isWeakForLinker(linkage))
    CTA.setSelfComdat();
  mlir::SymbolTable::setSymbolVisibility(CTA,
                                         CIRGenModule::getMLIRVisibility(CTA));
  return CTA;
}

cir::GlobalOp CIRGenMicrosoftCXXABI::getThrowInfo(mlir::Location loc,
                                                  QualType throwType) {
  auto it = throwInfos.find(throwType);
  if (it != throwInfos.end())
    return it->second;

  bool isConst, isVolatile, isUnaligned;
  QualType decomposedType = decomposeTypeForEH(
      cgm.getASTContext(), throwType, isConst, isVolatile, isUnaligned);

  cir::GlobalOp cta = getCatchableTypeArray(loc, decomposedType);
  auto ctaRecordTy = mlir::cast<cir::RecordType>(cta.getSymType());
  auto arrayMemberTy = mlir::cast<cir::ArrayType>(ctaRecordTy.getMembers()[1]);
  uint32_t numEntries = arrayMemberTy.getSize();

  SmallString<256> mangledName;
  {
    llvm::raw_svector_ostream out(mangledName);
    getMangleContext().mangleCXXThrowInfo(decomposedType, isConst, isVolatile,
                                          isUnaligned, numEntries, out);
  }

  if (auto gv = cgm.getGlobalValue(mangledName)) {
    auto globalOp = cast<cir::GlobalOp>(gv);
    throwInfos[throwType] = globalOp;
    return globalOp;
  }

  uint32_t flags = 0;
  if (isConst)
    flags |= 1;
  if (isVolatile)
    flags |= 2;
  if (isUnaligned)
    flags |= 4;

  CIRGenBuilderTy &builder = cgm.getBuilder();
  cir::PointerType voidPtrTy = builder.getVoidPtrTy();

  mlir::Attribute cleanupFnAttr;
  if (const CXXRecordDecl *rd = decomposedType->getAsCXXRecordDecl()) {
    if (CXXDestructorDecl *dtorD = rd->getDestructor()) {
      if (!dtorD->isTrivial()) {
        cir::FuncOp dtorFn =
            cgm.getAddrOfCXXStructor(GlobalDecl(dtorD, Dtor_Complete));
        cleanupFnAttr = mlir::FlatSymbolRefAttr::get(dtorFn.getSymNameAttr());
      }
    }
  }
  cleanupFnAttr = getImageRelativeAttr(cleanupFnAttr, loc);

  mlir::Attribute forwardCompat = getImageRelativeAttr(mlir::Attribute(), loc);

  mlir::Attribute pointerToCatchableTypes = getImageRelativeAttr(
      builder.getGlobalViewAttr(voidPtrTy, cta), loc);

  cir::RecordType tiType = getThrowInfoType();
  cir::IntType s32Ty = builder.getSInt32Ty();
  mlir::Attribute fields[] = {
      cir::IntAttr::get(s32Ty, flags),
      cleanupFnAttr,
      forwardCompat,
      pointerToCatchableTypes
  };

  cir::GlobalOp gv =
      cgm.createGlobalOp(loc, mangledName, tiType, /*isConstant=*/true);
  gv.setInitialValueAttr(cir::ConstRecordAttr::get(
      tiType, mlir::ArrayAttr::get(&cgm.getMLIRContext(), fields)));
  cir::GlobalLinkageKind linkage = getLinkageForRTTI(decomposedType);
  gv.setLinkage(linkage);
  gv.setSectionAttr(builder.getStringAttr(".xdata"));
  if (cgm.supportsCOMDAT() && cir::isWeakForLinker(linkage))
    gv.setSelfComdat();
  mlir::SymbolTable::setSymbolVisibility(gv,
                                         CIRGenModule::getMLIRVisibility(gv));

  throwInfos[throwType] = gv;
  return gv;
}

void CIRGenMicrosoftCXXABI::emitRethrow(CIRGenFunction &cgf, bool isNoReturn) {
  if (isNoReturn) {
    CIRGenBuilderTy &builder = cgf.getBuilder();
    assert(cgf.currSrcLoc && "expected source location");
    mlir::Location loc = cgf.getLoc(*cgf.currSrcLoc);
    insertThrowAndSplit(builder, loc);
  } else {
    cgm.errorNYI("emitRethrow with isNoReturn false: MSVC ABI");
  }
}

void CIRGenMicrosoftCXXABI::emitThrow(CIRGenFunction &cgf,
                                      const CXXThrowExpr *e) {
  const Expr *subExpr = e->getSubExpr();
  assert(subExpr && "subExpr cannot be null");
  QualType throwType = subExpr->getType();
  mlir::Location loc = cgf.getLoc(e->getSourceRange());

  Address ai = cgf.createMemTempWithoutCast(throwType, loc, "exception");
  cgf.emitAnyExprToMem(subExpr, ai, throwType.getQualifiers(),
                       /*isInitializer=*/true);

  cir::GlobalOp ti = getThrowInfo(loc, throwType);
  auto typeInfoAttr = mlir::FlatSymbolRefAttr::get(ti.getSymNameAttr());

  insertThrowAndSplit(cgf.getBuilder(), loc, ai.getPointer(), typeInfoAttr,
                      mlir::FlatSymbolRefAttr());
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

void CIRGenMicrosoftCXXABI::getStaticLocalGuardName(
    const VarDecl &varDecl, SmallVectorImpl<char> &out) {
  llvm::raw_svector_ostream stream(out);
  bool threadlocalStatic = varDecl.getTLSKind() != VarDecl::TLS_None;
  bool threadsafeStatic = cgm.getASTContext().getLangOpts().ThreadsafeStatics;
  bool hasPerVariableGuard = threadsafeStatic && !threadlocalStatic;

  if (hasPerVariableGuard) {
    unsigned guardNum;
    if (varDecl.isExternallyVisible()) {
      guardNum = cgm.getASTContext().getStaticLocalNumber(&varDecl);
      assert(guardNum > 0);
      guardNum--;
    } else {
      guardNum = threadSafeGuardNumMap[varDecl.getDeclContext()]++;
    }
    getMangleContext().mangleThreadSafeStaticGuardVariable(&varDecl, guardNum,
                                                           stream);
  } else {
    getMangleContext().mangleStaticGuardVariable(&varDecl, stream);
  }
}

CIRGenCXXABI *clang::CIRGen::CreateCIRGenMicrosoftCXXABI(CIRGenModule &cgm) {
  return new CIRGenMicrosoftCXXABI(cgm);
}
