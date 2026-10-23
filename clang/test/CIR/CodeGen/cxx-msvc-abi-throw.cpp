// RUN: %clang_cc1 -triple i386-pc-windows-msvc -std=c++20 -fcxx-exceptions -fms-extensions -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s
// RUN: %clang_cc1 -triple i386-pc-windows-msvc -std=c++20 -fcxx-exceptions -fms-extensions -fclangir -emit-llvm %s -o - | FileCheck %s --check-prefix=LLVM

// Module-level global checks must appear before any CIR-LABEL / LLVM-LABEL directives.
// CIR-DAG: cir.global constant linkonce_odr comdat @"_TI5?AUY@@" = #cir.const_record<{#cir.int<0> : !s32i, #cir.global_view<@"??_DY@@QAEXXZ"> : !cir.ptr<!void>, #cir.ptr<null> : !cir.ptr<!void>, #cir.global_view<@"_CTA5?AUY@@"> : !cir.ptr<!void>}> : !rec_eh2EThrowInfo
// CIR-DAG: cir.global constant linkonce_odr comdat @"_CTA5?AUY@@" = {{.*}} : !rec_eh2ECatchableTypeArray2E5
// CIR-DAG: cir.global constant linkonce_odr comdat @"_CT??_R0?AUY@@@8??0Y@@QAE@ABU0@@Z8" = {{.*}} : !rec_eh2ECatchableType
// CIR-DAG: cir.global constant linkonce_odr comdat @"_CT??_R0?AUDefault@@@8??_ODefault@@QAEXAAU0@@Z1" = {{.*}} : !rec_eh2ECatchableType
// CIR-DAG: cir.global constant linkonce_odr comdat @"_CT??_R0?AUDeletedCopy@@@81" = {{.*}} : !rec_eh2ECatchableType
// CIR-DAG: cir.global constant linkonce_odr comdat @"_CT??_R0?AUVariadic@@@8??_OVariadic@@QAEXAAU0@@Z1" = {{.*}} : !rec_eh2ECatchableType
// CIR-DAG: cir.global constant linkonce_odr comdat @_CTA2$$T = {{.*}} : !rec_eh2ECatchableTypeArray2E2
// CIR-DAG: cir.global constant linkonce_odr comdat @_TI2$$T = {{.*}} : !rec_eh2EThrowInfo

// LLVM-DAG: @"_TI5?AUY@@" = linkonce_odr constant %struct.eh.ThrowInfo
// LLVM-DAG: @"_CTA5?AUY@@" = linkonce_odr constant %struct.eh.CatchableTypeArray.5
// LLVM-DAG: @"_CT??_R0?AUY@@@8??0Y@@QAE@ABU0@@Z8" = linkonce_odr constant %struct.eh.CatchableType
// LLVM-DAG: @"_CT??_R0?AUDefault@@@8??_ODefault@@QAEXAAU0@@Z1" = linkonce_odr constant %struct.eh.CatchableType
// LLVM-DAG: @"_CT??_R0?AUDeletedCopy@@@81" = linkonce_odr constant %struct.eh.CatchableType
// LLVM-DAG: @"_CT??_R0?AUVariadic@@@8??_OVariadic@@QAEXAAU0@@Z1" = linkonce_odr constant %struct.eh.CatchableType
// LLVM-DAG: @"_CTA2$$T" = linkonce_odr constant %struct.eh.CatchableTypeArray.2
// LLVM-DAG: @"_TI2$$T" = linkonce_odr constant %struct.eh.ThrowInfo

struct N { ~N(); };
struct M : private N {};
struct X {};
struct Z {};
struct V : private X {};
struct W : M, virtual V {};
struct Y : Z, W, virtual V {};

void f(const Y &y) {
  // CIR-LABEL: cir.func no_inline dso_local @"?f@@YAXABUY@@@Z"
  // CIR: cir.call @"??0Y@@QAE@ABU0@@Z"
  // CIR: cir.throw %{{.*}} : !cir.ptr<!rec_Y>, @"_TI5?AUY@@"
  // LLVM-LABEL: define dso_local void @"?f@@YAXABUY@@@Z"
  // LLVM: call x86_thiscallcc noundef ptr @"??0Y@@QAE@ABU0@@Z"
  // LLVM: call x86_stdcallcc void @_CxxThrowException(ptr %{{.*}}, ptr @"_TI5?AUY@@")
  throw y;
}

void g(const int *const *y) {
  // CIR-LABEL: cir.func no_inline dso_local @"?g@@YAXPBQBH@Z"
  // CIR: cir.throw %{{.*}}, @_TIC2PAPBH
  // LLVM-LABEL: define dso_local void @"?g@@YAXPBQBH@Z"
  // LLVM: call x86_stdcallcc void @_CxxThrowException(ptr %{{.*}}, ptr @_TIC2PAPBH)
  throw y;
}

struct Default {
  Default(Default &, int = 42);
};

// CIR-LABEL: cir.func comdat linkonce_odr private @"??_ODefault@@QAEXAAU0@@Z"
// CIR: cir.call @"??0Default@@QAE@AAU0@H@Z"
// CIR: cir.return
// LLVM-LABEL: define linkonce_odr x86_thiscallcc void @"??_ODefault@@QAEXAAU0@@Z"
// LLVM: call x86_thiscallcc noundef ptr @"??0Default@@QAE@AAU0@H@Z"({{.*}}, i32 noundef 42)
// LLVM: ret void

void h(Default &d) {
  throw d;
}

struct DeletedCopy {
  DeletedCopy();
  DeletedCopy(DeletedCopy &&);
  DeletedCopy(const DeletedCopy &) = delete;
};

void throwDeletedCopy() {
  // CIR-LABEL: cir.func no_inline dso_local @"?throwDeletedCopy@@YAXXZ"
  // CIR: cir.throw %{{.*}}, @"_TI1?AUDeletedCopy@@"
  // LLVM-LABEL: define dso_local void @"?throwDeletedCopy@@YAXXZ"
  // LLVM: call x86_stdcallcc void @_CxxThrowException(ptr %{{.*}}, ptr @"_TI1?AUDeletedCopy@@")
  throw DeletedCopy();
}

struct Variadic {
  Variadic(Variadic &, ...);
};

// CIR-LABEL: cir.func comdat linkonce_odr private @"??_OVariadic@@QAEXAAU0@@Z"
// CIR: cir.call @"??0Variadic@@QAA@AAU0@ZZ"
// CIR: cir.return
// LLVM-LABEL: define linkonce_odr x86_thiscallcc void @"??_OVariadic@@QAEXAAU0@@Z"
// LLVM: call noundef ptr (ptr, ptr, ...) @"??0Variadic@@QAA@AAU0@ZZ"
// LLVM: ret void

void i(Variadic &v) {
  throw v;
}

void throwNullptr() {
  // CIR-LABEL: cir.func no_inline dso_local @"?throwNullptr@@YAXXZ"
  // CIR: cir.throw %{{.*}}, @_TI2$$T
  // LLVM-LABEL: define dso_local void @"?throwNullptr@@YAXXZ"
  // LLVM: call x86_stdcallcc void @_CxxThrowException(ptr %{{.*}}, ptr @"_TI2$$T")
  throw nullptr;
}

void testRethrow() {
  // CIR-LABEL: cir.func no_inline dso_local @"?testRethrow@@YAXXZ"
  // CIR: cir.throw loc(
  // LLVM-LABEL: define dso_local void @"?testRethrow@@YAXXZ"
  // LLVM: call x86_stdcallcc void @_CxxThrowException(ptr null, ptr null)
  throw;
}
