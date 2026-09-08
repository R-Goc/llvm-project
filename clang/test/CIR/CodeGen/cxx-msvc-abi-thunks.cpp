// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR

// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7C@@6BA@@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"??_EC@@UEAAPEAXI@Z">{{.*}}#cir.global_view<@"?public_f@C@@UEAAXXZ">{{.*}}]>
// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7C@@6BB@@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"??_EC@@W7EAAPEAXI@Z">{{.*}}#cir.global_view<@"?public_f@C@@W7EAAXXZ">{{.*}}]>
// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7E@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?goo@E@@QEAAPEAUBaseRet@@XZ">{{.*}}#cir.global_view<@"?goo@E@@UEAAPEAUDerivedRet@@XZ">{{.*}}]>
// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7DerivedAgg@@6BBaseAgg2@@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?method@DerivedAgg@@W7EAAXUAgg@@@Z">{{.*}}]>
// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7DerivedSret@@6BBaseSret2@@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?method_sret@DerivedSret@@W7EAA?AUAgg@@U2@@Z">{{.*}}]>
// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7DerivedUnproto@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@BaseUnproto2@@W7EAAXUIncomplete@@@Z">{{.*}}]>

// === 1. Multiple Inheritance this-adjustment thunks ===

struct A {
  virtual ~A();
  virtual void public_f();
};

struct B {
  virtual ~B();
  virtual void public_f();
};

struct C : A, B {
  C();
  virtual ~C();
  virtual void public_f();
};

C::C() {}

// CIR-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"??_EC@@W7EAAPEAXI@Z"
// CIR:   %[[THIS_LOAD:[0-9]+]] = cir.load %0
// CIR:   %[[BYTE_PTR:[0-9]+]] = cir.cast bitcast %[[THIS_LOAD]] : !cir.ptr<!rec_C> -> !cir.ptr<!u8i>
// CIR:   %[[MINUS_8:[0-9]+]] = cir.const #cir.int<-8> : !s32i
// CIR:   %[[ADJ_PTR:[0-9]+]] = cir.ptr_stride %[[BYTE_PTR]], %[[MINUS_8]]
// CIR:   %[[ADJ_THIS:[0-9]+]] = cir.cast bitcast %[[ADJ_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!rec_C>
// CIR:   cir.call @"??_EC@@UEAAPEAXI@Z"(%[[ADJ_THIS]], %{{[0-9]+}})
// CIR:   cir.return

// CIR-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?public_f@C@@W7EAAXXZ"
// CIR:   %[[THIS_LOAD:[0-9]+]] = cir.load %0
// CIR:   %[[BYTE_PTR:[0-9]+]] = cir.cast bitcast %[[THIS_LOAD]] : !cir.ptr<!rec_C> -> !cir.ptr<!u8i>
// CIR:   %[[MINUS_8:[0-9]+]] = cir.const #cir.int<-8> : !s32i
// CIR:   %[[ADJ_PTR:[0-9]+]] = cir.ptr_stride %[[BYTE_PTR]], %[[MINUS_8]]
// CIR:   %[[ADJ_THIS:[0-9]+]] = cir.cast bitcast %[[ADJ_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!rec_C>
// CIR:   cir.call @"?public_f@C@@UEAAXXZ"(%[[ADJ_THIS]])
// CIR:   cir.return

// === 2. Covariant Return Thunks ===

struct Base1 { int a; };
struct BaseRet { int b; };
struct DerivedRet : Base1, BaseRet { int d; };

struct D {
  virtual BaseRet *goo();
};

struct E : D {
  E();
  virtual DerivedRet *goo();
};

E::E() {}

// CIR-LABEL: cir.func no_inline comdat weak_odr dso_local @"?goo@E@@QEAAPEAUBaseRet@@XZ"
// CIR:   %[[CALL_RES:[0-9]+]] = cir.call @"?goo@E@@UEAAPEAUDerivedRet@@XZ"(%{{[0-9]+}})
// CIR:   %[[NULL_PTR:[0-9]+]] = cir.const #cir.ptr<null> : !cir.ptr<!rec_DerivedRet>
// CIR:   %[[IS_NOT_NULL:[0-9]+]] = cir.cmp ne %[[CALL_RES]], %[[NULL_PTR]]
// CIR:   %[[TERNARY_RES:[0-9]+]] = cir.ternary(%[[IS_NOT_NULL]], true {
// CIR:     %[[RAW_PTR:[0-9]+]] = cir.cast bitcast %[[CALL_RES]]
// CIR:     cir.yield %{{[0-9]+}}
// CIR:   }, false {
// CIR:     %[[NULL_RET:[0-9]+]] = cir.const #cir.ptr<null>
// CIR:     cir.yield %[[NULL_RET]]
// CIR:   })
// CIR:   cir.return %{{[0-9]+}}

