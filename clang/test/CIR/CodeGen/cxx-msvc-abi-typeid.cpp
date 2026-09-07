// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -Wno-potentially-evaluated-expression -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s

struct type_info;
namespace std { using ::type_info; }

struct V { virtual void f(); };
struct A : virtual V { A(); };

extern A a;
extern V v;
extern int b;
A* fn();
V* fn_v();

// Global variable declarations must appear before any CIR-LABEL directives.
// CIR-DAG: cir.global "private" constant external @"??_R0H@8" : !cir.ptr<!void>
// CIR-DAG: cir.global "private" constant external @"??_R0?AUA@@@8" : !cir.ptr<!void>
// CIR-DAG: cir.global "private" constant external @"??_R0PEAUA@@@8" : !cir.ptr<!void>
// CIR-DAG: cir.global "private" constant external @"??_R0?AUV@@@8" : !cir.ptr<!void>

const std::type_info* test0_typeid() { return &typeid(int); }
// CIR-LABEL: cir.func {{.*}}@"?test0_typeid@@YAPEBUtype_info@@XZ"()
// CIR:   %[[G:.*]] = cir.get_global @"??_R0H@8" : !cir.ptr<!cir.ptr<!void>>
// CIR:   %[[CAST:.*]] = cir.cast bitcast %[[G]] : !cir.ptr<!cir.ptr<!void>> -> !cir.ptr<!rec_type_info>
// CIR:   cir.store %[[CAST]]
// CIR:   cir.return

const std::type_info* test1_typeid() { return &typeid(A); }
// CIR-LABEL: cir.func {{.*}}@"?test1_typeid@@YAPEBUtype_info@@XZ"()
// CIR:   %[[G:.*]] = cir.get_global @"??_R0?AUA@@@8" : !cir.ptr<!cir.ptr<!void>>
// CIR:   %[[CAST:.*]] = cir.cast bitcast %[[G]] : !cir.ptr<!cir.ptr<!void>> -> !cir.ptr<!rec_type_info>
// CIR:   cir.store %[[CAST]]
// CIR:   cir.return

const std::type_info* test2_typeid() { return &typeid(&a); }
// CIR-LABEL: cir.func {{.*}}@"?test2_typeid@@YAPEBUtype_info@@XZ"()
// CIR:   %[[G:.*]] = cir.get_global @"??_R0PEAUA@@@8" : !cir.ptr<!cir.ptr<!void>>
// CIR:   %[[CAST:.*]] = cir.cast bitcast %[[G]] : !cir.ptr<!cir.ptr<!void>> -> !cir.ptr<!rec_type_info>
// CIR:   cir.store %[[CAST]]
// CIR:   cir.return

const std::type_info* test3_typeid() { return &typeid(*fn()); }
// CIR-LABEL: cir.func {{.*}}@"?test3_typeid@@YAPEBUtype_info@@XZ"()
// CIR:   %[[FN_RES:.*]] = cir.call @"?fn@@YAPEAUA@@XZ"()
// CIR:   %[[NULL_A:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!rec_A>
// CIR:   %[[IS_NULL:.*]] = cir.cmp eq %[[FN_RES]], %[[NULL_A]]
// CIR:   cir.if %[[IS_NULL]] {
// CIR:     %[[NULL:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!void>
// CIR:     cir.call @__RTtypeid(%[[NULL]]) {noreturn}
// CIR:     cir.unreachable
// CIR:   }
// CIR:   %[[CAST_U8:.*]] = cir.cast bitcast %[[FN_RES]] : !cir.ptr<!rec_A> -> !cir.ptr<!u8i>
// CIR:   %[[VBPTR:.*]] = cir.cast bitcast %{{.*}} : !cir.ptr<!u8i> -> !cir.ptr<!cir.ptr<!u8i>>
// CIR:   %[[VBTBL:.*]] = cir.load {{.*}}%[[VBPTR]] : !cir.ptr<!cir.ptr<!u8i>>, !cir.ptr<!u8i>
// CIR:   %[[VBTBL_I32:.*]] = cir.cast bitcast %[[VBTBL]] : !cir.ptr<!u8i> -> !cir.ptr<!s32i>
// CIR:   %[[SLOT:.*]] = cir.ptr_stride %[[VBTBL_I32]], %{{.*}} : (!cir.ptr<!s32i>, !s32i) -> !cir.ptr<!s32i>
// CIR:   %[[OFFS:.*]] = cir.load {{.*}}%[[SLOT]] : !cir.ptr<!s32i>, !s32i
// CIR:   %[[OFFS_EXT:.*]] = cir.cast integral %[[OFFS]] : !s32i -> !s64i
// CIR:   %[[ADJ:.*]] = cir.ptr_stride %[[CAST_U8]], %{{.*}} : (!cir.ptr<!u8i>, !s64i) -> !cir.ptr<!u8i>
// CIR:   %[[RAW_PTR:.*]] = cir.cast bitcast %[[ADJ]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR:   %[[RT_RES:.*]] = cir.call @__RTtypeid(%[[RAW_PTR]])
// CIR:   %[[RET:.*]] = cir.cast bitcast %[[RT_RES]] : !cir.ptr<!void> -> !cir.ptr<!rec_type_info>
// CIR:   cir.store %[[RET]]
// CIR:   cir.return

const std::type_info* test4_typeid() { return &typeid(b); }
// CIR-LABEL: cir.func {{.*}}@"?test4_typeid@@YAPEBUtype_info@@XZ"()
// CIR:   %[[G:.*]] = cir.get_global @"??_R0H@8" : !cir.ptr<!cir.ptr<!void>>
// CIR:   %[[CAST:.*]] = cir.cast bitcast %[[G]] : !cir.ptr<!cir.ptr<!void>> -> !cir.ptr<!rec_type_info>
// CIR:   cir.store %[[CAST]]
// CIR:   cir.return

const std::type_info* test5_typeid() { return &typeid(v); }
// CIR-LABEL: cir.func {{.*}}@"?test5_typeid@@YAPEBUtype_info@@XZ"()
// CIR:   %[[G:.*]] = cir.get_global @"??_R0?AUV@@@8" : !cir.ptr<!cir.ptr<!void>>
// CIR:   %[[CAST:.*]] = cir.cast bitcast %[[G]] : !cir.ptr<!cir.ptr<!void>> -> !cir.ptr<!rec_type_info>
// CIR:   cir.store %[[CAST]]
// CIR:   cir.return

const std::type_info* test6_typeid(V* p) { return &typeid(*p); }
// CIR-LABEL: cir.func {{.*}}@"?test6_typeid@@YAPEBUtype_info@@PEAUV@@@Z"(
// CIR-NOT: cir.cmp eq
// CIR:   %[[RAW_U8:.*]] = cir.cast bitcast %{{.*}} : !cir.ptr<!rec_V> -> !cir.ptr<!u8i>
// CIR:   %[[RAW_PTR:.*]] = cir.cast bitcast %[[RAW_U8]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR:   %[[RT_RES:.*]] = cir.call @__RTtypeid(%[[RAW_PTR]])
// CIR:   %[[RET:.*]] = cir.cast bitcast %[[RT_RES]] : !cir.ptr<!void> -> !cir.ptr<!rec_type_info>
// CIR:   cir.store %[[RET]]
// CIR:   cir.return
