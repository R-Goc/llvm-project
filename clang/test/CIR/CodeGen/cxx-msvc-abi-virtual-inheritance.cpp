// RUN: %clang_cc1 %s -fno-rtti -std=c++11 -Wno-inaccessible-base -triple=i386-pc-win32 -fclangir -emit-cir -o %t.cir
// RUN: FileCheck --check-prefix=CIR %s --input-file=%t.cir
// RUN: FileCheck --check-prefix=CIR2 %s --input-file=%t.cir
// RUN: %clang_cc1 %s -fno-rtti -std=c++11 -Wno-inaccessible-base -triple=x86_64-pc-win32 -fclangir -emit-cir -o %t.64.cir

// Module-level globals must precede function-level checks.
// CIR-DAG: cir.global constant linkonce_odr comdat dso_local @"??_8B@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<8> : !s32i]> : !cir.array<!s32i x 2>
// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7B@@6B@" = #cir.vtable<{#cir.const_array<[#cir.global_view<@"??_EB@@$4PPPPPPPM@A@AEPAXI@Z"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@B@@$4PPPPPPPM@A@AEXXZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?bar@B@@$4PPPPPPPM@A@AEXXZ"> : !cir.ptr<!u8i>]>
// CIR-DAG: cir.global constant linkonce_odr comdat dso_local @"??_8C@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<8> : !s32i]> : !cir.array<!s32i x 2>
// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7C@@6B@" = #cir.vtable<{#cir.const_array<[#cir.global_view<@"??_EC@@$4PPPPPPPM@A@AEPAXI@Z"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@B@@$4PPPPPPPM@A@AEXXZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?bar@B@@$4PPPPPPPM@A@AEXXZ"> : !cir.ptr<!u8i>]>
// CIR-DAG: cir.global constant linkonce_odr comdat dso_local @"??_8D@multiple_vbases@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<8> : !s32i, #cir.int<16> : !s32i, #cir.int<24> : !s32i]>
// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7D@multiple_vbases@@6BA@1@@"
// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7D@multiple_vbases@@6BB@1@@"
// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7D@multiple_vbases@@6BC@1@@"
// CIR-DAG: cir.global constant linkonce_odr comdat dso_local @"??_8D@pr36921@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<8> : !s32i, #cir.int<4> : !s32i]>
// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7D@pr36921@@6BA@1@@"
// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7D@pr36921@@6BC@1@@"

struct VBase {
  virtual ~VBase();
  virtual void foo();
  virtual void bar();
  int field;
};

struct B : virtual VBase {
  B();
  virtual ~B();
  virtual void foo();
  virtual void bar();
};

B::B() {
  // CIR-LABEL: cir.func no_inline {{.*}}dso_local @"??0B@@QAE@XZ"(
  // CIR:   %[[IS_MOST_DERIVED:.*]] = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
  // CIR:   %[[ZERO:.*]] = cir.const #cir.int<0> : !s32i
  // CIR:   %[[CMP:.*]] = cir.cmp ne %[[IS_MOST_DERIVED]], %[[ZERO]] : !s32i
  // CIR:   cir.if %[[CMP]] {
  // CIR:     cir.get_global @"??_8B@@7B@"
  // CIR:     cir.call @"??0VBase@@QAE@XZ"
  // CIR:   }
  // Store vfptr:
  // CIR:   %[[VFADDR:.*]] = cir.vtable.address_point(@"??_7B@@6B@", address_point = <index = 0, offset = 0>)
  // CIR:   %[[VPTR:.*]] = cir.vtable.get_vptr %{{.*}} : !cir.ptr<!rec_B> -> !cir.ptr<!cir.vptr>
  // CIR:   cir.store align(4) %[[VFADDR]], %[[VPTR]] : !cir.vptr, !cir.ptr<!cir.vptr>
  //
  // Initialize vtorDisp:
  // CIR:   %[[VTORDISP_VAL:.*]] = cir.sub %{{.*}}, %{{.*}} : !s32i
  // CIR:   %[[NEG4:.*]] = cir.const #cir.int<-4> : !s32i
  // CIR:   %[[VTORDISP_PTR:.*]] = cir.ptr_stride %{{.*}}, %[[NEG4]]
  // CIR:   %[[VTORDISP_CAST:.*]] = cir.cast bitcast %[[VTORDISP_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!s32i>
  // CIR:   cir.store align(4) %[[VTORDISP_VAL]], %[[VTORDISP_CAST]] : !s32i, !cir.ptr<!s32i>
  // CIR:   cir.return
}