// === 3. Callee-Destructed Byval Parameter Forwarding in Thunks ===

struct Agg {
  Agg();
  Agg(const Agg &);
  ~Agg();
  int x;
};

struct BaseAgg1 {
  virtual void method(Agg x);
};

struct BaseAgg2 {
  virtual void method(Agg x);
};

struct DerivedAgg : BaseAgg1, BaseAgg2 {
  DerivedAgg();
  virtual void method(Agg x);
};

DerivedAgg::DerivedAgg() {}

// CIR-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?method@DerivedAgg@@W7EAAXUAgg@@@Z"
// CIR:   %[[THIS_LOAD:[0-9]+]] = cir.load %0
// CIR:   %[[BYTE_PTR:[0-9]+]] = cir.cast bitcast %[[THIS_LOAD]] : !cir.ptr<!rec_DerivedAgg> -> !cir.ptr<!u8i>
// CIR:   %[[MINUS_8:[0-9]+]] = cir.const #cir.int<-8> : !s32i
// CIR:   %[[ADJ_PTR:[0-9]+]] = cir.ptr_stride %[[BYTE_PTR]], %[[MINUS_8]]
// CIR:   %[[ADJ_THIS:[0-9]+]] = cir.cast bitcast %[[ADJ_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!rec_DerivedAgg>
// CIR:   cir.call @"?method@DerivedAgg@@UEAAXUAgg@@@Z"(%[[ADJ_THIS]], %{{.*}})
// CIR:   cir.return

// === 4. Sret and Byval Parameter Forwarding in Thunks ===

struct BaseSret1 {
  virtual Agg method_sret(Agg x);
};

struct BaseSret2 {
  virtual Agg method_sret(Agg x);
};

struct DerivedSret : BaseSret1, BaseSret2 {
  DerivedSret();
  virtual Agg method_sret(Agg x);
};

DerivedSret::DerivedSret() {}

// CIR-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?method_sret@DerivedSret@@W7EAA?AUAgg@@U2@@Z"
// CIR:   %[[THIS_LOAD:[0-9]+]] = cir.load %0
// CIR:   %[[BYTE_PTR:[0-9]+]] = cir.cast bitcast %[[THIS_LOAD]] : !cir.ptr<!rec_DerivedSret> -> !cir.ptr<!u8i>
// CIR:   %[[MINUS_8:[0-9]+]] = cir.const #cir.int<-8> : !s32i
// CIR:   %[[ADJ_PTR:[0-9]+]] = cir.ptr_stride %[[BYTE_PTR]], %[[MINUS_8]]
// CIR:   %[[ADJ_THIS:[0-9]+]] = cir.cast bitcast %[[ADJ_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!rec_DerivedSret>
// CIR:   cir.call @"?method_sret@DerivedSret@@UEAA?AUAgg@@U2@@Z"(%{{.*}}, %[[ADJ_THIS]], %{{.*}})
// CIR:   cir.return

// === 5. Unprototyped Must-Tail Thunks ===

struct Incomplete;

struct BaseUnproto1 {
  virtual void foo(Incomplete p) = 0;
};

struct BaseUnproto2 : virtual BaseUnproto1 {
  void foo(Incomplete p) override;
};

struct DerivedUnproto : BaseUnproto2 {
  int c;
};

DerivedUnproto unproto_obj;

// CIR-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?foo@BaseUnproto2@@W7EAAXUIncomplete@@@Z"
// CIR-SAME: attributes {{{.*}}thunk{{.*}}}
// CIR:   %[[THIS_ADJ:[0-9]+]] = cir.cast bitcast %{{[0-9]+}} : !cir.ptr<!u8i> -> !cir.ptr<!rec_BaseUnproto2>
// CIR:   cir.call @"?foo@BaseUnproto2@@UEAAXUIncomplete@@@Z"(%[[THIS_ADJ]]) musttail
// CIR:   cir.return
