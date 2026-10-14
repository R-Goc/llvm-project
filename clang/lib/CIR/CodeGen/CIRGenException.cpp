//===--- CIRGenException.cpp - Emit CIR Code for C++ exceptions -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This contains code dealing with C++ exception related code generation.
//
//===----------------------------------------------------------------------===//

#include "CIRGenCXXABI.h"
#include "CIRGenFunction.h"
#include "mlir/IR/Block.h"
#include "mlir/IR/Location.h"

#include "clang/AST/Mangle.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/CIR/MissingFeatures.h"
#include "llvm/Support/SaveAndRestore.h"

using namespace clang;
using namespace clang::CIRGen;

namespace clang::CIRGen {

const EHPersonality &getEHPersonality(CIRGenModule &cgm,
                                      const FunctionDecl *fd) {
  return CodeGenUtils::getEHPersonality(cgm.getTarget(), cgm.getLangOpts(),
                                        cgm.getCodeGenOpts(), fd);
}

const EHPersonality &getEHPersonality(CIRGenFunction &cgf) {
  const auto *fg = cgf.curCodeDecl;
  // For outlined finallys and filters, use the SEH personality in case they
  // contain more SEH. This mostly only affects finallys. Filters could
  // hypothetically use gnu statement expressions to sneak in nested SEH.
  fg = fg ? fg : cgf.curSEHParent.getDecl();
  return getEHPersonality(cgf.cgm, dyn_cast_or_null<FunctionDecl>(fg));
}

} // namespace clang::CIRGen

static llvm::StringRef getPersonalityFn(CIRGenModule &cgm,
                                        const EHPersonality &personality) {
  // Create the personality function type: i32 (...)
  mlir::Type i32Ty = cgm.getBuilder().getSInt32Ty();
  auto funcTy = cir::FuncType::get({}, i32Ty, /*isVarArg=*/true);

  cir::FuncOp personalityFn = cgm.createRuntimeFunction(
      funcTy, personality.PersonalityFn, {}, /*isLocal=*/true);

  return personalityFn.getSymName();
}

void CIRGenFunction::emitStartEHSpec(const Decl *d) {
  if (!cgm.getLangOpts().CXXExceptions)
    return;

  const FunctionDecl *fd = dyn_cast_or_null<FunctionDecl>(d);
  if (!fd) {
    // This can't currently happen in CIR, but if we do get here, we need to
    // handle the cd->isNoThrow() case.
    if (const CapturedDecl *cd = dyn_cast_or_null<CapturedDecl>(d)) {
      if (cd->isNothrow())
        cgm.errorNYI(cd->getSourceRange(),
                     "emitStartEHSpec CapturedDecl nothrow");
    }
    return;
  }

  const FunctionProtoType *proto = fd->getType()->getAs<FunctionProtoType>();
  if (!proto)
    return;

  ExceptionSpecificationType est = proto->getExceptionSpecType();
  // In C++17 and later, and in Wasm EH in any standard, 'throw()' aka
  // EST_DynamicNone is treated the same way as noexcept. In earlier standards
  // it is handled with 'throw(X...)'.
  bool isDynamicSpec = est == EST_Dynamic ||
                       (est == EST_DynamicNone && !getLangOpts().CPlusPlus17 &&
                        !cgm.getCodeGenOpts().hasWasmExceptions());

  // A specification that permits nothing to escape is a terminate scope: any
  // exception that tries to leave the function calls std::terminate. Under
  // -EHa a hardware exception can still occur, so there is no scope.
  bool needsTerminate = !isDynamicSpec && proto->canThrow() == CT_Cannot &&
                        !getLangOpts().EHAsynch;

  if (isDynamicSpec) {
    // TODO: Revisit exception specifications for the MS ABI. There is a way
    // to encode these in an object file but MSVC doesn't do anything with it.
    if (getTarget().getCXXABI().isMicrosoft())
      return;

    // In Wasm EH, a specification with types is ignored with a warning for
    // now. 'throw()' is a terminate scope, which the classification above
    // takes care of.
    // TODO Correctly handle exception specification in Wasm EH
    if (cgm.getCodeGenOpts().hasWasmExceptions()) {
      cgm.getDiags().Report(d->getLocation(),
                            diag::warn_wasm_dynamic_exception_spec_ignored)
          << fd->getExceptionSpecSourceRange();
      return;
    }

    // Currently Emscripten EH only handles 'throw()' but not 'throw' with
    // types. 'throw()' handling will be done in JS glue code so we don't need
    // to do anything in that case. Just print a warning message in case of
    // throw with types.
    // TODO Correctly handle exception specification in Emscripten EH
    if (getTarget().getCXXABI() == TargetCXXABI::WebAssembly &&
        (cgm.getCodeGenOpts().getExceptionHandling() ==
             clang::CodeGenOptions::ExceptionHandlingKind::None ||
         cgm.getCodeGenOpts().getExceptionHandling() ==
             clang::CodeGenOptions::ExceptionHandlingKind::Default) &&
        est == EST_Dynamic)
      cgm.getDiags().Report(d->getLocation(),
                            diag::warn_wasm_dynamic_exception_spec_ignored)
          << fd->getExceptionSpecSourceRange();
  } else if (!needsTerminate) {
    return;
  }

  mlir::Location loc = getLoc(fd->getSourceRange());

  SmallVector<mlir::Attribute, 4> permittedTypes;
  if (isDynamicSpec) {
    for (QualType ty : proto->exceptions()) {
      QualType exceptType = ty.getNonReferenceType().getUnqualifiedType();
      permittedTypes.push_back(
          cgm.getAddrOfRTTIDescriptor(loc, exceptType, /*forEh=*/true));
    }
  }

  cir::FuncOp funcOp = mlir::cast<cir::FuncOp>(curFn);
  if (!funcOp.getPersonality())
    funcOp.setPersonality(getPersonalityFn(cgm, getEHPersonality(*this)));

  bool emptyFilter = permittedTypes.empty();
  ehSpecTryOp = cir::TryOp::create(
      builder, loc,
      // The try body holds the function body, which the caller emits after
      // this function returns and emitEndEHSpec terminates.
      /*tryBuilder=*/[](mlir::OpBuilder &, mlir::Location) {},
      /*handlersBuilder=*/
      [&](mlir::OpBuilder &b, mlir::Location loc,
          mlir::OperationState &result) {
        mlir::OpBuilder::InsertionGuard guard(b);
        mlir::Type ehTokenTy = cir::EhTokenType::get(&getMLIRContext());

        if (needsTerminate) {
          mlir::Block *terminateBlock = b.createBlock(
              result.addRegion(), /*insertPt=*/{}, {ehTokenTy}, {loc});
          cir::EhTerminateOp::create(b, loc, terminateBlock->getArgument(0));
          return;
        }

        mlir::Block *filterBlock = b.createBlock(
            result.addRegion(), /*insertPt=*/{}, {ehTokenTy}, {loc});
        if (emptyFilter)
          cir::UnreachableOp::create(b, loc);
        else
          cir::ResumeOp::create(b, loc, filterBlock->getArgument(0));

        mlir::Block *unexpectedBlock = b.createBlock(
            result.addRegion(), /*insertPt=*/{}, {ehTokenTy}, {loc});
        cir::EhUnexpectedOp::create(b, loc, unexpectedBlock->getArgument(0));
      });

  SmallVector<mlir::Attribute, 2> handlerAttrs;
  if (needsTerminate) {
    // Any exception reaching the handler terminates the program, so it catches
    // everything. The catch itself is performed by the runtime helper that
    // cir.eh.terminate lowers to.
    handlerAttrs.push_back(cir::CatchAllAttr::get(&getMLIRContext()));
  } else {
    handlerAttrs.push_back(cir::EhFilterAttr::get(
        &getMLIRContext(), builder.getArrayAttr(permittedTypes)));
    handlerAttrs.push_back(cir::EhUnexpectedAttr::get(&getMLIRContext()));
  }
  ehSpecTryOp.setHandlerTypesAttr(
      mlir::ArrayAttr::get(&getMLIRContext(), handlerAttrs));

  // Continue emitting into the try body.
  builder.setInsertionPointToEnd(&ehSpecTryOp.getTryRegion().front());
}

