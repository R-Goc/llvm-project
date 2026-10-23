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
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Mangle.h"
#include "clang/Basic/TargetInfo.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::CIRGen;

namespace {

class CIRGenMicrosoftCXXABI : public CIRGenCXXABI {
public:
  CIRGenMicrosoftCXXABI(CIRGenModule &cgm) : CIRGenCXXABI(cgm) {}

  bool hasThisReturn(GlobalDecl gd) const override {
    return isa<CXXConstructorDecl>(gd.getDecl());
  }

  bool hasMostDerivedReturn(GlobalDecl gd) const override { return false; }

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
    return false;
  }

  bool isZeroInitializable(const MemberPointerType *mpt) override {
    return true;
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

  void setThunkLinkage(cir::FuncOp thunk, bool forVTable, GlobalDecl gd,
                       bool returnAdjustment) override {
    cgm.errorNYI("setThunkLinkage: MSVC ABI");
  }

  AddedStructorArgCounts
  buildStructorSignature(GlobalDecl gd,
                         SmallVectorImpl<CanQualType> &argTys) override {
    return AddedStructorArgCounts{};
  }

  void addImplicitStructorParams(CIRGenFunction &cgf, QualType &resTy,
                                 FunctionArgList &params) override {}

  void emitInstanceFunctionProlog(SourceLocation loc,
                                  CIRGenFunction &cgf) override {
    if (cgf.curFuncDecl && cgf.curFuncDecl->hasAttr<NakedAttr>())
      return;

    mlir::Value thisVal = loadIncomingCXXThis(cgf);
    setCXXABIThisValue(cgf, thisVal);

    if (hasThisReturn(cgf.curGD)) {
      if (cgf.returnValue.isValid())
        cgf.getBuilder().createStore(cgf.getLoc(loc), thisVal, cgf.returnValue);
    }
  }

  AddedStructorArgs getImplicitConstructorArgs(CIRGenFunction &cgf,
                                               const CXXConstructorDecl *d,
                                               CXXCtorType type,
                                               bool forVirtualBase,
                                               bool delegating) override {
    return AddedStructorArgs{};
  }

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
                                        DeleteOrMemberCallExpr e) override {
    cgf.cgm.errorNYI(dtor->getSourceRange(),
                     "emitVirtualDestructorCall: MSVC ABI");
    return nullptr;
  }

  void emitVirtualObjectDelete(CIRGenFunction &cgf, const CXXDeleteExpr *de,
                               Address ptr, QualType elementType,
                               const CXXDestructorDecl *dtor) override {
    cgf.cgm.errorNYI(de->getSourceRange(), "emitVirtualObjectDelete: MSVC ABI");
  }

  size_t getSrcArgforCopyCtor(const CXXConstructorDecl *cd,
                              FunctionArgList &args) const override {
    return 1;
  }

  const CXXRecordDecl *
  getThisArgumentTypeForMethod(const CXXMethodDecl *md) override {
    return md->getParent();
  }

  Address adjustThisArgumentForVirtualFunctionCall(CIRGenFunction &cgf,
                                                   GlobalDecl gd,
                                                   Address thisAddr,
                                                   bool virtualCall) override {
    return thisAddr;
  }

  bool isVirtualOffsetNeededForVTableField(CIRGenFunction &cgf,
                                           CIRGenFunction::VPtr vptr) override {
    return false;
  }

  cir::GlobalOp getAddrOfVTable(const CXXRecordDecl *rd,
                                CharUnits vptrOffset) override {
    cgm.errorNYI(rd->getSourceRange(), "getAddrOfVTable: MSVC ABI");
    return nullptr;
  }

  mlir::Value getVTableAddressPoint(BaseSubobject base,
                                    const CXXRecordDecl *vtableClass) override {
    cgm.errorNYI(vtableClass->getSourceRange(),
                 "getVTableAddressPoint: MSVC ABI");
    return nullptr;
  }

  cir::GlobalViewAttr
  getVTableAddressPointAttr(BaseSubobject base,
                            const CXXRecordDecl *vtableClass) override {
    cgm.errorNYI(vtableClass->getSourceRange(),
                 "getVTableAddressPointAttr: MSVC ABI");
    return {};
  }

  mlir::Value getVTableAddressPointInStructor(
      CIRGenFunction &cgf, const CXXRecordDecl *vtableClass, BaseSubobject base,
      const CXXRecordDecl *nearestVBase) override {
    cgf.cgm.errorNYI(vtableClass->getSourceRange(),
                     "getVTableAddressPointInStructor: MSVC ABI");
    return nullptr;
  }

  CIRGenCallee getVirtualFunctionPointer(CIRGenFunction &cgf, GlobalDecl gd,
                                         Address thisAddr, mlir::Type ty,
                                         SourceLocation loc) override {
    cgf.cgm.errorNYI(loc, "getVirtualFunctionPointer: MSVC ABI");
    return CIRGenCallee();
  }

  void emitVTableDefinitions(CIRGenVTables &cgvt,
                             const CXXRecordDecl *rd) override {
    cgm.errorNYI(rd->getSourceRange(), "emitVTableDefinitions: MSVC ABI");
  }

  void emitVirtualInheritanceTables(const CXXRecordDecl *rd) override {
    cgm.errorNYI(rd->getSourceRange(),
                 "emitVirtualInheritanceTables: MSVC ABI");
  }

  mlir::Value
  getVirtualBaseClassOffset(mlir::Location loc, CIRGenFunction &cgf,
                            Address thisAddr, const CXXRecordDecl *classDecl,
                            const CXXRecordDecl *baseClassDecl) override {
    cgf.cgm.errorNYI(loc, "getVirtualBaseClassOffset: MSVC ABI");
    return nullptr;
  }

  cir::MethodAttr buildVirtualMethodAttr(cir::MethodType methodTy,
                                         const CXXMethodDecl *md) override {
    cgm.errorNYI(md->getSourceRange(), "buildVirtualMethodAttr: MSVC ABI");
    return {};
  }

  mlir::Value performThisAdjustment(CIRGenFunction &cgf, Address thisAddr,
                                    const CXXRecordDecl *unadjustedClass,
                                    const ThunkInfo &ti) override {
    cgf.cgm.errorNYI("performThisAdjustment: MSVC ABI");
    return thisAddr.emitRawPointer();
  }

  mlir::Value performReturnAdjustment(CIRGenFunction &cgf, Address ret,
                                      const CXXRecordDecl *unadjustedClass,
                                      const ReturnAdjustment &ra) override {
    cgf.cgm.errorNYI("performReturnAdjustment: MSVC ABI");
    return ret.emitRawPointer();
  }

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
                                          QualType ty) override {
    cgm.errorNYI(loc, "getAddrOfRTTIDescriptor: MSVC ABI");
    return nullptr;
  }

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

  if (gd.getDtorType() == Dtor_Deleting ||
      gd.getDtorType() == Dtor_VectorDeleting) {
    cgm.errorNYI(dtor->getSourceRange(),
                 "emitCXXStructor: MSVC deleting destructor");
    return;
  }

  auto fn = cgm.codegenCXXStructor(gd);
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
CIRGenCXXABI *clang::CIRGen::CreateCIRGenMicrosoftCXXABI(CIRGenModule &cgm) {
  return new CIRGenMicrosoftCXXABI(cgm);
}
