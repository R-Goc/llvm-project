// RUN: %clang_cc1 -std=c++17 -triple i386-pc-windows-msvc -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR32
// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fclangir -emit-cir %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir %s -check-prefix=CIR64

// In each test case, two member pointers have the same vtable offset and mangling,
// but their prototypes conflict. ClangIR emits an unprototyped musttail thunk ("??_9...")
// that forwards %this and all variadic arguments to the loaded virtual function pointer.

namespace num_params {
struct A { virtual void a(int); };
struct B { virtual void b(int, int); };
struct C : A, B {
  virtual void a(int);
  virtual void b(int, int);
};
void f(C *c) {
  (c->*(&C::a))(0);
  (c->*(&C::b))(0, 0);
}
}

// CIR32-LABEL: cir.func no_inline dso_local @"?f@num_params@@YAXPAUC@1@@Z"
// CIR32:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@num_params@@$BA@AE"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR32:         cir.call %{{[0-9]+}}(%{{[0-9]+}}, %{{[0-9]+}}) cc(x86_thiscall)
// CIR32:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@num_params@@$BA@AE"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR32:         cir.call %{{[0-9]+}}(%{{[0-9]+}}, %{{[0-9]+}}, %{{[0-9]+}}) cc(x86_thiscall)

// CIR32-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"??_9C@num_params@@$BA@AE"(%arg0: !cir.ptr<!rec_num_params3A3AC>{{.*}}, ...) cc(x86_thiscall) attributes {{{.*}}thunk{{.*}}}
// CIR32:         %[[VTABLE:.*]] = cir.load
// CIR32:         %[[SLOT:.*]] = cir.ptr_stride %[[VTABLE]]
// CIR32:         %[[CALLEE:.*]] = cir.load
// CIR32:         %[[CALLEE_TYPED:.*]] = cir.cast bitcast %[[CALLEE]] : !cir.ptr<!void> -> !cir.ptr<!cir.func<(!cir.ptr<!rec_num_params3A3AC>, ...)>>
// CIR32:         cir.call %[[CALLEE_TYPED]](%arg0) musttail cc(x86_thiscall)
// CIR32:         cir.return

// CIR64-LABEL: cir.func no_inline dso_local @"?f@num_params@@YAXPEAUC@1@@Z"
// CIR64:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@num_params@@$BA@AA"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR64:         cir.call %{{[0-9]+}}(%{{[0-9]+}}, %{{[0-9]+}})
// CIR64:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@num_params@@$BA@AA"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR64:         cir.call %{{[0-9]+}}(%{{[0-9]+}}, %{{[0-9]+}}, %{{[0-9]+}})

// CIR64-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"??_9C@num_params@@$BA@AA"(%arg0: !cir.ptr<!rec_num_params3A3AC>{{.*}}, ...) attributes {{{.*}}thunk{{.*}}}
// CIR64:         %[[VTABLE:.*]] = cir.load
// CIR64:         %[[SLOT:.*]] = cir.ptr_stride %[[VTABLE]]
// CIR64:         %[[CALLEE:.*]] = cir.load
// CIR64:         %[[CALLEE_TYPED:.*]] = cir.cast bitcast %[[CALLEE]] : !cir.ptr<!void> -> !cir.ptr<!cir.func<(!cir.ptr<!rec_num_params3A3AC>, ...)>>
// CIR64:         cir.call %[[CALLEE_TYPED]](%arg0) musttail
// CIR64:         cir.return

namespace i64_return {
struct A { virtual int a(); };
struct B { virtual long long b(); };
struct C : A, B {
  virtual int a();
  virtual long long b();
};
long long f(C *c) {
  int x = (c->*(&C::a))();
  long long y = (c->*(&C::b))();
  return x + y;
}
}

// CIR32-LABEL: cir.func no_inline dso_local @"?f@i64_return@@YA_JPAUC@1@@Z"
// CIR32:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@i64_return@@$BA@AE"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR32:         cir.call %{{[0-9]+}}(%{{[0-9]+}}) cc(x86_thiscall)
// CIR32:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@i64_return@@$BA@AE"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR32:         cir.call %{{[0-9]+}}(%{{[0-9]+}}) cc(x86_thiscall)