void CIRGenFunction::emitEndEHSpec(const Decl *) {
  if (!ehSpecTryOp)
    return;

  cir::TryOp tryOp = ehSpecTryOp;
  ehSpecTryOp = cir::TryOp();

  // Terminate the try body. Emitting the function body may have left the last
  // block without a terminator, for instance when control falls off the end.
  mlir::Block *bodyExit = &tryOp.getTryRegion().back();
  if (bodyExit->empty() ||
      !bodyExit->back().hasTrait<mlir::OpTrait::IsTerminator>()) {
    mlir::OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToEnd(bodyExit);
    builder.createYield(tryOp.getLoc());
  }

  // Continue emitting the function epilogue outside the try.
  builder.setInsertionPointAfter(tryOp);
}

void CIRGenFunction::emitCXXThrowExpr(const CXXThrowExpr *e) {
  const llvm::Triple &triple = getTarget().getTriple();
  if (cgm.getLangOpts().OpenMPIsTargetDevice &&
      (triple.isNVPTX() || triple.isAMDGCN())) {
    cgm.errorNYI("emitCXXThrowExpr OpenMP with NVPTX or AMDGCN Triples");
    return;
  }

  if (const Expr *subExpr = e->getSubExpr()) {
    QualType throwType = subExpr->getType();
    if (throwType->isObjCObjectPointerType()) {
      cgm.errorNYI("emitCXXThrowExpr ObjCObjectPointerType");
      return;
    }

    cgm.getCXXABI().emitThrow(*this, e);
    return;
  }

  cgm.getCXXABI().emitRethrow(*this, /*isNoReturn=*/true);
}

void CIRGenFunction::emitAnyExprToExn(const Expr *e, Address addr) {
  // Make sure the exception object is cleaned up if there's an
  // exception during initialization.
  assert(!cir::MissingFeatures::ehCleanupScope());

  // __cxa_allocate_exception returns a void*;  we need to cast this
  // to the appropriate type for the object.
  mlir::Type ty = convertTypeForMem(e->getType());
  Address typedAddr = addr.withElementType(builder, ty);

  // From LLVM's codegen:
  // FIXME: this isn't quite right!  If there's a final unelided call
  // to a copy constructor, then according to [except.terminate]p1 we
  // must call std::terminate() if that constructor throws, because
  // technically that copy occurs after the exception expression is
  // evaluated but before the exception is caught.  But the best way
  // to handle that is to teach EmitAggExpr to do the final copy
  // differently if it can't be elided.
  emitAnyExprToMem(e, typedAddr, e->getType().getQualifiers(),
                   /*isInitializer=*/true);

  // Deactivate the cleanup block.
  assert(!cir::MissingFeatures::ehCleanupScope());
}

void CIRGenFunction::addCatchHandlerAttr(
    const CXXCatchStmt *catchStmt, SmallVector<mlir::Attribute> &handlerAttrs) {
  mlir::Location catchLoc = getLoc(catchStmt->getBeginLoc());

  if (catchStmt->getExceptionDecl()) {
    // FIXME: Dropping the reference type on the type into makes it
    // impossible to correctly implement catch-by-reference
    // semantics for pointers.  Unfortunately, this is what all
    // existing compilers do, and it's not clear that the standard
    // personality routine is capable of doing this right.  See C++ DR 388:
    //   http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_active.html#388
    Qualifiers caughtTypeQuals;
    QualType caughtType = cgm.getASTContext().getUnqualifiedArrayType(
        catchStmt->getCaughtType().getNonReferenceType(), caughtTypeQuals);
    if (caughtType->isObjCObjectPointerType()) {
      cgm.errorNYI("addCatchHandlerAttr: caughtType ObjCObjectPointerType");
      return;
    }

    CatchTypeInfo typeInfo = cgm.getCXXABI().getAddrOfCXXCatchHandlerType(
        catchLoc, caughtType, catchStmt->getCaughtType());
    handlerAttrs.push_back(typeInfo.rtti);
  } else {
    // No exception decl indicates '...', a catch-all.
    handlerAttrs.push_back(cir::CatchAllAttr::get(&getMLIRContext()));
  }
}

namespace {
struct CallEndCatch final : EHScopeStack::Cleanup {
  CallEndCatch(mlir::Value catchToken) : catchToken(catchToken) {}
  mlir::Value catchToken;

  void emit(CIRGenFunction &cgf, Flags flags) override {
    cir::EndCatchOp::create(cgf.getBuilder(), cgf.getLoc(*cgf.currSrcLoc),
                            catchToken);
    cir::YieldOp::create(cgf.getBuilder(), cgf.getLoc(*cgf.currSrcLoc));
  }
};
} // namespace

