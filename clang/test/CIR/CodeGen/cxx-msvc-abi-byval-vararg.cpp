// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -Wno-non-pod-varargs -emit-cir %s -o - | FileCheck %s --check-prefix=X64
// RUN: %clang_cc1 -triple i686-pc-windows-msvc -Wno-non-pod-varargs -emit-cir %s -o - | FileCheck %s --check-prefix=X86

#include <stdarg.h>

struct A {
  A(int a) : a(a) {}
  A(const A &o) : a(o.a) {}
  ~A() {}
  int a;
};

int foo(A a, ...) {
  va_list ap;
  va_start(ap, a);
  int sum = 0;
  for (int i = 0; i < a.a; ++i)
    sum += va_arg(ap, int);
  va_end(ap);
  return sum;
}

// X64-LABEL: cir.func no_inline dso_local @"?foo@@YAHUA@@ZZ"(%arg0: !cir.ptr<!rec_A>
// X64:   cir.va_start
// X64:   cir.va_arg
// X64:   cir.va_end
// X64:   cleanup normal
// X64:   cir.call @"??1A@@QEAA@XZ"(%arg0)

// X86-LABEL: cir.func no_inline dso_local @"?foo@@YAHUA@@ZZ"(%arg0: !rec_A
// X86:   cir.va_start
// X86:   cir.va_arg
// X86:   cir.va_end
// X86:   cleanup normal
// X86:   cir.call @"??1A@@QAE@XZ"(%0) nothrow cc(x86_thiscall)

// X64-DAG: cir.func no_inline comdat linkonce_odr dso_local @"??1A@@QEAA@XZ"(
// X86-DAG: cir.func no_inline comdat linkonce_odr dso_local @"??1A@@QAE@XZ"(

int main() {
  return foo(A(3), 1, 2, 3);
}

// X64-LABEL: cir.func no_inline dso_local @main()
// X64:   cir.call @"??0A@@QEAA@H@Z"
// X64:   cir.call @"?foo@@YAHUA@@ZZ"(%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}})

// X86-LABEL: cir.func no_inline dso_local @main()
// X86:   cir.call @"??0A@@QAE@H@Z"
// X86:   cir.call @"?foo@@YAHUA@@ZZ"(%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}})

// X64-DAG: cir.func no_inline comdat linkonce_odr dso_local @"??0A@@QEAA@H@Z"(
// X86-DAG: cir.func no_inline comdat linkonce_odr dso_local @"??0A@@QAE@H@Z"(

void varargs_zero(...);
void varargs_one(int, ...);
void varargs_two(int, int, ...);
void varargs_three(int, int, int, ...);
void call_var_args() {
  A x(3);
  varargs_zero(x);
  varargs_one(1, x);
  varargs_two(1, 2, x);
  varargs_three(1, 2, 3, x);
}

// X64-LABEL: cir.func no_inline dso_local @"?call_var_args@@YAXXZ"()
// X64:   cir.call @"??0A@@QEAA@H@Z"
// X64:   cir.trap
// X64:   cleanup normal
// X64:   cir.call @"??1A@@QEAA@XZ"

// X86-LABEL: cir.func no_inline dso_local @"?call_var_args@@YAXXZ"()
// X86:   cir.call @"??0A@@QAE@H@Z"
// X86:   cir.trap
// X86:   cleanup normal
// X86:   cir.call @"??1A@@QAE@XZ"

// X64-DAG: cir.func private @"?varargs_zero@@YAXZZ"(...)
// X64-DAG: cir.func private @"?varargs_one@@YAXHZZ"(!s32i
// X64-DAG: cir.func private @"?varargs_two@@YAXHHZZ"(!s32i
// X64-DAG: cir.func private @"?varargs_three@@YAXHHHZZ"(!s32i
// X64-DAG: cir.func no_inline comdat linkonce_odr dso_local @"??0A@@QEAA@AEBU0@@Z"(

// X86-DAG: cir.func private @"?varargs_zero@@YAXZZ"(...)
// X86-DAG: cir.func private @"?varargs_one@@YAXHZZ"(!s32i
// X86-DAG: cir.func private @"?varargs_two@@YAXHHZZ"(!s32i
// X86-DAG: cir.func private @"?varargs_three@@YAXHHHZZ"(!s32i
// X86-DAG: cir.func no_inline comdat linkonce_odr dso_local @"??0A@@QAE@ABU0@@Z"(