B::~B() {
  // CIR-LABEL: cir.func no_inline {{.*}}dso_local @"??1B@@UAE@XZ"(
  // Reload and adjust the this parameter:
  // CIR:   %[[THIS_RELOAD:.*]] = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_B>>, !cir.ptr<!rec_B>
  // CIR:   %[[THIS_U8:.*]] = cir.cast bitcast %[[THIS_RELOAD]] : !cir.ptr<!rec_B> -> !cir.ptr<!u8i>
  // CIR:   %[[NEG8:.*]] = cir.const #cir.int<-8> : !s32i
  // CIR:   %[[THIS_ADJ:.*]] = cir.ptr_stride %[[THIS_U8]], %[[NEG8]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>
  //
  // Restore vfptr:
  // CIR:   %[[VFADDR:.*]] = cir.vtable.address_point(@"??_7B@@6B@", address_point = <index = 0, offset = 0>)
  // CIR:   %[[VPTR:.*]] = cir.vtable.get_vptr %{{.*}} : !cir.ptr<!rec_B> -> !cir.ptr<!cir.vptr>
  // CIR:   cir.store align(4) %[[VFADDR]], %[[VPTR]] : !cir.vptr, !cir.ptr<!cir.vptr>
  //
  // Initialize vtorDisp:
  // CIR:   %[[VTORDISP_VAL:.*]] = cir.sub %{{.*}}, %{{.*}} : !s32i
  // CIR:   %[[NEG4:.*]] = cir.const #cir.int<-4> : !s32i
  // CIR:   %[[VTORDISP_PTR:.*]] = cir.ptr_stride %{{.*}}, %[[NEG4]]
  // CIR:   %[[VTORDISP_CAST:.*]] = cir.cast bitcast %[[VTORDISP_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!s32i>
  // CIR:   cir.store align(4) %[[VTORDISP_VAL]], %[[VTORDISP_CAST]] : !s32i, !cir.ptr<!s32i>

  foo();  // Avoid the "trivial destructor" optimization.
  // CIR: cir.call
  // CIR: cir.return

  // Complete object destructor for B:
  // CIR2-LABEL: cir.func no_inline comdat {{.*}}linkonce_odr dso_local @"??_DB@@QAEXXZ"(
  // CIR2:   cir.call @"??1B@@UAE@XZ"(%{{.*}}) cc(x86_thiscall) nothrow
  // CIR2:   cir.call @"??1VBase@@UAE@XZ"(%{{.*}}) cc(x86_thiscall) nothrow
  // CIR2:   cir.return

  // Scalar deleting destructor for pr36921::D:
  // CIR2-LABEL: cir.func no_inline comdat {{.*}}linkonce_odr dso_local @"??_GD@pr36921@@UAEPAXI@Z"(
  // CIR2:   %[[NEG4:.*]] = cir.const #cir.int<-4> : !s32i
  // CIR2:   %[[THIS_ADJ:.*]] = cir.ptr_stride %{{.*}}, %[[NEG4]]
  // CIR2:   cir.call @"??_DD@pr36921@@QAEXXZ"(%{{.*}}) cc(x86_thiscall) nothrow
  // CIR2:   cir.return

  // Vector deleting destructor alias with x86_thiscall calling convention:
  // CIR2: cir.func linkonce_odr private dso_local @"??_ED@pr36921@@UAEPAXI@Z"(!cir.ptr<!void>, !s32i) -> !cir.ptr<!void> alias(@"??_GD@pr36921@@UAEPAXI@Z") cc(x86_thiscall) func_info<#cir.cxx_dtor<!rec_pr369213A3AD>>

  // Scalar deleting destructor for B:
  // CIR2-LABEL: cir.func no_inline comdat {{.*}}linkonce_odr dso_local @"??_GB@@UAEPAXI@Z"(
  // CIR2:   %[[NEG8:.*]] = cir.const #cir.int<-8> : !s32i
  // CIR2:   %[[THIS_ADJ:.*]] = cir.ptr_stride %{{.*}}, %[[NEG8]]
  // CIR2:   cir.call @"??_DB@@QAEXXZ"(%{{.*}}) cc(x86_thiscall) nothrow
  // CIR2:   cir.call @"??3@YAXPAX@Z"
  // CIR2:   cir.return
}

