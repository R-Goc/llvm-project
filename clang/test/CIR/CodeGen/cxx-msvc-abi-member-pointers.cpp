// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -fclangir -emit-cir -mmlir -mlir-print-ir-before=cir-cxxabi-lowering %s -o %t.cir 2> %t-before.cir
// RUN: FileCheck --check-prefix=CIR-BEFORE --input-file=%t-before.cir %s
// RUN: FileCheck --check-prefix=CIR-AFTER --input-file=%t.cir %s

struct S {
  int a;
  void f();
};

struct Poly {
  virtual void g();
  int b;
};

struct M : S, Poly {
  int c;
  void h();
};

struct V : virtual S {
  int d;
  void k();
};

struct U;

// Global member pointers
int S::*s_data = &S::a;
int S::*s_null = nullptr;
int Poly::*poly_null = nullptr;
int V::*v_null = nullptr;
int U::*u_null = nullptr;

void (S::*s_fn)() = &S::f;
void (Poly::*poly_vfn)() = &Poly::g;
void (M::*m_fn)() = &M::h;
void (M::*m_null)() = nullptr;

// CIR-BEFORE-DAG: cir.global external dso_local @"?s_data@@3PEQS@@HEQ1@" = #cir.data_member<[0]> : !cir.data_member<!s32i in !rec_S, single>
// CIR-BEFORE-DAG: cir.global external dso_local @"?s_null@@3PEQS@@HEQ1@" = #cir.data_member<null> : !cir.data_member<!s32i in !rec_S, single>
// CIR-BEFORE-DAG: cir.global external dso_local @"?poly_null@@3PEQPoly@@HEQ1@" = #cir.data_member<null> : !cir.data_member<!s32i in !rec_Poly, single>
// CIR-BEFORE-DAG: cir.global external dso_local @"?v_null@@3PEQV@@HEQ1@" = #cir.data_member<null> : !cir.data_member<!s32i in !rec_V, virtual>
// CIR-BEFORE-DAG: cir.global external dso_local @"?u_null@@3PEQU@@HEQ1@" = #cir.data_member<null> : !cir.data_member<!s32i in !rec_U, unspecified>
// CIR-BEFORE-DAG: cir.global external dso_local @"?s_fn@@3P8S@@EAAXXZEQ1@" = #cir.method<@"?f@S@@QEAAXXZ"> : !cir.method<!cir.func<(!cir.ptr<!rec_S>)> in !rec_S, single>
// CIR-BEFORE-DAG: cir.global external dso_local @"?poly_vfn@@3P8Poly@@EAAXXZEQ1@" = #cir.method<@"??_9Poly@@$BA@AA"> : !cir.method<!cir.func<(!cir.ptr<!rec_Poly>)> in !rec_Poly, single>
// CIR-BEFORE-DAG: cir.global external dso_local @"?m_fn@@3P8M@@EAAXXZEQ1@" = #cir.method<@"?h@M@@QEAAXXZ"> : !cir.method<!cir.func<(!cir.ptr<!rec_M>)> in !rec_M, multiple>
// CIR-BEFORE-DAG: cir.global external dso_local @"?m_null@@3P8M@@EAAXXZEQ1@" = #cir.method<null> : !cir.method<!cir.func<(!cir.ptr<!rec_M>)> in !rec_M, multiple>

// CIR-AFTER-DAG: cir.global external dso_local @"?s_data@@3PEQS@@HEQ1@" = #cir.int<0> : !s32i
// CIR-AFTER-DAG: cir.global external dso_local @"?s_null@@3PEQS@@HEQ1@" = #cir.int<-1> : !s32i
// CIR-AFTER-DAG: cir.global external dso_local @"?poly_null@@3PEQPoly@@HEQ1@" = #cir.int<0> : !s32i
// CIR-AFTER-DAG: cir.global external dso_local @"?v_null@@3PEQV@@HEQ1@" = #cir.const_record<{#cir.int<0> : !s32i, #cir.int<-1> : !s32i}>
// CIR-AFTER-DAG: cir.global external dso_local @"?u_null@@3PEQU@@HEQ1@" = #cir.const_record<{#cir.int<0> : !s32i, #cir.int<0> : !s32i, #cir.int<-1> : !s32i}>
// CIR-AFTER-DAG: cir.global external dso_local @"?s_fn@@3P8S@@EAAXXZEQ1@" = #cir.global_view<@"?f@S@@QEAAXXZ"> : !cir.ptr<!void>
// CIR-AFTER-DAG: cir.global external dso_local @"?poly_vfn@@3P8Poly@@EAAXXZEQ1@" = #cir.global_view<@"??_9Poly@@$BA@AA"> : !cir.ptr<!void>
// CIR-AFTER-DAG: cir.global external dso_local @"?m_fn@@3P8M@@EAAXXZEQ1@" = #cir.const_record<{#cir.global_view<@"?h@M@@QEAAXXZ"> : !cir.ptr<!void>, #cir.int<0> : !s32i}>
// CIR-AFTER-DAG: cir.global external dso_local @"?m_null@@3P8M@@EAAXXZEQ1@" = #cir.const_record<{#cir.ptr<null> : !cir.ptr<!void>, #cir.int<0> : !s32i}>