static cir::BeginCatchOp
callBeginCatch(CIRGenFunction &cgf, mlir::Value ehToken, mlir::Type exnPtrTy,
               mlir::Value paramAddr = {}, cir::GlobalViewAttr typeInfo = {},
               std::optional<uint32_t> flags = std::nullopt) {
  auto catchTokenTy = cir::CatchTokenType::get(cgf.getBuilder().getContext());
  mlir::IntegerAttr flagsAttr;
  if (flags)
    flagsAttr = cgf.getBuilder().getI32IntegerAttr(*flags);
  auto beginCatch = cir::BeginCatchOp::create(
      cgf.getBuilder(), cgf.getBuilder().getUnknownLoc(),
      catchTokenTy, exnPtrTy, ehToken, paramAddr, typeInfo, flagsAttr);

  const auto cleanupKind =
      cgf.cgm.getTarget().getCXXABI().isMicrosoft()
          ? NormalCleanup
          : NormalAndEHCleanup;
  cgf.ehStack.pushCleanup<CallEndCatch>(cleanupKind,
                                        beginCatch.getCatchToken());

  return beginCatch;
}

/// Get or create the catch-init copy thunk for \p catchParam.
///
/// The copy thunk has signature `void(T*, T*)` (where `T` is the catch
/// parameter type) and contains the normal aggregate emission of the catch
/// parameter's init expression.
///
/// The thunk name is keyed off the catch parameter's canonical type mangled
/// name, so a single translation unit emits at most one thunk per catch type.
static cir::FuncOp getOrCreateCopyThunk(CIRGenFunction &cgf,
                                        const VarDecl &catchParam,
                                        cir::PointerType paramAddrType,
                                        SourceLocation clangLoc) {
  mlir::Location loc = cgf.getLoc(clangLoc);
  CIRGenModule &cgm = cgf.cgm;
  CIRGenBuilderTy &builder = cgm.getBuilder();
  mlir::ModuleOp mod = cgm.getModule();

  const Expr *copyExpr = catchParam.getInit();
  assert(copyExpr && "non-trivial copy expects a copy expression");

  llvm::SmallString<128> thunkName;
  llvm::raw_svector_ostream thunkNameStream(thunkName);
  thunkNameStream << "__clang_cir_catch_copy_";
  cgm.getCXXABI().getMangleContext().mangleCanonicalTypeName(
      catchParam.getType(), thunkNameStream);

  if (cir::FuncOp existing = cgm.lookupFuncOp(thunkName))
    return existing;

  mlir::Type voidTy = cir::VoidType::get(builder.getContext());
  auto thunkTy = cir::FuncType::get({paramAddrType, paramAddrType}, voidTy,
                                    /*isVarArg=*/false);

  mlir::OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToEnd(mod.getBody());
  cir::FuncOp thunk = cir::FuncOp::create(builder, loc, thunkName, thunkTy);
  cgm.insertGlobalSymbol(thunk);
  thunk.setLinkage(cir::GlobalLinkageKind::LinkOnceODRLinkage);
  thunk.setGlobalVisibility(cir::VisibilityKind::Hidden);
  thunk->setAttr(cir::CIRDialect::getCatchCopyThunkAttrName(),
                 builder.getUnitAttr());

  mlir::Block *entry = thunk.addEntryBlock();
  builder.setInsertionPointToStart(entry);

  // Use a fresh CIRGenFunction to drive the body emission. We need just enough
  // state for emitAggExpr / emitCXXConstructorCall to compute the call-site
  // argument attributes; the helper has no AST decl, no exception scopes, and
  // no return value, so we bypass the full startFunction/finishFunction
  // machinery.
  CIRGenFunction subCgf(cgm, builder);
  subCgf.curFn = thunk;

  // Some emission paths (e.g. materializing temporaries for default args via
  // emitAnyExprToTemp) need both a current source location and a lexical
  // scope to anchor allocas. Since we bypass startFunction, install both
  // explicitly for the lifetime of the thunk's body emission.
  CIRGenFunction::SourceLocRAIIObject thunkLoc(subCgf, clangLoc);
  CIRGenFunction::LexicalScope thunkScope(subCgf, loc, entry);

  // Bind the OpaqueValueExpr at the source position of the catch parameter's
  // copy expression to an LValue at the thunk's `src` block argument.
  LValue srcLV = subCgf.makeNaturalAlignAddrLValue(entry->getArgument(1),
                                                   catchParam.getType());
  CIRGenFunction::OpaqueValueMapping opaqueValue(
      subCgf, OpaqueValueExpr::findInCopyConstruct(copyExpr), srcLV);

  // Drive the construction into the helper's `dest` block argument via the
  // normal aggregate-emission machinery so that `ExprWithCleanups`,
  // converting/inheriting constructors, and any future copy-construction
  // shapes flow through unchanged.
  Address destAddr = subCgf.makeNaturalAddressForPointer(
      entry->getArgument(0), catchParam.getType(), clang::CharUnits::Zero());
  subCgf.emitAggExpr(
      copyExpr, AggValueSlot::forAddr(
                    destAddr, Qualifiers(), AggValueSlot::IsNotDestructed,
                    AggValueSlot::IsNotAliased, AggValueSlot::DoesNotOverlap));

  cir::ReturnOp::create(builder, loc);
  return thunk;
}