void B::foo() {
  // CIR-LABEL: cir.func no_inline {{.*}}dso_local @"?foo@B@@UAEXXZ"(
  // Coerce this to correct type and adjust:
  // CIR:   %[[THIS_RELOAD:.*]] = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_B>>, !cir.ptr<!rec_B>
  // CIR:   %[[THIS_U8:.*]] = cir.cast bitcast %[[THIS_RELOAD]] : !cir.ptr<!rec_B> -> !cir.ptr<!u8i>
  // CIR:   %[[NEG8:.*]] = cir.const #cir.int<-8> : !s32i
  // CIR:   %[[THIS_ADJ:.*]] = cir.ptr_stride %[[THIS_U8]], %[[NEG8]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>

  field = 42;
  // CIR:   %[[C42:.*]] = cir.const #cir.int<42> : !s32i
  // CIR:   %[[FIELD:.*]] = cir.get_member %{{.*}}[1] {name = "field"} : !cir.ptr<!rec_VBase> -> !cir.ptr<!s32i>
  // CIR:   cir.store align(4) %[[C42]], %[[FIELD]] : !s32i, !cir.ptr<!s32i>
  // CIR:   cir.return
}

void call_vbase_bar(B *obj) {
  // CIR-LABEL: cir.func no_inline dso_local @"?call_vbase_bar@@YAXPAUB@@@Z"(
  // Dynamic virtual call adjusting this via vbtable:
  // CIR:   %[[BAR_ADDR:.*]] = cir.vtable.get_virtual_fn_addr %{{.*}}[2]
  // CIR:   %[[BAR_FN:.*]] = cir.load align(4) %[[BAR_ADDR]]
  // CIR:   cir.call %[[BAR_FN]](%{{.*}}) cc(x86_thiscall)
  // CIR:   cir.return
  obj->bar();
}

void delete_B(B *obj) {
  // CIR-LABEL: cir.func no_inline dso_local @"?delete_B@@YAXPAUB@@@Z"(
  // CIR:   %[[VFADDR:.*]] = cir.vtable.get_virtual_fn_addr %{{.*}}[0]
  // CIR:   %[[VFN:.*]] = cir.load align(4) %[[VFADDR]]
  // CIR:   cir.call %[[VFN]](%{{.*}}, %{{.*}}) cc(x86_thiscall) nothrow
  // CIR:   cir.return
  delete obj;
}

void call_complete_dtor() {
  // CIR-LABEL: cir.func no_inline dso_local @"?call_complete_dtor@@YAXXZ"()
  // CIR:   cir.call @"??0B@@QAE@XZ"(%[[B:.*]], %{{.*}}) cc(x86_thiscall)
  // CIR:   cir.call @"??_DB@@QAEXXZ"(%[[B]]) cc(x86_thiscall) nothrow
  // CIR:   cir.return
  B b;
}

struct C : B {
  C();
};

C::C() {
  // CIR-LABEL: cir.func no_inline {{.*}}dso_local @"??0C@@QAE@XZ"(
  // CIR:   cir.call @"??0VBase@@QAE@XZ"
  // CIR:   cir.call @"??0B@@QAE@XZ"
  // CIR:   cir.return
}

namespace multiple_vbases {
struct A {
  virtual void a();
};

struct B {
  virtual void b();
};

struct C {
  virtual void c();
};

struct D : virtual A, virtual B, virtual C {
  virtual void a();
  virtual void b();
  virtual void c();
  D();
};

D::D() {
  // CIR-LABEL: cir.func no_inline {{.*}}dso_local @"??0D@multiple_vbases@@QAE@XZ"(
  // Emits 3 vfptrs then 3 vtordisps:
  // CIR:   %[[VFPTR_A:.*]] = cir.vtable.address_point(@"??_7D@multiple_vbases@@6BA@1@@", address_point = <index = 0, offset = 0>)
  // CIR:   cir.store align(4) %[[VFPTR_A]], %{{.*}} : !cir.vptr, !cir.ptr<!cir.vptr>
  // CIR:   %[[VFPTR_B:.*]] = cir.vtable.address_point(@"??_7D@multiple_vbases@@6BB@1@@", address_point = <index = 0, offset = 0>)
  // CIR:   cir.store align(4) %[[VFPTR_B]], %{{.*}} : !cir.vptr, !cir.ptr<!cir.vptr>
  // CIR:   %[[VFPTR_C:.*]] = cir.vtable.address_point(@"??_7D@multiple_vbases@@6BC@1@@", address_point = <index = 0, offset = 0>)
  // CIR:   cir.store align(4) %[[VFPTR_C]], %{{.*}} : !cir.vptr, !cir.ptr<!cir.vptr>
  // CIR:   cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
  // CIR:   cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
  // CIR:   cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
  // CIR:   cir.return
}
}