// Test vcall thunk generation
// CIR-BEFORE-LABEL: cir.func linkonce_odr @"??_9Poly@@$BA@AA"(%arg0: !cir.ptr<!rec_Poly>)
// CIR-BEFORE:   %[[THIS_PTR:.*]] = cir.cast bitcast %arg0 : !cir.ptr<!rec_Poly> -> !cir.ptr<!cir.ptr<!void>>
// CIR-BEFORE:   %[[VTABLE:.*]] = cir.load {{.*}}%[[THIS_PTR]] : !cir.ptr<!cir.ptr<!void>>, !cir.ptr<!void>
// CIR-BEFORE:   %[[IDX:.*]] = cir.const #cir.int<0> : !u64i
// CIR-BEFORE:   %[[SLOT:.*]] = cir.ptr_stride %[[VTABLE]], %[[IDX]] : (!cir.ptr<!void>, !u64i) -> !cir.ptr<!void>
// CIR-BEFORE:   %[[SLOT_PTR:.*]] = cir.cast bitcast %[[SLOT]] : !cir.ptr<!void> -> !cir.ptr<!cir.ptr<!void>>
// CIR-BEFORE:   %[[CALLEE:.*]] = cir.load {{.*}}%[[SLOT_PTR]] : !cir.ptr<!cir.ptr<!void>>, !cir.ptr<!void>
// CIR-BEFORE:   %[[CALLEE_TYPED:.*]] = cir.cast bitcast %[[CALLEE]] : !cir.ptr<!void> -> !cir.ptr<!cir.func<(!cir.ptr<!rec_Poly>)>>
// CIR-BEFORE:   cir.call %[[CALLEE_TYPED]](%arg0)
// CIR-BEFORE:   cir.return

// Test member data pointer access
int test_access(S *s, int S::*p) {
  return s->*p;
}
// CIR-BEFORE-LABEL: cir.func {{.*}}@"?test_access@@YAHPEAUS@@PEQ1@H@Z"
// CIR-BEFORE:   %[[BASE:.*]] = cir.load {{.*}} : !cir.ptr<!cir.ptr<!rec_S>>, !cir.ptr<!rec_S>
// CIR-BEFORE:   %[[MEM_PTR:.*]] = cir.load {{.*}} : !cir.ptr<!cir.data_member<!s32i in !rec_S, single>>, !cir.data_member<!s32i in !rec_S, single>
// CIR-BEFORE:   %[[RES:.*]] = cir.get_runtime_member %[[BASE]][%[[MEM_PTR]] : !cir.data_member<!s32i in !rec_S, single>] : !cir.ptr<!rec_S> -> !cir.ptr<!s32i>
// CIR-BEFORE:   %[[VAL:.*]] = cir.load {{.*}}%[[RES]] : !cir.ptr<!s32i>, !s32i
// CIR-BEFORE:   cir.return %{{.*}} : !s32i

// CIR-AFTER-LABEL: cir.func {{.*}}@"?test_access@@YAHPEAUS@@PEQ1@H@Z"
// CIR-AFTER:   %[[BASE:.*]] = cir.load {{.*}} : !cir.ptr<!cir.ptr<!rec_S>>, !cir.ptr<!rec_S>
// CIR-AFTER:   %[[MEM_VAL:.*]] = cir.load {{.*}} : !cir.ptr<!s32i>, !s32i
// CIR-AFTER:   %[[OBJ_BYTES:.*]] = cir.cast bitcast %[[BASE]] : !cir.ptr<!rec_S> -> !cir.ptr<!u8i>
// CIR-AFTER:   %[[MEMBER_BYTES:.*]] = cir.ptr_stride %[[OBJ_BYTES]], %[[MEM_VAL]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>
// CIR-AFTER:   %[[MEMBER_PTR:.*]] = cir.cast bitcast %[[MEMBER_BYTES]] : !cir.ptr<!u8i> -> !cir.ptr<!s32i>
// CIR-AFTER:   %[[VAL:.*]] = cir.load {{.*}}%[[MEMBER_PTR]] : !cir.ptr<!s32i>, !s32i
// CIR-AFTER:   cir.return %{{.*}} : !s32i