/// A "special initializer" callback for initializing a catch
/// parameter during catch initialization.
static void initCatchParam(CIRGenFunction &cgf, CIRGenBuilderTy &builder,
                           mlir::Value ehToken, const VarDecl &catchParam,
                           SourceLocation loc) {
  CanQualType catchType =
      cgf.cgm.getASTContext().getCanonicalType(catchParam.getType());
  cir::InitCatchKind kind;

  if (isa<ReferenceType>(catchType)) {
    QualType caughtType = cast<ReferenceType>(catchType)->getPointeeType();
    const PointerType *ptr = dyn_cast<PointerType>(caughtType);
    if (!ptr)
      kind = cir::InitCatchKind::Reference;
    else if (ptr->getPointeeType()->isRecordType())
      kind = cir::InitCatchKind::ReferenceToRecordPointer;
    else
      kind = cir::InitCatchKind::ReferenceToPointer;
  } else {
    cir::TypeEvaluationKind tek = cgf.getEvaluationKind(catchType);
    if (tek == cir::TEK_Aggregate) {
      assert(isa<RecordType>(catchType) && "unexpected catch type!");
      const Expr *copyExpr = catchParam.getInit();
      kind = !copyExpr ? cir::InitCatchKind::TrivialCopy
                       : cir::InitCatchKind::NonTrivialCopy;
    } else {
      // Scalars and complexes.
      if (catchType->hasPointerRepresentation()) {
        switch (catchType.getQualifiers().getObjCLifetime()) {
        case Qualifiers::OCL_Weak:
        case Qualifiers::OCL_Strong:
          kind = cir::InitCatchKind::Objc;
          break;

        case Qualifiers::OCL_ExplicitNone:
        case Qualifiers::OCL_Autoreleasing:
        case Qualifiers::OCL_None:
          kind = cir::InitCatchKind::Pointer;
          break;
        }
      } else {
        kind = cir::InitCatchKind::Scalar;
      }
    }
  }

  CIRGenFunction::AutoVarEmission var = cgf.emitAutoVarAlloca(catchParam);
  Address paramAddr = var.getAllocatedAddress();
  mlir::Location mloc = cgf.getLoc(loc);

  if (kind == cir::InitCatchKind::NonTrivialCopy ||
      kind == cir::InitCatchKind::ReferenceToPointer) {
    // Sanitizer-checked construction (UBSan vptr/derived-class checks, etc.)
    // would require additional adornments that cir.construct_catch_param does
    // not yet carry.
    assert(!cir::MissingFeatures::sanitizers());

    mlir::FlatSymbolRefAttr copyFun{};
    if (kind == cir::InitCatchKind::NonTrivialCopy) {
      auto paramAddrType =
          mlir::cast<cir::PointerType>(paramAddr.getPointer().getType());
      cir::FuncOp thunk =
          getOrCreateCopyThunk(cgf, catchParam, paramAddrType, loc);
      copyFun = mlir::FlatSymbolRefAttr::get(thunk.getSymNameAttr());
    }

    cir::ConstructCatchParamOp::create(builder, mloc, ehToken,
                                       paramAddr.getPointer(), kind, copyFun);
  }

  const bool isMSVC = cgf.cgm.getTarget().getCXXABI().isMicrosoft();
  mlir::Value param{};
  cir::GlobalViewAttr typeAttr{};
  std::optional<uint32_t> flags = std::nullopt;
  if (isMSVC) {
    CatchTypeInfo typeInfo = cgf.cgm.getCXXABI().getAddrOfCXXCatchHandlerType(
        mloc, catchParam.getType().getNonReferenceType(), catchParam.getType());
    param = paramAddr.getPointer();
    typeAttr = mlir::dyn_cast_or_null<cir::GlobalViewAttr>(typeInfo.rtti);
    flags = typeInfo.flags;
  }
  mlir::Value exnPtr = callBeginCatch(cgf, ehToken, builder.getVoidPtrTy(),
                                      param, typeAttr, flags)
                           .getExnPtr();
  cir::InitCatchParamOp::create(builder, mloc, exnPtr, paramAddr.getPointer(),
                                kind);
  cgf.emitAutoVarCleanups(var);
}

/// Begins a catch statement by initializing the catch variable and
/// calling __cxa_begin_catch.
void CIRGenFunction::emitBeginCatch(const CXXCatchStmt *catchStmt,
                                    mlir::Value ehToken) {
  // We have to be very careful with the ordering of cleanups here:
  //   C++ [except.throw]p4:
  //     The destruction [of the exception temporary] occurs
  //     immediately after the destruction of the object declared in
  //     the exception-declaration in the handler.
  //
  // So the precise ordering is:
  //   1.  Construct catch variable.
  //   2.  begin_catch
  //   3.  Enter CallEndCatch cleanup
  //   4.  Enter dtor cleanup
  //
  VarDecl *catchParam = catchStmt->getExceptionDecl();
  const bool isMSVC = cgm.getTarget().getCXXABI().isMicrosoft();
  if (!catchParam) {
    // Catch-all: catch (...)
    std::optional<uint32_t> flags =
        isMSVC ? std::optional<uint32_t>(64) : std::nullopt;
    callBeginCatch(*this, ehToken, builder.getVoidPtrTy(),
                   /*paramAddr=*/{}, /*typeInfo=*/{}, flags);
    return;
  }

  if (isMSVC && !catchParam->getDeclName()) {
    // Unnamed catch param: catch (int)
    mlir::Location catchLoc = getLoc(catchStmt->getCatchLoc());
    QualType caughtType = catchParam->getType();
    CatchTypeInfo typeInfo = cgm.getCXXABI().getAddrOfCXXCatchHandlerType(
        catchLoc, caughtType, catchStmt->getCaughtType());
    callBeginCatch(*this, ehToken, builder.getVoidPtrTy(),
                   /*paramAddr=*/{},
                   mlir::dyn_cast_or_null<cir::GlobalViewAttr>(typeInfo.rtti),
                   typeInfo.flags);
    return;
  }

  // Emit the local. Make sure the alloca's superseed the current scope, since
  // these are going to be consumed by `cir.catch`, which is not within the
  // current scope.
  initCatchParam(*this, builder, ehToken, *catchParam,
                 catchStmt->getBeginLoc());
}