namespace diamond {
struct A {
  A();
  virtual ~A();
};

struct B : virtual A {
  B();
  ~B();
};

struct C : virtual A {
  C();
  ~C();
  int c1, c2, c3;
};

struct Z {
  int z;
};

struct D : virtual Z, B, C {
  D();
  ~D();
} d;

D::~D() {
  // CIR-LABEL: cir.func no_inline {{.*}}dso_local @"??1D@diamond@@UAE@XZ"(
  // Adjust this parameter by -24:
  // CIR:   %[[THIS_RELOAD:.*]] = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_diamond3A3AD>>, !cir.ptr<!rec_diamond3A3AD>
  // CIR:   %[[THIS_U8:.*]] = cir.cast bitcast %[[THIS_RELOAD]] : !cir.ptr<!rec_diamond3A3AD> -> !cir.ptr<!u8i>
  // CIR:   %[[NEG24:.*]] = cir.const #cir.int<-24> : !s32i
  // CIR:   %[[THIS_ADJ:.*]] = cir.ptr_stride %[[THIS_U8]], %[[NEG24]]
  // CIR:   cir.call @"??1C@diamond@@UAE@XZ"(%{{.*}}) cc(x86_thiscall) nothrow
  // CIR:   cir.call @"??1B@diamond@@UAE@XZ"(%{{.*}}) cc(x86_thiscall) nothrow
  // CIR:   cir.return
}

}

namespace test2 {
struct A { A(); };
struct B : virtual A { B() {} };
struct C : B, A { C() {} };

void callC() { C x; }

// CIR-LABEL: cir.func no_inline comdat {{.*}}linkonce_odr dso_local @"??0C@test2@@QAE@XZ"(
// Virtual base:
// CIR:   cir.call @"??0A@test2@@QAE@XZ"(%{{.*}}) cc(x86_thiscall)
// Non-virtual base B:
// CIR:   cir.call @"??0B@test2@@QAE@XZ"(%{{.*}}, %{{.*}}) cc(x86_thiscall)
// Non-virtual base A:
// CIR:   cir.call @"??0A@test2@@QAE@XZ"(%{{.*}}) cc(x86_thiscall)
// CIR:   cir.return

}

namespace test3 {
struct A {
  virtual void foo();
};

struct B : virtual A {
  virtual void bar();
};

struct C : virtual A {
  virtual void foo();
};

struct D : B, C {
  virtual void bar();
  int field;
};

void D::bar() {
  // CIR-LABEL: cir.func no_inline {{.*}}dso_local @"?bar@D@test3@@UAEXXZ"(
  // Direct non-virtual call to C::foo() with static adjustment:
  // CIR:   %[[C_PTR:.*]] = cir.ptr_stride %{{.*}}, %{{.*}}
  // CIR:   cir.call @"?foo@C@test3@@UAEXXZ"(%{{.*}}) cc(x86_thiscall)
  // CIR:   cir.return
  C::foo();
}
}

namespace test4 {
struct A {
  virtual ~A() {}
};

struct B {
  virtual ~B() {}
};

struct C : virtual A, B {
  virtual ~C();
};

void foo(void*);

C::~C() {
  // CIR-LABEL: cir.func no_inline {{.*}}dso_local @"??1C@test4@@UAE@XZ"(
  // In this case this points to most derived class:
  // CIR:   %[[VFADDR:.*]] = cir.vtable.address_point(@"??_7C@test4@@6BB@1@@", address_point = <index = 0, offset = 0>)
  // CIR:   cir.store align(4) %[[VFADDR]], %{{.*}}
  // CIR:   cir.call @"?foo@test4@@YAXPAX@Z"
  // CIR:   cir.return
  foo(this);
}

void destroy(C *obj) {
  // CIR-LABEL: cir.func no_inline dso_local @"?destroy@test4@@YAXPAUC@1@@Z"(
  // CIR:   %[[VFADDR:.*]] = cir.vtable.get_virtual_fn_addr %{{.*}}[0]
  // CIR:   %[[VFN:.*]] = cir.load align(4) %[[VFADDR]]
  // CIR:   cir.call %[[VFN]](%{{.*}}, %{{.*}}){{.*}}cc(x86_thiscall)
  // CIR:   cir.return
  delete obj;
}

struct D {
  virtual void d();
};

struct E : D, B, virtual A {
  virtual ~E();
};

E::~E() {
  // CIR-LABEL: cir.func no_inline {{.*}}dso_local @"??1E@test4@@UAE@XZ"(
  // CIR:   %[[VFADDR:.*]] = cir.vtable.address_point(@"??_7E@test4@@6BD@1@@", address_point = <index = 0, offset = 0>)
  // CIR:   cir.store align(4) %[[VFADDR]], %{{.*}}
  // CIR:   cir.call @"?foo@test4@@YAXPAX@Z"
  // CIR:   cir.return
  foo(this);
}

void destroy(E *obj) {
  // CIR-LABEL: cir.func no_inline dso_local @"?destroy@test4@@YAXPAUE@1@@Z"(
  // CIR:   %[[VFADDR:.*]] = cir.vtable.get_virtual_fn_addr %{{.*}}[0]
  // CIR:   %[[VFN:.*]] = cir.load align(4) %[[VFADDR]]
  // CIR:   cir.call %[[VFN]](%{{.*}}, %{{.*}}){{.*}}cc(x86_thiscall)
  // CIR:   cir.return
  delete obj;
}

}

