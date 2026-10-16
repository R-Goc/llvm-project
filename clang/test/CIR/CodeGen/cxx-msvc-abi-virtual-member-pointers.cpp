// RUN: %clang_cc1 -triple i386-pc-windows-msvc -std=c++11 -emit-cir %s -o - | FileCheck %s --check-prefix=X86
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++11 -emit-cir %s -o - | FileCheck %s --check-prefix=X64

struct S {
  int x, y, z;
};

// U is not trivially copyable, and requires inalloca to pass by value.
struct U {
  int u;
  U();
  ~U();
  U(const U &);
};

struct B;

struct C {
  virtual void foo();
  virtual int bar(int, double);
  virtual S baz(int);
  virtual S qux(U);
  virtual void thud(...);
  virtual void (B::*plugh())();
};

namespace {
struct D {
  virtual void foo();
};
}

void f() {
  void (C::*ptr)();
  ptr = &C::foo;
  ptr = &C::foo; // Don't crash trying to define the thunk twice :)

  int (C::*ptr2)(int, double);
  ptr2 = &C::bar;

  S (C::*ptr3)(int);
  ptr3 = &C::baz;

  void (D::*ptr4)();
  ptr4 = &D::foo;

  S (C::*ptr5)(U);
  ptr5 = &C::qux;

  void (C::*ptr6)(...);
  ptr6 = &C::thud;

  auto ptr7 = &C::plugh;

// X86-LABEL: cir.func no_inline dso_local @"?f@@YAXXZ"()
// X86:   %[[PTR:.*]] = cir.alloca "ptr"
// X86:   %[[PTR2:.*]] = cir.alloca "ptr2"
// X86:   %[[PTR3:.*]] = cir.alloca "ptr3"
// X86:   %[[PTR4:.*]] = cir.alloca "ptr4"
// X86:   %[[PTR5:.*]] = cir.alloca "ptr5"
// X86:   %[[PTR6:.*]] = cir.alloca "ptr6"
// X86:   %[[PTR7:.*]] = cir.alloca "ptr7"
// X86:   %[[V1:.*]] = cir.const #cir.global_view<@"??_9C@@$BA@AE">
// X86:   cir.store align(4) %[[V1]], %[[PTR]]
// X86:   %[[V1b:.*]] = cir.const #cir.global_view<@"??_9C@@$BA@AE">
// X86:   cir.store align(4) %[[V1b]], %[[PTR]]
// X86:   %[[V2:.*]] = cir.const #cir.global_view<@"??_9C@@$B3AE">
// X86:   cir.store align(4) %[[V2]], %[[PTR2]]
// X86:   %[[V3:.*]] = cir.const #cir.global_view<@"??_9C@@$B7AE">
// X86:   cir.store align(4) %[[V3]], %[[PTR3]]
// X86:   %[[V4:.*]] = cir.const #cir.global_view<@"??_9D@?A0x{{[0-9a-fA-F]+}}@@$BA@AE">
// X86:   cir.store align(4) %[[V4]], %[[PTR4]]
// X86:   %[[V5:.*]] = cir.const #cir.global_view<@"??_9C@@$BM@AE">
// X86:   cir.store align(4) %[[V5]], %[[PTR5]]
// X86:   %[[V6:.*]] = cir.const #cir.global_view<@"??_9C@@$BBA@AA">
// X86:   cir.store align(4) %[[V6]], %[[PTR6]]
// X86:   %[[V7:.*]] = cir.const #cir.global_view<@"??_9C@@$BBE@AE">
// X86:   cir.store align(4) %[[V7]], %[[PTR7]]
// X86:   cir.return

// X64-LABEL: cir.func no_inline dso_local @"?f@@YAXXZ"()
// X64:   %[[PTR:.*]] = cir.alloca "ptr"
// X64:   %[[PTR2:.*]] = cir.alloca "ptr2"
// X64:   %[[PTR3:.*]] = cir.alloca "ptr3"
// X64:   %[[PTR4:.*]] = cir.alloca "ptr4"
// X64:   %[[PTR5:.*]] = cir.alloca "ptr5"
// X64:   %[[PTR6:.*]] = cir.alloca "ptr6"
// X64:   %[[PTR7:.*]] = cir.alloca "ptr7"
// X64:   %[[V1:.*]] = cir.const #cir.global_view<@"??_9C@@$BA@AA">
// X64:   cir.store align(8) %[[V1]], %[[PTR]]
// X64:   %[[V1b:.*]] = cir.const #cir.global_view<@"??_9C@@$BA@AA">
// X64:   cir.store align(8) %[[V1b]], %[[PTR]]
// X64:   %[[V2:.*]] = cir.const #cir.global_view<@"??_9C@@$B7AA">
// X64:   cir.store align(8) %[[V2]], %[[PTR2]]
// X64:   %[[V3:.*]] = cir.const #cir.global_view<@"??_9C@@$BBA@AA">
// X64:   cir.store align(8) %[[V3]], %[[PTR3]]
// X64:   %[[V4:.*]] = cir.const #cir.global_view<@"??_9D@?A0x{{[0-9a-fA-F]+}}@@$BA@AA">
// X64:   cir.store align(8) %[[V4]], %[[PTR4]]
// X64:   %[[V5:.*]] = cir.const #cir.global_view<@"??_9C@@$BBI@AA">
// X64:   cir.store align(8) %[[V5]], %[[PTR5]]
// X64:   %[[V6:.*]] = cir.const #cir.global_view<@"??_9C@@$BCA@AA">
// X64:   cir.store align(8) %[[V6]], %[[PTR6]]
// X64:   %[[V7:.*]] = cir.const #cir.global_view<@"??_9C@@$BCI@AA">
// X64:   cir.store align(8) %[[V7]], %[[PTR7]]
// X64:   cir.return
}