mlir::LogicalResult CIRGenFunction::emitCXXTryStmt(
    const CXXTryStmt &s, cxxTryBodyEmitter &bodyCallback, bool isFnTryBlock) {
  mlir::Location loc = getLoc(s.getSourceRange());

  // Create a scope to hold try local storage for catch params.
  mlir::OpBuilder::InsertPoint scopeIP;
  cir::ScopeOp::create(
      builder, loc,
      /*scopeBuilder=*/[&](mlir::OpBuilder &b, mlir::Location loc) {
        scopeIP = builder.saveInsertionPoint();
      });

  // Set personality function if not already set
  auto funcOp = mlir::cast<cir::FuncOp>(curFn);
  if (!funcOp.getPersonality())
    funcOp.setPersonality(getPersonalityFn(cgm, getEHPersonality(*this)));

  mlir::OpBuilder::InsertionGuard guard(builder);
  builder.restoreInsertionPoint(scopeIP);

  const llvm::Triple &t = getTarget().getTriple();
  // If we encounter a try statement on in an OpenMP target region offloaded
  // to a GPU, we treat it as a basic block.
  const bool isTargetDevice =
      (cgm.getLangOpts().OpenMPIsTargetDevice && (t.isNVPTX() || t.isAMDGCN()));
  if (isTargetDevice) {
    cgm.errorNYI("emitCXXTryStmt: OpenMP target region offloaded to GPU");
    return mlir::success();
  }

  mlir::Location tryLoc = getLoc(s.getBeginLoc());
  SmallVector<mlir::Attribute> handlerAttrs;

  CIRGenFunction::LexicalScope tryBodyScope{*this, tryLoc,
                                            builder.getInsertionBlock()};

  if (getLangOpts().EHAsynch) {
    cgm.errorNYI("enterCXXTryStmt: EHAsynch");
    return mlir::failure();
  }

  // Create the try operation.
  mlir::LogicalResult tryRes = mlir::success();
  auto tryOp = cir::TryOp::create(
      builder, tryLoc,
      /*tryBuilder=*/
      [&](mlir::OpBuilder &b, mlir::Location loc) {
        // Create a RunCleanupsScope that allows us to apply any cleanups that
        // are created for statements within the try body before exiting the
        // try body.
        RunCleanupsScope tryBodyCleanups(*this);
        if (bodyCallback(*this).failed())
          tryRes = mlir::failure();
        tryBodyCleanups.forceCleanup();
        if (!builder.getBlock()->mightHaveTerminator() ||
            !builder.getBlock()->getTerminator())
          cir::YieldOp::create(builder, loc);
      },
      /*handlersBuilder=*/
      [&](mlir::OpBuilder &b, mlir::Location loc,
          mlir::OperationState &result) {
        mlir::OpBuilder::InsertionGuard guard(b);
        bool hasCatchAll = false;
        unsigned numHandlers = s.getNumHandlers();
        mlir::Type ehTokenTy = cir::EhTokenType::get(&getMLIRContext());
        for (unsigned i = 0; i != numHandlers; ++i) {
          const CXXCatchStmt *catchStmt = s.getHandler(i);
          if (!catchStmt->getExceptionDecl())
            hasCatchAll = true;
          mlir::Region *region = result.addRegion();
          builder.createBlock(region, /*insertPt=*/{}, {ehTokenTy}, {loc});
          addCatchHandlerAttr(catchStmt, handlerAttrs);
        }
        if (!hasCatchAll) {
          // Create unwind region.
          mlir::Region *region = result.addRegion();
          mlir::Block *unwindBlock =
              builder.createBlock(region, /*insertPt=*/{}, {ehTokenTy}, {loc});
          cir::ResumeOp::create(builder, loc, unwindBlock->getArgument(0));
          handlerAttrs.push_back(cir::UnwindAttr::get(&getMLIRContext()));
        }
      });

  if (tryRes.failed())
    return mlir::failure();

  // Add final array of clauses into TryOp.
  tryOp.setHandlerTypesAttr(
      mlir::ArrayAttr::get(&getMLIRContext(), handlerAttrs));

  // Determine if we need an implicit rethrow for all these catch handlers;
  // see the comment below.
  const bool doImplicitRethrow =
      isFnTryBlock && (isa<CXXConstructorDecl>(curCodeDecl) ||
                       isa<CXXDestructorDecl>(curCodeDecl));

  // Emit the catch handler bodies. This has to be done after the try op is
  // created and in place so that we can find the insertion point for the
  // catch parameter alloca.
  unsigned numHandlers = s.getNumHandlers();
  for (unsigned i = 0; i != numHandlers; ++i) {
    const CXXCatchStmt *catchStmt = s.getHandler(i);
    mlir::Region *handler = &tryOp.getHandlerRegions()[i];
    mlir::Location handlerLoc = getLoc(catchStmt->getCatchLoc());

    mlir::OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToStart(&handler->front());

    // Get the !cir.eh_token block argument from the handler region.
    mlir::Value ehToken = handler->front().getArgument(0);

    // Enter a cleanup scope, including the catch variable and the
    // end-catch.
    RunCleanupsScope handlerScope(*this);

    // Initialize the catch variable.
    // TODO(cir): Move this out of CXXABI.
    assert(!cir::MissingFeatures::currentFuncletPad());
    emitBeginCatch(catchStmt, ehToken);

    // Emit the PGO counter increment.
    assert(!cir::MissingFeatures::incrementProfileCounter());

    // Perform the body of the catch.
    [[maybe_unused]] mlir::LogicalResult emitResult =
        emitStmt(catchStmt->getHandlerBlock(), /*useCurrentScope=*/true);
    assert(emitResult.succeeded() && "failed to emit catch handler block");

    // [except.handle]p11:
    //   The currently handled exception is rethrown if control
    //   reaches the end of a handler of the function-try-block of a
    //   constructor or destructor.
    //
    // It is important that we only do this on fallthrough and not on
    // return.  Note that it's illegal to put a return in a
    // constructor function-try-block's catch handler (p14), so this
    // really only applies to destructors.
    if (doImplicitRethrow && insertionPointIsReachable())
      cgm.getCXXABI().emitRethrow(*this, /*isNoReturn=*/true);

    // Fall out through the catch cleanups.
    handlerScope.forceCleanup();

    mlir::Block *block = &handler->getBlocks().back();
    if (block->empty() ||
        !block->back().hasTrait<mlir::OpTrait::IsTerminator>()) {
      mlir::OpBuilder::InsertionGuard guard(builder);
      builder.setInsertionPointToEnd(block);
      builder.createYield(handlerLoc);
    }
  }

  return mlir::success();
}

mlir::LogicalResult CIRGenFunction::emitCXXTryStmt(const CXXTryStmt &s) {
  if (s.getTryBlock()->body_empty())
    return mlir::LogicalResult::success();

  struct simpleTryBodyEmitter final : cxxTryBodyEmitter {
    const clang::CXXTryStmt &s;
    simpleTryBodyEmitter(const clang::CXXTryStmt &s) : s(s) {}

    mlir::LogicalResult operator()(CIRGenFunction &cgf) override {
      return cgf.emitStmt(s.getTryBlock(), /*useCurrentScope=*/true);
    }
    ~simpleTryBodyEmitter() override = default;
  };

  simpleTryBodyEmitter emitter{s};

  return emitCXXTryStmt(s, emitter);
}

namespace {
struct SEHCaptureFinder : public ConstStmtVisitor<SEHCaptureFinder> {
  SmallVector<const VarDecl *, 4> captures;
  llvm::SmallPtrSet<const VarDecl *, 4> visited;

  void Visit(const Stmt *s) {
    if (!s)
      return;
    ConstStmtVisitor<SEHCaptureFinder>::Visit(s);
    for (const Stmt *child : s->children())
      if (child)
        Visit(child);
  }

