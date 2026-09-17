// RUN: %clang_cc1 -fms-extensions -std=c++17 -triple i386-pc-windows-msvc -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR

struct A {
  virtual void __fastcall f(int a, int b);
};
void (__fastcall A::*doit())(int, int) {
  return &A::f;
}

// CIR-LABEL: cir.func no_inline dso_local @"?doit@@YAP8A@@AIXHH@ZXZ"()
// CIR:         cir.const #cir.global_view<@"??_9A@@$BA@AI">

// CIR-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"??_9A@@$BA@AI"(%arg0: !cir.ptr<!rec_A>{{.*}}, ...) cc(x86_fastcall) attributes {{{.*}}thunk{{.*}}}
// CIR:         %[[VPTR:.*]] = cir.load
// CIR:         %[[SLOT:.*]] = cir.ptr_stride %[[VPTR]]
// CIR:         %[[CALLEE:.*]] = cir.load
// CIR:         %[[CALLEE_TYPED:.*]] = cir.cast bitcast %[[CALLEE]] : !cir.ptr<!void> -> !cir.ptr<!cir.func<(!cir.ptr<!rec_A>, ...)>>
// CIR:         cir.call %[[CALLEE_TYPED]](%arg0) musttail cc(x86_fastcall)
// CIR:         cir.return