// Test member function pointer call
void test_call(M *m, void (M::*fn)()) {
  (m->*fn)();
}
// CIR-BEFORE-LABEL: cir.func {{.*}}@"?test_call@@YAXPEAUM@@P81@EAAXXZ@Z"
// CIR-BEFORE:   %[[BASE:.*]] = cir.load {{.*}} : !cir.ptr<!cir.ptr<!rec_M>>, !cir.ptr<!rec_M>
// CIR-BEFORE:   %[[FN_PTR:.*]] = cir.load {{.*}} : !cir.ptr<!cir.method<!cir.func<(!cir.ptr<!rec_M>)> in !rec_M, multiple>>, !cir.method<!cir.func<(!cir.ptr<!rec_M>)> in !rec_M, multiple>
// CIR-BEFORE:   %[[CALLEE:.*]], %[[ADJ_THIS:.*]] = cir.get_method %[[FN_PTR]], %[[BASE]]
// CIR-BEFORE:   cir.call %[[CALLEE]](%[[ADJ_THIS]])
// CIR-BEFORE:   cir.return

// CIR-AFTER-LABEL: cir.func {{.*}}@"?test_call@@YAXPEAUM@@P81@EAAXXZ@Z"
// CIR-AFTER:   %[[OBJ:.*]] = cir.load {{.*}} : !cir.ptr<!cir.ptr<!rec_M>>, !cir.ptr<!rec_M>
// CIR-AFTER:   %[[FN_STRUCT:.*]] = cir.load {{.*}} : !cir.ptr<!rec_anon_struct2>, !rec_anon_struct2
// CIR-AFTER:   %[[CALLEE_RAW:.*]] = cir.extract_member %[[FN_STRUCT]][0] : !rec_anon_struct2 -> !cir.ptr<!void>
// CIR-AFTER:   %[[ADJ:.*]] = cir.extract_member %[[FN_STRUCT]][1] : !rec_anon_struct2 -> !s32i
// CIR-AFTER:   %[[OBJ_BYTES:.*]] = cir.cast bitcast %[[OBJ]] : !cir.ptr<!rec_M> -> !cir.ptr<!u8i>
// CIR-AFTER:   %[[ADJ_BYTES:.*]] = cir.ptr_stride %[[OBJ_BYTES]], %[[ADJ]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>
// CIR-AFTER:   %[[ADJ_OBJ:.*]] = cir.cast bitcast %[[ADJ_BYTES]] : !cir.ptr<!u8i> -> !cir.ptr<!rec_M>
// CIR-AFTER:   %[[CALLEE:.*]] = cir.cast bitcast %[[CALLEE_RAW]] : !cir.ptr<!void> -> !cir.ptr<!cir.func<(!cir.ptr<!void>)>>
// CIR-AFTER:   %[[ADJ_VOID:.*]] = cir.cast bitcast %[[ADJ_OBJ]] : !cir.ptr<!rec_M> -> !cir.ptr<!void>
// CIR-AFTER:   cir.call %[[CALLEE]](%[[ADJ_VOID]])
// CIR-AFTER:   cir.return

// Test bool cast for data member
bool test_bool_data(int S::*p) {
  return (bool)p;
}
// CIR-BEFORE-LABEL: cir.func {{.*}}@"?test_bool_data@@YA_NPEQS@@H@Z"
// CIR-BEFORE:   %[[P:.*]] = cir.load {{.*}} : !cir.ptr<!cir.data_member<!s32i in !rec_S, single>>, !cir.data_member<!s32i in !rec_S, single>
// CIR-BEFORE:   %[[BOOL:.*]] = cir.cast member_ptr_to_bool %[[P]] : !cir.data_member<!s32i in !rec_S, single> -> !cir.bool
// CIR-BEFORE:   cir.return %{{.*}} : !cir.bool

// CIR-AFTER-LABEL: cir.func {{.*}}@"?test_bool_data@@YA_NPEQS@@H@Z"
// CIR-AFTER:   %[[P_VAL:.*]] = cir.load {{.*}} : !cir.ptr<!s32i>, !s32i
// CIR-AFTER:   %[[NULL_VAL:.*]] = cir.const #cir.int<-1> : !s32i
// CIR-AFTER:   %[[CMP:.*]] = cir.cmp ne %[[P_VAL]], %[[NULL_VAL]] : !s32i
// CIR-AFTER:   cir.return %{{.*}} : !cir.bool