  void VisitDeclRefExpr(const DeclRefExpr *e) {
    if (const auto *vd = dyn_cast<VarDecl>(e->getDecl())) {
      if (vd->isLocalVarDeclOrParm() && vd->hasLocalStorage()) {
        if (visited.insert(vd).second)
          captures.push_back(vd);
      }
    }
  }
};

static void recoverCapturedLocals(CIRGenFunction &subCgf,
                                  CIRGenFunction &parentCGF,
                                  const SEHCaptureFinder &finder,
                                  mlir::Value framePtrArg, mlir::Location loc) {
  CIRGenBuilderTy &builder = subCgf.getBuilder();
  for (const VarDecl *vd : finder.captures) {
    auto it = parentCGF.localDeclMap.find(vd);
    if (it == parentCGF.localDeclMap.end())
      continue;
    Address parentAlloca = it->second;

    int idx = -1;
    for (size_t i = 0; i < parentCGF.escapedLocals.size(); ++i) {
      if (parentCGF.escapedLocals[i].first == vd) {
        idx = static_cast<int>(i);
        break;
      }
    }
    if (idx == -1) {
      idx = static_cast<int>(parentCGF.escapedLocals.size());
      parentCGF.escapedLocals.push_back({vd, parentAlloca});
    }

    auto parentFn = mlir::cast<cir::FuncOp>(parentCGF.curFn);
    auto parentSym = mlir::FlatSymbolRefAttr::get(parentFn.getSymNameAttr());
    auto gvAttr = cir::GlobalViewAttr::get(builder.getVoidPtrTy(), parentSym);
    auto parentPtr = cir::ConstantOp::create(builder, loc,
                                             builder.getVoidPtrTy(), gvAttr);
    auto idxVal = builder.getSInt32(idx, loc);
    auto recoveredVoidPtr = cir::LLVMIntrinsicCallOp::create(
        builder, loc, builder.getStringAttr("localrecover"),
        builder.getVoidPtrTy(),
        mlir::ValueRange{parentPtr, framePtrArg, idxVal});
    auto typedRecoveredPtr = builder.createBitcast(
        loc, recoveredVoidPtr.getResult(), parentAlloca.getPointer().getType());
    subCgf.setAddrOfLocalVar(
        vd, Address(typedRecoveredPtr, parentAlloca.getElementType(),
                    parentAlloca.getAlignment()));
  }
}
} // namespace

cir::FuncOp CIRGenFunction::GenerateSEHFinallyFunction(
    CIRGenFunction &parentCGF, const clang::SEHFinallyStmt &finally) {
  mlir::OpBuilder::InsertionGuard guard(builder);

  SmallString<128> name;
  {
    llvm::raw_svector_ostream os(name);
    GlobalDecl parentSEHFn = parentCGF.curSEHParent;
    if (!parentSEHFn) {
      parentSEHFn = parentCGF.curGD;
      parentCGF.curSEHParent = parentSEHFn;
    }
    assert(parentSEHFn && "No CurSEHParent!");
    MangleContext &mangler = cgm.getCXXABI().getMangleContext();
    mangler.mangleSEHFinallyBlock(parentSEHFn, os);
  }

  mlir::Location loc = getLoc(finally.getBeginLoc());
  auto fnType = cir::FuncType::get(
      {builder.getUInt8Ty(), builder.getVoidPtrTy()}, builder.getVoidTy());

  builder.setInsertionPoint(parentCGF.curFn);
  cir::FuncOp fn = cir::FuncOp::create(builder, loc, name, fnType);
  fn.setLinkage(cir::GlobalLinkageKind::InternalLinkage);
  fn.setSymVisibility("private");
  fn->setAttr(cir::CIRDialect::getNoThrowAttrName(), builder.getUnitAttr());
  fn->setAttr(cir::CIRDialect::getNoUnwindAttrName(), builder.getUnitAttr());
  cgm.insertGlobalSymbol(fn);

  mlir::Block *entry = fn.addEntryBlock();

  CIRGenFunction subCgf(cgm, builder, /*suppressNewContext=*/true);
  subCgf.curFn = fn;
  subCgf.isOutlinedSEHHelper = true;
  subCgf.curSEHParent = parentCGF.curSEHParent;
  subCgf.curCodeDecl = parentCGF.curCodeDecl;

  builder.setInsertionPointToStart(entry);

  mlir::Value framePtrArg = entry->getArgument(1);

  CIRGenFunction::SourceLocRAIIObject helperLoc(subCgf,
                                                finally.getSourceRange());
  CIRGenFunction::LexicalScope helperScope(subCgf, loc, entry);
  SymTableScopeTy varScope(subCgf.symbolTable);

  SEHCaptureFinder finder;
  finder.Visit(finally.getBlock());
  recoverCapturedLocals(subCgf, parentCGF, finder, framePtrArg, loc);

  (void)subCgf.emitStmt(finally.getBlock(), /*useCurrentScope=*/true);

  mlir::Block *curBlock = builder.getBlock();
  if (curBlock &&
      (!curBlock->mightHaveTerminator() || !curBlock->getTerminator())) {
    cir::ReturnOp::create(builder, loc);
  }

  return fn;
}