// CIR32-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"??_9C@i64_return@@$BA@AE"(%arg0: !cir.ptr<!rec_i64_return3A3AC>{{.*}}, ...) cc(x86_thiscall) attributes {{{.*}}thunk{{.*}}}
// CIR32:         %[[VTABLE:.*]] = cir.load
// CIR32:         %[[SLOT:.*]] = cir.ptr_stride %[[VTABLE]]
// CIR32:         %[[CALLEE:.*]] = cir.load
// CIR32:         %[[CALLEE_TYPED:.*]] = cir.cast bitcast %[[CALLEE]] : !cir.ptr<!void> -> !cir.ptr<!cir.func<(!cir.ptr<!rec_i64_return3A3AC>, ...)>>
// CIR32:         cir.call %[[CALLEE_TYPED]](%arg0) musttail cc(x86_thiscall)
// CIR32:         cir.return

// CIR64-LABEL: cir.func no_inline dso_local @"?f@i64_return@@YA_JPEAUC@1@@Z"
// CIR64:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@i64_return@@$BA@AA"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR64:         cir.call %{{[0-9]+}}(%{{[0-9]+}})
// CIR64:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@i64_return@@$BA@AA"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR64:         cir.call %{{[0-9]+}}(%{{[0-9]+}})

// CIR64-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"??_9C@i64_return@@$BA@AA"(%arg0: !cir.ptr<!rec_i64_return3A3AC>{{.*}}, ...) attributes {{{.*}}thunk{{.*}}}
// CIR64:         %[[VTABLE:.*]] = cir.load
// CIR64:         %[[SLOT:.*]] = cir.ptr_stride %[[VTABLE]]
// CIR64:         %[[CALLEE:.*]] = cir.load
// CIR64:         %[[CALLEE_TYPED:.*]] = cir.cast bitcast %[[CALLEE]] : !cir.ptr<!void> -> !cir.ptr<!cir.func<(!cir.ptr<!rec_i64_return3A3AC>, ...)>>
// CIR64:         cir.call %[[CALLEE_TYPED]](%arg0) musttail
// CIR64:         cir.return

namespace sret {
struct Big { int big[32]; };
struct A { virtual int a(); };
struct B { virtual Big b(); };
struct C : A, B {
  virtual int a();
  virtual Big b();
};
void f(C *c) {
  (c->*(&C::a))();
  Big b((c->*(&C::b))());
}
}

// CIR32-LABEL: cir.func no_inline dso_local @"?f@sret@@YAXPAUC@1@@Z"
// CIR32:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@sret@@$BA@AE"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR32:         cir.call %{{[0-9]+}}(%{{[0-9]+}}) cc(x86_thiscall)
// CIR32:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@sret@@$BA@AE"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR32:         cir.call %{{[0-9]+}}(%{{[0-9]+}}, %{{[0-9]+}}) cc(x86_thiscall)

// CIR32-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"??_9C@sret@@$BA@AE"(%arg0: !cir.ptr<!rec_sret3A3AC>{{.*}}, ...) cc(x86_thiscall) attributes {{{.*}}thunk{{.*}}}
// CIR32:         %[[VTABLE:.*]] = cir.load
// CIR32:         %[[SLOT:.*]] = cir.ptr_stride %[[VTABLE]]
// CIR32:         %[[CALLEE:.*]] = cir.load
// CIR32:         %[[CALLEE_TYPED:.*]] = cir.cast bitcast %[[CALLEE]] : !cir.ptr<!void> -> !cir.ptr<!cir.func<(!cir.ptr<!rec_sret3A3AC>, ...)>>
// CIR32:         cir.call %[[CALLEE_TYPED]](%arg0) musttail cc(x86_thiscall)
// CIR32:         cir.return

// CIR64-LABEL: cir.func no_inline dso_local @"?f@sret@@YAXPEAUC@1@@Z"
// CIR64:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@sret@@$BA@AA"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR64:         cir.call %{{[0-9]+}}(%{{[0-9]+}})
// CIR64:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@sret@@$BA@AA"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR64:         cir.call %{{[0-9]+}}(%{{[0-9]+}}, %{{[0-9]+}})