namespace test5 {
struct A {
  virtual void f();
};

struct B : virtual A {
  int Field;
};

struct C : B {
  C();
};

C::C() : B() {}
// CIR-LABEL: cir.func no_inline {{.*}}dso_local @"??0C@test5@@QAE@XZ"(
// Zero-initialize Field skipping vbptr:
// CIR:   %[[FIELD_OFFSET:.*]] = cir.const #cir.int<4> : !s32i
// CIR:   %[[FIELD_PTR:.*]] = cir.ptr_stride %{{.*}}, %[[FIELD_OFFSET]]
// CIR:   %[[FIELD_VOID:.*]] = cir.cast bitcast %[[FIELD_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR:   cir.libc.memset %{{.*}} bytes at %[[FIELD_VOID]] align(4) to %{{.*}}
// CIR:   cir.call @"??0B@test5@@QAE@XZ"
// CIR:   cir.return
}

namespace pr27621 {
struct A { virtual void f(); };
struct B { virtual void g(); };
struct C final : A, B {
  virtual void h();
  void g() override;
};
void callit(C *p) {
  static_cast<B*>(p)->g();
}
// CIR-LABEL: cir.func no_inline dso_local @"?callit@pr27621@@YAXPAUC@1@@Z"(
// CIR:   %[[OFFSET:.*]] = cir.const #cir.int<4> : !s32i
// CIR:   %[[B_PTR:.*]] = cir.ptr_stride %{{.*}}, %[[OFFSET]]
// CIR:   cir.call @"?g@C@pr27621@@UAEXXZ"(%{{.*}}) cc(x86_thiscall)
// CIR:   cir.return
}

namespace test6 {
class A {};
class B : virtual A {};
class C : virtual B {
  virtual void m_fn1();
  float field;
};
class D : C {
  D();
};
D::D() : C() {}
// CIR-LABEL: cir.func no_inline {{.*}}dso_local @"??0D@test6@@AAE@XZ"(
// CIR:   cir.libc.memset %{{.*}} bytes at %{{.*}} align(4) to %{{.*}}
// CIR:   cir.call @"??0C@test6@@QAE@XZ"
// CIR:   cir.return
}

namespace pr36921 {
struct A {
  virtual ~A() {}
};
struct B {
  virtual ~B() {}
};
struct C : virtual B {};
struct D : virtual A, C {};
D d;
}

namespace issue_60465 {
struct A {
  virtual ~A();
};

struct alignas(2 * sizeof(void *)) B : virtual A {
  ~B();
  void *x, *y;
};

B::~B() {
  // CIR-LABEL: cir.func no_inline {{.*}}dso_local @"??1B@issue_60465@@UAE@XZ"(
  // CIR:   %[[NEG12:.*]] = cir.const #cir.int<-12> : !s32i
  // CIR:   %[[THIS_ADJ:.*]] = cir.ptr_stride %{{.*}}, %[[NEG12]]
  // CIR:   %[[NULL:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!void>
  // CIR:   %[[X:.*]] = cir.get_member %{{.*}}[1] {name = "x"} : !cir.ptr<!rec_issue_604653A3AB> -> !cir.ptr<!cir.ptr<!void>>
  // CIR:   cir.store align(4) %[[NULL]], %[[X]] : !cir.ptr<!void>, !cir.ptr<!cir.ptr<!void>>
  // CIR:   %[[Y:.*]] = cir.get_member %{{.*}}[2] {name = "y"} : !cir.ptr<!rec_issue_604653A3AB> -> !cir.ptr<!cir.ptr<!void>>
  // CIR:   cir.store align(8) %{{.*}}, %[[Y]] : !cir.ptr<!void>, !cir.ptr<!cir.ptr<!void>>
  // CIR:   cir.return
  x = nullptr;
  y = nullptr;
}
}