// Test bool cast for method pointer
bool test_bool_method(void (M::*fn)()) {
  return (bool)fn;
}
// CIR-BEFORE-LABEL: cir.func {{.*}}@"?test_bool_method@@YA_NP8M@@EAAXXZ@Z"
// CIR-BEFORE:   %[[FN:.*]] = cir.load {{.*}} : !cir.ptr<!cir.method<!cir.func<(!cir.ptr<!rec_M>)> in !rec_M, multiple>>, !cir.method<!cir.func<(!cir.ptr<!rec_M>)> in !rec_M, multiple>
// CIR-BEFORE:   %[[BOOL:.*]] = cir.cast member_ptr_to_bool %[[FN]] : !cir.method<!cir.func<(!cir.ptr<!rec_M>)> in !rec_M, multiple> -> !cir.bool
// CIR-BEFORE:   cir.return %{{.*}} : !cir.bool

// CIR-AFTER-LABEL: cir.func {{.*}}@"?test_bool_method@@YA_NP8M@@EAAXXZ@Z"
// CIR-AFTER:   %[[NULL_PTR:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!void>
// CIR-AFTER:   %[[FN_PTR:.*]] = cir.extract_member %{{.*}}[0] : !rec_anon_struct2 -> !cir.ptr<!void>
// CIR-AFTER:   %[[CMP:.*]] = cir.cmp ne %[[FN_PTR]], %[[NULL_PTR]] : !cir.ptr<!void>
// CIR-AFTER:   cir.return %{{.*}} : !cir.bool

// Test data member pointer comparison
bool test_cmp_data(int S::*p1, int S::*p2) {
  return p1 == p2;
}
// CIR-BEFORE-LABEL: cir.func {{.*}}@"?test_cmp_data@@YA_NPEQS@@H0@Z"
// CIR-BEFORE:   %[[LHS:.*]] = cir.load {{.*}} : !cir.ptr<!cir.data_member<!s32i in !rec_S, single>>, !cir.data_member<!s32i in !rec_S, single>
// CIR-BEFORE:   %[[RHS:.*]] = cir.load {{.*}} : !cir.ptr<!cir.data_member<!s32i in !rec_S, single>>, !cir.data_member<!s32i in !rec_S, single>
// CIR-BEFORE:   %[[CMP:.*]] = cir.cmp eq %[[LHS]], %[[RHS]] : !cir.data_member<!s32i in !rec_S, single>
// CIR-BEFORE:   cir.return %{{.*}} : !cir.bool

// CIR-AFTER-LABEL: cir.func {{.*}}@"?test_cmp_data@@YA_NPEQS@@H0@Z"
// CIR-AFTER:   %[[LHS:.*]] = cir.load {{.*}} : !cir.ptr<!s32i>, !s32i
// CIR-AFTER:   %[[RHS:.*]] = cir.load {{.*}} : !cir.ptr<!s32i>, !s32i
// CIR-AFTER:   %[[CMP:.*]] = cir.cmp eq %[[LHS]], %[[RHS]] : !s32i
// CIR-AFTER:   cir.return %{{.*}} : !cir.bool

// Test method pointer comparison
bool test_cmp_method(void (M::*f1)(), void (M::*f2)()) {
  return f1 == f2;
}
// CIR-BEFORE-LABEL: cir.func {{.*}}@"?test_cmp_method@@YA_NP8M@@EAAXXZ0@Z"
// CIR-BEFORE:   %[[LHS:.*]] = cir.load {{.*}} : !cir.ptr<!cir.method<!cir.func<(!cir.ptr<!rec_M>)> in !rec_M, multiple>>, !cir.method<!cir.func<(!cir.ptr<!rec_M>)> in !rec_M, multiple>
// CIR-BEFORE:   %[[RHS:.*]] = cir.load {{.*}} : !cir.ptr<!cir.method<!cir.func<(!cir.ptr<!rec_M>)> in !rec_M, multiple>>, !cir.method<!cir.func<(!cir.ptr<!rec_M>)> in !rec_M, multiple>
// CIR-BEFORE:   %[[CMP:.*]] = cir.cmp eq %[[LHS]], %[[RHS]] : !cir.method<!cir.func<(!cir.ptr<!rec_M>)> in !rec_M, multiple>
// CIR-BEFORE:   cir.return %{{.*}} : !cir.bool

