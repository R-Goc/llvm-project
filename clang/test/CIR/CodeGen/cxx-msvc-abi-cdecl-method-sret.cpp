// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -emit-cir %s -o - | FileCheck %s --check-prefix=X64
// RUN: %clang_cc1 -triple i686-pc-windows-msvc -emit-cir %s -o - | FileCheck %s --check-prefix=X86

// X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_C@_04JIHMPGLA@asdf?$AA@" = #cir.const_array<"asdf" : !cir.array<!s8i x 4>, trailing_zeros> : !cir.array<!s8i x 5>
// X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_C@_04JIHMPGLA@asdf?$AA@" = #cir.const_array<"asdf" : !cir.array<!s8i x 4>, trailing_zeros> : !cir.array<!s8i x 5>

struct S {
  S();
  ~S();
  int x;
};

struct C {
  S variadic_sret(const char *f, ...);
  S __cdecl cdecl_sret();
  S __cdecl byval_and_sret(S a);
};

struct A {
  S __fastcall f(int x);
};

// ----------------------------------------------------------------------------
// Method definitions
// ----------------------------------------------------------------------------

// X64-LABEL: cir.func no_inline dso_local @"?variadic_sret@C@@QEAA?AUS@@PEBDZZ"(%arg0: !cir.ptr<!rec_C> {{.*}}, %arg1: !cir.ptr<!rec_S> {{.*}}, %arg2: !cir.ptr<!s8i> {{.*}}, ...)
// X86-LABEL: cir.func no_inline dso_local @"?variadic_sret@C@@QAA?AUS@@PBDZZ"(%arg0: !cir.ptr<!rec_C> {{.*}}, %arg1: !cir.ptr<!rec_S> {{.*}}, %arg2: !cir.ptr<!s8i> {{.*}}, ...)
S C::variadic_sret(const char *f, ...) { return S(); }

// X64-LABEL: cir.func no_inline dso_local @"?cdecl_sret@C@@QEAA?AUS@@XZ"(%arg0: !cir.ptr<!rec_C> {{.*}}, %arg1: !cir.ptr<!rec_S> {{.*}})
// X86-LABEL: cir.func no_inline dso_local @"?cdecl_sret@C@@QAA?AUS@@XZ"(%arg0: !cir.ptr<!rec_C> {{.*}}, %arg1: !cir.ptr<!rec_S> {{.*}})
S C::cdecl_sret() { return S(); }

// X64-LABEL: cir.func no_inline dso_local @"?byval_and_sret@C@@QEAA?AUS@@U2@@Z"(%arg0: !cir.ptr<!rec_C> {{.*}}, %arg1: !cir.ptr<!rec_S> {{.*}}, %arg2: !u32i {{.*}})
// X86-LABEL: cir.func no_inline dso_local @"?byval_and_sret@C@@QAA?AUS@@U2@@Z"(%arg0: !cir.ptr<!rec_C> {{.*}}, %arg1: !cir.ptr<!rec_S> {{.*}}, %arg2: !rec_S {{.*}})
S C::byval_and_sret(S a) { return S(); }

// X64-LABEL: cir.func no_inline dso_local @"?f@A@@QEAA?AUS@@H@Z"(%arg0: !cir.ptr<!rec_A> {{.*}}, %arg1: !cir.ptr<!rec_S> {{.*}}, %arg2: !s32i {{.*}})
// X86-LABEL: cir.func no_inline dso_local @"?f@A@@QAI?AUS@@H@Z"(%arg0: !cir.ptr<!rec_A> {{.*}}, %arg1: !cir.ptr<!rec_S> {{.*}}, %arg2: !s32i {{.*}}) cc(x86_fastcall)
S A::f(int x) { return S(); }

// ----------------------------------------------------------------------------
// Callers
// ----------------------------------------------------------------------------

// X64-LABEL: cir.func no_inline dso_local @"?test_variadic@@YAXAEAUC@@@Z"(%arg0: !cir.ptr<!rec_C>
// X64:         cir.call @"?variadic_sret@C@@QEAA?AUS@@PEBDZZ"(%{{.*}}, %{{.*}}, %{{.*}}) : (!cir.ptr<!rec_C> {{.*}}, !cir.ptr<!rec_S>, !cir.ptr<!s8i> {{.*}}) -> ()
// X86-LABEL: cir.func no_inline dso_local @"?test_variadic@@YAXAAUC@@@Z"(%arg0: !cir.ptr<!rec_C>
// X86:         cir.call @"?variadic_sret@C@@QAA?AUS@@PBDZZ"(%{{.*}}, %{{.*}}, %{{.*}}) : (!cir.ptr<!rec_C> {{.*}}, !cir.ptr<!rec_S>, !cir.ptr<!s8i> {{.*}}) -> ()
void test_variadic(C &c) {
  c.variadic_sret("asdf");
}

// X64-LABEL: cir.func no_inline dso_local @"?test_cdecl@@YAXAEAUC@@@Z"(%arg0: !cir.ptr<!rec_C>
// X64:         cir.call @"?cdecl_sret@C@@QEAA?AUS@@XZ"(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_C> {{.*}}, !cir.ptr<!rec_S>) -> ()
// X86-LABEL: cir.func no_inline dso_local @"?test_cdecl@@YAXAAUC@@@Z"(%arg0: !cir.ptr<!rec_C>
// X86:         cir.call @"?cdecl_sret@C@@QAA?AUS@@XZ"(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_C> {{.*}}, !cir.ptr<!rec_S>) -> ()
void test_cdecl(C &c) {
  c.cdecl_sret();
}

// X64-LABEL: cir.func no_inline dso_local @"?test_byval_and_sret@@YAXAEAUC@@@Z"(%arg0: !cir.ptr<!rec_C>
// X64:         cir.call @"?byval_and_sret@C@@QEAA?AUS@@U2@@Z"(%{{.*}}, %{{.*}}, %{{.*}}) : (!cir.ptr<!rec_C> {{.*}}, !cir.ptr<!rec_S>, !u32i) -> ()
// X86-LABEL: cir.func no_inline dso_local @"?test_byval_and_sret@@YAXAAUC@@@Z"(%arg0: !cir.ptr<!rec_C>
// X86:         cir.call @"?byval_and_sret@C@@QAA?AUS@@U2@@Z"(%{{.*}}, %{{.*}}, %{{.*}}) : (!cir.ptr<!rec_C> {{.*}}, !cir.ptr<!rec_S>, !rec_S) -> ()
void test_byval_and_sret(C &c) {
  c.byval_and_sret(S());
}

// X64-LABEL: cir.func no_inline dso_local @"?test_fastcall@@YAXAEAUA@@@Z"(%arg0: !cir.ptr<!rec_A>
// X64:         cir.call @"?f@A@@QEAA?AUS@@H@Z"(%{{.*}}, %{{.*}}, %{{.*}}) : (!cir.ptr<!rec_A> {{.*}}, !cir.ptr<!rec_S>, !s32i {{.*}}) -> ()
// X86-LABEL: cir.func no_inline dso_local @"?test_fastcall@@YAXAAUA@@@Z"(%arg0: !cir.ptr<!rec_A>
// X86:         cir.call @"?f@A@@QAI?AUS@@H@Z"(%{{.*}}, %{{.*}}, %{{.*}}) cc(x86_fastcall) : (!cir.ptr<!rec_A> {{.*}}, !cir.ptr<!rec_S>, !s32i {{.*}}) -> ()
void test_fastcall(A &a) {
  a.f(42);
}