cir::FuncOp CIRGenFunction::GenerateSEHFilterFunction(
    CIRGenFunction &parentCGF, const clang::SEHExceptStmt &except) {
  mlir::OpBuilder::InsertionGuard guard(builder);

  SmallString<128> name;
  {
    llvm::raw_svector_ostream os(name);
    GlobalDecl parentSEHFn = parentCGF.curSEHParent;
    if (!parentSEHFn) {
      parentSEHFn = parentCGF.curGD;
      parentCGF.curSEHParent = parentSEHFn;
    }
    assert(parentSEHFn && "No CurSEHParent!");
    MangleContext &mangler = cgm.getCXXABI().getMangleContext();
    mangler.mangleSEHFilterExpression(parentSEHFn, os);
  }

  mlir::Location loc = getLoc(except.getBeginLoc());
  cir::FuncType fnType;
  if (cgm.getTarget().getTriple().getArch() != llvm::Triple::x86) {
    fnType = cir::FuncType::get(
        {builder.getVoidPtrTy(), builder.getVoidPtrTy()},
        builder.getSInt32Ty());
  } else {
    fnType = cir::FuncType::get({}, builder.getSInt32Ty());
  }

  builder.setInsertionPoint(parentCGF.curFn);
  cir::FuncOp fn = cir::FuncOp::create(builder, loc, name, fnType);
  fn.setLinkage(cir::GlobalLinkageKind::InternalLinkage);
  fn.setSymVisibility("private");
  fn->setAttr(cir::CIRDialect::getNoThrowAttrName(), builder.getUnitAttr());
  fn->setAttr(cir::CIRDialect::getNoUnwindAttrName(), builder.getUnitAttr());
  cgm.insertGlobalSymbol(fn);

  mlir::Block *entry = fn.addEntryBlock();

  CIRGenFunction subCgf(cgm, builder, /*suppressNewContext=*/true);
  subCgf.curFn = fn;
  subCgf.isOutlinedSEHHelper = true;
  subCgf.curSEHParent = parentCGF.curSEHParent;
  subCgf.curCodeDecl = parentCGF.curCodeDecl;

  builder.setInsertionPointToStart(entry);

  const Expr *filterExpr = except.getFilterExpr();
  CIRGenFunction::SourceLocRAIIObject helperLoc(subCgf,
                                                filterExpr->getSourceRange());
  CIRGenFunction::LexicalScope helperScope(subCgf, loc, entry);
  SymTableScopeTy varScope(subCgf.symbolTable);

  SEHCaptureFinder finder;
  finder.Visit(filterExpr);

  if (cgm.getTarget().getTriple().getArch() != llvm::Triple::x86) {
    mlir::Value exnPointers = entry->getArgument(0);
    mlir::Value framePtrArg = entry->getArgument(1);
    subCgf.sehInfo = exnPointers;
    recoverCapturedLocals(subCgf, parentCGF, finder, framePtrArg, loc);

    Address codeSlot = subCgf.createTempAlloca(
        builder.getSInt32Ty(), CharUnits::fromQuantity(4), loc,
        "__exception_code");
    subCgf.sehCodeSlotStack.push_back(codeSlot.getPointer());

    auto voidPtrPtrTy = builder.getPointerTo(builder.getVoidPtrTy());
    mlir::Value bitcastedExnPointers =
        builder.createBitcast(loc, exnPointers, voidPtrPtrTy);
    mlir::Value recPtr =
        builder.createLoad(loc, bitcastedExnPointers).getResult();
    auto s32PtrTy = builder.getPointerTo(builder.getSInt32Ty());
    mlir::Value bitcastedRecPtr = builder.createBitcast(loc, recPtr, s32PtrTy);
    mlir::Value code = builder.createLoad(loc, bitcastedRecPtr).getResult();
    builder.createStore(loc, code, codeSlot);
  }

  mlir::Value filterVal = subCgf.emitScalarExpr(filterExpr);
  mlir::Value intVal =
      builder.createBoolIntToIntCast(filterVal, builder.getSInt32Ty());
  cir::ReturnOp::create(builder, loc, intVal);

  if (!subCgf.sehCodeSlotStack.empty())
    subCgf.sehCodeSlotStack.pop_back();

  return fn;
}