// CIR64-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"??_9C@sret@@$BA@AA"(%arg0: !cir.ptr<!rec_sret3A3AC>{{.*}}, ...) attributes {{{.*}}thunk{{.*}}}
// CIR64:         %[[VTABLE:.*]] = cir.load
// CIR64:         %[[SLOT:.*]] = cir.ptr_stride %[[VTABLE]]
// CIR64:         %[[CALLEE:.*]] = cir.load
// CIR64:         %[[CALLEE_TYPED:.*]] = cir.cast bitcast %[[CALLEE]] : !cir.ptr<!void> -> !cir.ptr<!cir.func<(!cir.ptr<!rec_sret3A3AC>, ...)>>
// CIR64:         cir.call %[[CALLEE_TYPED]](%arg0) musttail
// CIR64:         cir.return

namespace cdecl_inalloca {
struct Big {
  Big();
  ~Big();
  int big[32];
};
struct A { virtual void __cdecl a(); };
struct B { virtual void __cdecl b(Big); };
struct C : A, B {
  virtual void __cdecl a();
  virtual void __cdecl b(Big);
};
void f(C *c) {
  Big b;
  (c->*(&C::a))();
  ((c->*(&C::b))(b));
}
}

// CIR32-LABEL: cir.func no_inline dso_local @"?f@cdecl_inalloca@@YAXPAUC@1@@Z"
// CIR32:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@cdecl_inalloca@@$BA@AA"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR32:         cir.call %{{[0-9]+}}(%{{[0-9]+}})
// CIR32:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@cdecl_inalloca@@$BA@AA"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR32:         cir.call %{{[0-9]+}}(%{{[0-9]+}}, %{{[0-9]+}})

// CIR32-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"??_9C@cdecl_inalloca@@$BA@AA"(%arg0: !cir.ptr<!rec_cdecl_inalloca3A3AC>{{.*}}, ...) attributes {{{.*}}thunk{{.*}}}
// CIR32:         %[[VTABLE:.*]] = cir.load
// CIR32:         %[[SLOT:.*]] = cir.ptr_stride %[[VTABLE]]
// CIR32:         %[[CALLEE:.*]] = cir.load
// CIR32:         %[[CALLEE_TYPED:.*]] = cir.cast bitcast %[[CALLEE]] : !cir.ptr<!void> -> !cir.ptr<!cir.func<(!cir.ptr<!rec_cdecl_inalloca3A3AC>, ...)>>
// CIR32:         cir.call %[[CALLEE_TYPED]](%arg0) musttail
// CIR32:         cir.return

// CIR64-LABEL: cir.func no_inline dso_local @"?f@cdecl_inalloca@@YAXPEAUC@1@@Z"
// CIR64:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@cdecl_inalloca@@$BA@AA"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR64:         cir.call %{{[0-9]+}}(%{{[0-9]+}})
// CIR64:         cir.const #cir.const_record<{#cir.global_view<@"??_9C@cdecl_inalloca@@$BA@AA"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR64:         cir.call %{{[0-9]+}}(%{{[0-9]+}}, %{{[0-9]+}})

// CIR64-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"??_9C@cdecl_inalloca@@$BA@AA"(%arg0: !cir.ptr<!rec_cdecl_inalloca3A3AC>{{.*}}, ...) attributes {{{.*}}thunk{{.*}}}
// CIR64:         %[[VTABLE:.*]] = cir.load
// CIR64:         %[[SLOT:.*]] = cir.ptr_stride %[[VTABLE]]
// CIR64:         %[[CALLEE:.*]] = cir.load
// CIR64:         %[[CALLEE_TYPED:.*]] = cir.cast bitcast %[[CALLEE]] : !cir.ptr<!void> -> !cir.ptr<!cir.func<(!cir.ptr<!rec_cdecl_inalloca3A3AC>, ...)>>
// CIR64:         cir.call %[[CALLEE_TYPED]](%arg0) musttail
// CIR64:         cir.return