// CIR-AFTER-LABEL: cir.func {{.*}}@"?test_cmp_method@@YA_NP8M@@EAAXXZ0@Z"
// CIR-AFTER:   %[[LHS_FN:.*]] = cir.extract_member %{{.*}}[0] : !rec_anon_struct2 -> !cir.ptr<!void>
// CIR-AFTER:   %[[RHS_FN:.*]] = cir.extract_member %{{.*}}[0] : !rec_anon_struct2 -> !cir.ptr<!void>
// CIR-AFTER:   %[[FN_CMP:.*]] = cir.cmp eq %[[LHS_FN]], %[[RHS_FN]] : !cir.ptr<!void>
// CIR-AFTER:   %[[NULL_PTR:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!void>
// CIR-AFTER:   %[[IS_NULL:.*]] = cir.cmp eq %[[LHS_FN]], %[[NULL_PTR]] : !cir.ptr<!void>
// CIR-AFTER:   %[[LHS_ADJ:.*]] = cir.extract_member %{{.*}}[1] : !rec_anon_struct2 -> !s32i
// CIR-AFTER:   %[[RHS_ADJ:.*]] = cir.extract_member %{{.*}}[1] : !rec_anon_struct2 -> !s32i
// CIR-AFTER:   %[[ADJ_CMP:.*]] = cir.cmp eq %[[LHS_ADJ]], %[[RHS_ADJ]] : !s32i
// CIR-AFTER:   %[[NULL_OR_EQ:.*]] = cir.or %[[IS_NULL]], %[[ADJ_CMP]] : !cir.bool
// CIR-AFTER:   %[[IS_EQ:.*]] = cir.and %[[FN_CMP]], %[[NULL_OR_EQ]] : !cir.bool
// CIR-AFTER:   cir.return %{{.*}} : !cir.bool

// Test base to derived conversion
void (M::*test_conversion(void (S::*fn)()))() {
  return fn;
}
// CIR-BEFORE-LABEL: cir.func {{.*}}@"?test_conversion@@YAP8M@@EAAXXZP8S@@EAAXXZ@Z"
// CIR-BEFORE:   %[[SRC:.*]] = cir.load {{.*}} : !cir.ptr<!cir.method<!cir.func<(!cir.ptr<!rec_S>)> in !rec_S, single>>, !cir.method<!cir.func<(!cir.ptr<!rec_S>)> in !rec_S, single>
// CIR-BEFORE:   %[[DERIVED:.*]] = cir.derived_method %[[SRC]][16] : !cir.method<!cir.func<(!cir.ptr<!rec_S>)> in !rec_S, single> -> !cir.method<!cir.func<(!cir.ptr<!rec_M>)> in !rec_M, multiple>
// CIR-BEFORE:   cir.return %{{.*}} : !cir.method<!cir.func<(!cir.ptr<!rec_M>)> in !rec_M, multiple>

// CIR-AFTER-LABEL: cir.func {{.*}}@"?test_conversion@@YAP8M@@EAAXXZP8S@@EAAXXZ@Z"
// CIR-AFTER:   %[[SRC_FN:.*]] = cir.load {{.*}} : !cir.ptr<!cir.ptr<!void>>, !cir.ptr<!void>
// CIR-AFTER:   %[[NULL_PTR:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!void>
// CIR-AFTER:   %[[IS_NULL:.*]] = cir.cmp eq %[[SRC_FN]], %[[NULL_PTR]] : !cir.ptr<!void>
// CIR-AFTER:   %[[OFFSET:.*]] = cir.const #cir.int<16> : !s32i
// CIR-AFTER:   %[[ADJ:.*]] = cir.add nsw %{{.*}}, %[[OFFSET]] : !s32i
// CIR-AFTER:   %[[NEW_ADJ:.*]] = cir.select if %[[IS_NULL]] then %{{.*}} else %[[ADJ]] : (!cir.bool, !s32i, !s32i) -> !s32i
// CIR-AFTER:   %[[ZERO_STRUCT:.*]] = cir.const #cir.zero : !rec_anon_struct2
// CIR-AFTER:   %[[RES0:.*]] = cir.insert_member %[[ZERO_STRUCT]][0], %[[SRC_FN]] : !rec_anon_struct2, !cir.ptr<!void>
// CIR-AFTER:   %[[RES1:.*]] = cir.insert_member %[[RES0]][1], %[[NEW_ADJ]] : !rec_anon_struct2, !s32i
// CIR-AFTER:   cir.return %{{.*}} : !rec_anon_struct2