mlir::LogicalResult CIRGenFunction::emitSEHTryStmt(const SEHTryStmt &s) {
  if (const SEHFinallyStmt *finally = s.getFinallyHandler()) {
    // Set personality function for SEH if not already set.
    auto funcOp = mlir::cast<cir::FuncOp>(curFn);
    if (!funcOp.getPersonality()) {
      const llvm::Triple &triple = cgm.getTarget().getTriple();
      const EHPersonality &personality = getSEHPersonalityMSVC(triple);
      funcOp.setPersonality(getPersonalityFn(cgm, personality));
    }

    cir::FuncOp finallyFn = GenerateSEHFinallyFunction(*this, *finally);

    mlir::Location tryLoc = getLoc(s.getBeginLoc());
    Address abnormalSlot = createMemTempWithoutCast(
        getContext().UnsignedCharTy, tryLoc, "abnormal_termination.slot");
    mlir::Value one = builder.getConstantInt(tryLoc, builder.getUInt8Ty(), 1);
    builder.createStore(tryLoc, one, abnormalSlot);

    sehFinallyAbnormalSlotStack.push_back(abnormalSlot);

    mlir::LogicalResult tryRes = mlir::success();

    cir::CleanupScopeOp::create(
        builder, tryLoc, cir::CleanupKind::All,
        /*bodyBuilder=*/
        [&](mlir::OpBuilder &b, mlir::Location loc) {
          mlir::Region *bodyRegion = builder.getBlock()->getParent();
          mlir::Block *bodyEntryBlock = builder.getBlock();
          mlir::Block *bodyExitBlock = builder.createBlock(bodyRegion);
          builder.setInsertionPointToEnd(bodyEntryBlock);

          sehTryEpilogueStack.push_back(bodyExitBlock);

          RunCleanupsScope tryBodyCleanups(*this);
          if (emitStmt(s.getTryBlock(), /*useCurrentScope=*/true).failed())
            tryRes = mlir::failure();
          tryBodyCleanups.forceCleanup();

          if (!builder.getBlock()->mightHaveTerminator() ||
              !builder.getBlock()->getTerminator()) {
            mlir::Value zero = builder.getConstantInt(loc, builder.getUInt8Ty(), 0);
            builder.createStore(loc, zero, abnormalSlot);
            cir::BrOp::create(builder, loc, bodyExitBlock);
          }

          builder.setInsertionPointToEnd(bodyExitBlock);
          if (!bodyExitBlock->mightHaveTerminator() ||
              !bodyExitBlock->getTerminator()) {
            cir::YieldOp::create(builder, loc);
          }

          sehTryEpilogueStack.pop_back();
        },
        /*cleanupBuilder=*/
        [&](mlir::OpBuilder &b, mlir::Location loc) {
          mlir::Value abnormalTerm = builder.createLoad(loc, abnormalSlot);
          auto framePtr = cir::LLVMIntrinsicCallOp::create(
              builder, loc, builder.getStringAttr("localaddress"),
              builder.getVoidPtrTy());
          cir::CallOp callOp = cir::CallOp::create(
              builder, loc,
              mlir::FlatSymbolRefAttr::get(finallyFn.getSymNameAttr()),
              builder.getVoidTy(),
              mlir::ValueRange{abnormalTerm, framePtr.getResult()});
          callOp->setAttr(cir::CIRDialect::getNoThrowAttrName(),
                          builder.getUnitAttr());
          callOp->setAttr(cir::CIRDialect::getNoUnwindAttrName(),
                          builder.getUnitAttr());
          cir::YieldOp::create(builder, loc);
        });

    sehFinallyAbnormalSlotStack.pop_back();
    return tryRes;
  }

  const SEHExceptStmt *exceptStmt = s.getExceptHandler();
  if (!exceptStmt) {
    cgm.errorNYI(s.getSourceRange(), "emitSEHTryStmt without __except");
    return mlir::failure();
  }

  // Set personality function for SEH if not already set.
  auto funcOp = mlir::cast<cir::FuncOp>(curFn);
  if (!funcOp.getPersonality())
    funcOp.setPersonality(getPersonalityFn(cgm, getEHPersonality(*this)));

  mlir::Location tryLoc = getLoc(s.getBeginLoc());

  // Allocate a 32-bit int slot for __exception_code.
  Address codeSlotAddr =
      createMemTempWithoutCast(getContext().IntTy, tryLoc, "__exception_code");
  mlir::Value codeSlot = codeSlotAddr.getPointer();
  sehCodeSlotStack.push_back(codeSlot);

  bool isConstantOne = false;
  if (cgm.getTarget().getTriple().getArch() != llvm::Triple::x86) {
    Expr::EvalResult eval;
    if (exceptStmt->getFilterExpr()->EvaluateAsInt(eval, getContext())) {
      if (eval.Val.isInt() && eval.Val.getInt() == 1)
        isConstantOne = true;
    }
  }

  cir::FuncOp filterFn;
  cir::GlobalViewAttr filterAttr;
  if (!isConstantOne) {
    filterFn = GenerateSEHFilterFunction(*this, *exceptStmt);
    filterAttr = builder.getGlobalViewAttr(builder.getVoidPtrTy(), filterFn);
  }

  SmallVector<mlir::Attribute> handlerAttrs;
  mlir::LogicalResult tryRes = mlir::success();

  auto tryOp = cir::TryOp::create(
      builder, tryLoc,
      /*tryBuilder=*/
      [&](mlir::OpBuilder &b, mlir::Location loc) {
        mlir::Region *tryRegion = builder.getBlock()->getParent();
        mlir::Block *tryEntryBlock = builder.getBlock();
        mlir::Block *tryExitBlock = builder.createBlock(tryRegion);
        builder.setInsertionPointToEnd(tryEntryBlock);

        sehTryEpilogueStack.push_back(tryExitBlock);

        RunCleanupsScope tryBodyCleanups(*this);
        if (emitStmt(s.getTryBlock(), /*useCurrentScope=*/true).failed())
          tryRes = mlir::failure();
        tryBodyCleanups.forceCleanup();

        if (!builder.getBlock()->mightHaveTerminator() ||
            !builder.getBlock()->getTerminator()) {
          cir::BrOp::create(builder, loc, tryExitBlock);
        }

        builder.setInsertionPointToEnd(tryExitBlock);
        if (!tryExitBlock->mightHaveTerminator() ||
            !tryExitBlock->getTerminator()) {
          cir::YieldOp::create(builder, loc);
        }

        sehTryEpilogueStack.pop_back();
      },
      /*handlersBuilder=*/
      [&](mlir::OpBuilder &b, mlir::Location loc,
          mlir::OperationState &result) {
        mlir::OpBuilder::InsertionGuard guard(b);
        mlir::Type ehTokenTy = cir::EhTokenType::get(&getMLIRContext());
        mlir::Region *region = result.addRegion();
        builder.createBlock(region, /*insertPt=*/{}, {ehTokenTy}, {loc});
        handlerAttrs.push_back(cir::CatchAllAttr::get(&getMLIRContext()));
      });

  if (tryRes.failed()) {
    sehCodeSlotStack.pop_back();
    return mlir::failure();
  }

  tryOp.setHandlerTypesAttr(
      mlir::ArrayAttr::get(&getMLIRContext(), handlerAttrs));

  // Emit the __except handler body.
  mlir::Region *handler = &tryOp.getHandlerRegions()[0];
  mlir::Location handlerLoc = getLoc(exceptStmt->getExceptLoc());

  {
    mlir::OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToStart(&handler->front());

    mlir::Value ehToken = handler->front().getArgument(0);

    RunCleanupsScope handlerScope(*this);
    cir::BeginCatchOp beginCatch =
        isConstantOne
            ? callBeginCatch(*this, ehToken, builder.getVoidPtrTy(),
                             /*paramAddr=*/{}, /*typeInfo=*/{}, /*flags=*/0)
            : callBeginCatch(*this, ehToken, builder.getVoidPtrTy(),
                             /*paramAddr=*/{}, filterAttr, /*flags=*/0);

    if (cgm.getTarget().getTriple().getArch() != llvm::Triple::x86) {
      auto exnCode = cir::LLVMIntrinsicCallOp::create(
          builder, handlerLoc, builder.getStringAttr("eh.exceptioncode"),
          builder.getSInt32Ty(),
          mlir::ValueRange{beginCatch.getCatchToken()});
      builder.createStore(handlerLoc, exnCode.getResult(), codeSlotAddr);
    }

    if (emitStmt(exceptStmt->getBlock(), /*useCurrentScope=*/true).failed()) {
      sehCodeSlotStack.pop_back();
      return mlir::failure();
    }

    handlerScope.forceCleanup();

    mlir::Block *block = &handler->getBlocks().back();
    if (block->empty() ||
        !block->back().hasTrait<mlir::OpTrait::IsTerminator>()) {
      mlir::OpBuilder::InsertionGuard guard(builder);
      builder.setInsertionPointToEnd(block);
      builder.createYield(handlerLoc);
    }
  }

  sehCodeSlotStack.pop_back();
  return mlir::success();
}

bool CIRGenFunction::currentFunctionUsesSEHTry() const {
  const Decl *d = curGD.getDecl();
  if (!d)
    d = curCodeDecl;
  if (const auto *fd = dyn_cast_or_null<FunctionDecl>(d))
    return fd->usesSEHTry();
  return false;
}

// in classic codegen this function is mapping to `isInvokeDest` previously
// and currently it's mapping to the conditions that performs early returns in
// `getInvokeDestImpl`, in CIR we need the condition to know if the EH scope
// may throw exception or now.
bool CIRGenFunction::isCatchOrCleanupRequired() {
  // If exceptions are disabled/ignored and SEH is not in use, then there is
  // no invoke destination. SEH "works" even if exceptions are off. In
  // practice, this means that C++ destructors and other EH cleanups don't
  // run, which is consistent with MSVC's behavior, except in the presence of
  // -EHa
  const LangOptions &lo = cgm.getLangOpts();
  if (!lo.Exceptions || lo.IgnoreExceptions) {
    if (!lo.Borland && !lo.MicrosoftExt)
      return false;
    if (!currentFunctionUsesSEHTry())
      return false;
  }

  // CUDA device code doesn't have exceptions.
  if (lo.CUDA && lo.CUDAIsDevice)
    return false;

  return ehStack.requiresCatchOrCleanup();
}