// Thunk for calling the 1st virtual function in C with no parameters.
// X86-LABEL: cir.func {{.*}}linkonce_odr{{.*}}@"??_9C@@$BA@AE"(%arg0: !cir.ptr<!rec_C>{{.*}}, ...) cc(x86_thiscall) attributes {{{.*}}thunk{{.*}}}
// X86:   %[[THIS_PTR:.*]] = cir.cast bitcast %arg0 : !cir.ptr<!rec_C> -> !cir.ptr<!cir.ptr<!void>>
// X86:   %[[VTABLE:.*]] = cir.load align(1) %[[THIS_PTR]] : !cir.ptr<!cir.ptr<!void>>, !cir.ptr<!void>
// X86:   %[[IDX:.*]] = cir.const #cir.int<0> : !u64i
// X86:   %[[SLOT:.*]] = cir.ptr_stride %[[VTABLE]], %[[IDX]] : (!cir.ptr<!void>, !u64i) -> !cir.ptr<!void>
// X86:   %[[SLOT_PTR:.*]] = cir.cast bitcast %[[SLOT]] : !cir.ptr<!void> -> !cir.ptr<!cir.ptr<!void>>
// X86:   %[[CALLEE:.*]] = cir.load align(1) %[[SLOT_PTR]] : !cir.ptr<!cir.ptr<!void>>, !cir.ptr<!void>
// X86:   %[[CALLEE_TYPED:.*]] = cir.cast bitcast %[[CALLEE]] : !cir.ptr<!void> -> !cir.ptr<!cir.func<(!cir.ptr<!rec_C>, ...)>>
// X86:   cir.call %[[CALLEE_TYPED]](%arg0) cc(x86_thiscall) musttail : (!cir.ptr<!cir.func<(!cir.ptr<!rec_C>, ...)>>, !cir.ptr<!rec_C>) -> ()
// X86:   cir.return

// X64-LABEL: cir.func {{.*}}linkonce_odr{{.*}}@"??_9C@@$BA@AA"(%arg0: !cir.ptr<!rec_C>{{.*}}, ...) attributes {{{.*}}thunk{{.*}}}
// X64:   %[[THIS_PTR:.*]] = cir.cast bitcast %arg0 : !cir.ptr<!rec_C> -> !cir.ptr<!cir.ptr<!void>>
// X64:   %[[VTABLE:.*]] = cir.load align(1) %[[THIS_PTR]] : !cir.ptr<!cir.ptr<!void>>, !cir.ptr<!void>
// X64:   %[[IDX:.*]] = cir.const #cir.int<0> : !u64i
// X64:   %[[SLOT:.*]] = cir.ptr_stride %[[VTABLE]], %[[IDX]] : (!cir.ptr<!void>, !u64i) -> !cir.ptr<!void>
// X64:   %[[SLOT_PTR:.*]] = cir.cast bitcast %[[SLOT]] : !cir.ptr<!void> -> !cir.ptr<!cir.ptr<!void>>
// X64:   %[[CALLEE:.*]] = cir.load align(1) %[[SLOT_PTR]] : !cir.ptr<!cir.ptr<!void>>, !cir.ptr<!void>
// X64:   %[[CALLEE_TYPED:.*]] = cir.cast bitcast %[[CALLEE]] : !cir.ptr<!void> -> !cir.ptr<!cir.func<(!cir.ptr<!rec_C>, ...)>>
// X64:   cir.call %[[CALLEE_TYPED]](%arg0) musttail : (!cir.ptr<!cir.func<(!cir.ptr<!rec_C>, ...)>>, !cir.ptr<!rec_C>) -> ()
// X64:   cir.return

// Other virtual call thunks:
// X86-DAG: cir.func {{.*}}linkonce_odr{{.*}}@"??_9C@@$B3AE"(%arg0: !cir.ptr<!rec_C>{{.*}}, ...) cc(x86_thiscall) attributes {{{.*}}thunk{{.*}}}
// X64-DAG: cir.func {{.*}}linkonce_odr{{.*}}@"??_9C@@$B7AA"(%arg0: !cir.ptr<!rec_C>{{.*}}, ...) attributes {{{.*}}thunk{{.*}}}
// X86-DAG: cir.func {{.*}}linkonce_odr{{.*}}@"??_9C@@$B7AE"(%arg0: !cir.ptr<!rec_C>{{.*}}, ...) cc(x86_thiscall) attributes {{{.*}}thunk{{.*}}}
// X64-DAG: cir.func {{.*}}linkonce_odr{{.*}}@"??_9C@@$BBA@AA"(%arg0: !cir.ptr<!rec_C>{{.*}}, ...) attributes {{{.*}}thunk{{.*}}}
// X86-DAG: cir.func {{.*}}internal{{.*}}@"??_9D@?A0x{{[0-9a-fA-F]+}}@@$BA@AE"(%arg0: !cir.ptr<{{.*}}D>{{.*}}, ...) cc(x86_thiscall) attributes {{{.*}}thunk{{.*}}}
// X64-DAG: cir.func {{.*}}internal{{.*}}@"??_9D@?A0x{{[0-9a-fA-F]+}}@@$BA@AA"(%arg0: !cir.ptr<{{.*}}D>{{.*}}, ...) attributes {{{.*}}thunk{{.*}}}
// X86-DAG: cir.func {{.*}}linkonce_odr{{.*}}@"??_9C@@$BM@AE"(%arg0: !cir.ptr<!rec_C>{{.*}}, ...) cc(x86_thiscall) attributes {{{.*}}thunk{{.*}}}
// X64-DAG: cir.func {{.*}}linkonce_odr{{.*}}@"??_9C@@$BBI@AA"(%arg0: !cir.ptr<!rec_C>{{.*}}, ...) attributes {{{.*}}thunk{{.*}}}
// X86-DAG: cir.func {{.*}}linkonce_odr{{.*}}@"??_9C@@$BBA@AA"(%arg0: !cir.ptr<!rec_C>{{.*}}, ...) attributes {{{.*}}thunk{{.*}}}
// X64-DAG: cir.func {{.*}}linkonce_odr{{.*}}@"??_9C@@$BCA@AA"(%arg0: !cir.ptr<!rec_C>{{.*}}, ...) attributes {{{.*}}thunk{{.*}}}
// X86-DAG: cir.func {{.*}}linkonce_odr{{.*}}@"??_9C@@$BBE@AE"(%arg0: !cir.ptr<!rec_C>{{.*}}, ...) cc(x86_thiscall) attributes {{{.*}}thunk{{.*}}}
// X64-DAG: cir.func {{.*}}linkonce_odr{{.*}}@"??_9C@@$BCI@AA"(%arg0: !cir.ptr<!rec_C>{{.*}}, ...) attributes {{{.*}}thunk{{.*}}}
