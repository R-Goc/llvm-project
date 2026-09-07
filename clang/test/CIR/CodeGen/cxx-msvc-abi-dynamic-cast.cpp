// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s

struct S { char a; };
struct V { virtual void f(); };
struct A : virtual V {};
struct B : S, virtual V {};
struct T {};

// Global variable declarations must appear before any CIR-LABEL directives.
// CIR-DAG: cir.global "private" constant external @"??_R0?AUB@@@8" : !cir.ptr<!void>
// CIR-DAG: cir.global "private" constant external @"??_R0?AUT@@@8" : !cir.ptr<!void>
// CIR-DAG: cir.global "private" constant external @"??_R0?AUV@@@8" : !cir.ptr<!void>
// CIR-DAG: cir.global "private" constant external @"??_R0?AUA@@@8" : !cir.ptr<!void>

T* test0() { return dynamic_cast<T*>((B*)0); }
// CIR-LABEL: cir.func {{.*}}@"?test0@@YAPEAUT@@XZ"()
// CIR:   %[[NULL_B:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!rec_B>
// CIR:   %[[IS_NULL:.*]] = cir.cmp eq %[[NULL_B]], %{{.*}}
// CIR:   %[[RES:.*]] = cir.ternary(%[[IS_NULL]], true {
// CIR:     %[[NULL_T:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!rec_T>
// CIR:     cir.yield %[[NULL_T]] : !cir.ptr<!rec_T>
// CIR:   }, false {
// CIR:     %[[CALL:.*]] = cir.call @__RTDynamicCast
// CIR:     %[[CAST:.*]] = cir.cast bitcast %[[CALL]] : !cir.ptr<!void> -> !cir.ptr<!rec_T>
// CIR:     cir.yield %[[CAST]] : !cir.ptr<!rec_T>
// CIR:   })
// CIR:   cir.store %[[RES]]
// CIR:   cir.return

T* test1(V* x) { return &dynamic_cast<T&>(*x); }
// CIR-LABEL: cir.func {{.*}}@"?test1@@YAPEAUT@@PEAUV@@@Z"(
// CIR:   %[[LOAD_X:.*]] = cir.load {{.*}} : !cir.ptr<!cir.ptr<!rec_V>>, !cir.ptr<!rec_V>
// CIR:   %[[CAST_U8:.*]] = cir.cast bitcast %[[LOAD_X]] : !cir.ptr<!rec_V> -> !cir.ptr<!u8i>
// CIR:   %[[CAST_RAW:.*]] = cir.cast bitcast %[[CAST_U8]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR:   %[[ZERO:.*]] = cir.const #cir.int<0> : !s32i
// CIR:   %[[SRC_G:.*]] = cir.get_global @"??_R0?AUV@@@8"
// CIR:   %[[SRC_VOID:.*]] = cir.cast bitcast %[[SRC_G]]
// CIR:   %[[DST_G:.*]] = cir.get_global @"??_R0?AUT@@@8"
// CIR:   %[[DST_VOID:.*]] = cir.cast bitcast %[[DST_G]]
// CIR:   %[[IS_REF:.*]] = cir.const #cir.int<1> : !s32i
// CIR:   %[[CALL:.*]] = cir.call @__RTDynamicCast(%[[CAST_RAW]], %[[ZERO]], %[[SRC_VOID]], %[[DST_VOID]], %[[IS_REF]])
// CIR:   %[[RET:.*]] = cir.cast bitcast %[[CALL]] : !cir.ptr<!void> -> !cir.ptr<!rec_T>
// CIR:   cir.store %[[RET]]
// CIR:   cir.return

T* test2(A* x) { return &dynamic_cast<T&>(*x); }
// CIR-LABEL: cir.func {{.*}}@"?test2@@YAPEAUT@@PEAUA@@@Z"(
// CIR:   %[[LOAD_X:.*]] = cir.load {{.*}} : !cir.ptr<!cir.ptr<!rec_A>>, !cir.ptr<!rec_A>
// CIR:   %[[CAST_U8:.*]] = cir.cast bitcast %[[LOAD_X]] : !cir.ptr<!rec_A> -> !cir.ptr<!u8i>
// CIR:   %[[VBPTR:.*]] = cir.cast bitcast %{{.*}} : !cir.ptr<!u8i> -> !cir.ptr<!cir.ptr<!u8i>>
// CIR:   %[[VBTBL:.*]] = cir.load {{.*}}%[[VBPTR]] : !cir.ptr<!cir.ptr<!u8i>>, !cir.ptr<!u8i>
// CIR:   %[[VBTBL_I32:.*]] = cir.cast bitcast %[[VBTBL]] : !cir.ptr<!u8i> -> !cir.ptr<!s32i>
// CIR:   %[[SLOT:.*]] = cir.ptr_stride %[[VBTBL_I32]], %{{.*}} : (!cir.ptr<!s32i>, !s32i) -> !cir.ptr<!s32i>
// CIR:   %[[OFFS:.*]] = cir.load {{.*}}%[[SLOT]] : !cir.ptr<!s32i>, !s32i
// CIR:   %[[OFFS_EXT:.*]] = cir.cast integral %[[OFFS]] : !s32i -> !s64i
// CIR:   %[[ADJ:.*]] = cir.ptr_stride %[[CAST_U8]], %{{.*}} : (!cir.ptr<!u8i>, !s64i) -> !cir.ptr<!u8i>
// CIR:   %[[RAW_PTR:.*]] = cir.cast bitcast %[[ADJ]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR:   %[[CALL:.*]] = cir.call @__RTDynamicCast(%[[RAW_PTR]], %{{.*}}, %{{.*}}, %{{.*}}, %{{.*}})
// CIR:   %[[RET:.*]] = cir.cast bitcast %[[CALL]] : !cir.ptr<!void> -> !cir.ptr<!rec_T>
// CIR:   cir.store %[[RET]]
// CIR:   cir.return

T* test3(B* x) { return &dynamic_cast<T&>(*x); }
// CIR-LABEL: cir.func {{.*}}@"?test3@@YAPEAUT@@PEAUB@@@Z"(
// CIR:   %[[LOAD_X:.*]] = cir.load {{.*}} : !cir.ptr<!cir.ptr<!rec_B>>, !cir.ptr<!rec_B>
// CIR:   %[[VBASE_OFFS:.*]] = cir.load {{.*}} : !cir.ptr<!s32i>, !s32i
// CIR:   %[[VBASE_EXT:.*]] = cir.cast integral %[[VBASE_OFFS]] : !s32i -> !s64i
// CIR:   %[[VBPTR_OFFS:.*]] = cir.const #cir.int<8> : !s64i
// CIR:   %[[TOTAL_DELTA:.*]] = cir.add %[[VBPTR_OFFS]], %[[VBASE_EXT]] : !s64i
// CIR:   %[[ADJ:.*]] = cir.ptr_stride %{{.*}} : (!cir.ptr<!u8i>, !s64i) -> !cir.ptr<!u8i>
// CIR:   %[[RAW_PTR:.*]] = cir.cast bitcast %[[ADJ]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR:   %[[DELTA_I32:.*]] = cir.cast integral %[[TOTAL_DELTA]] : !s64i -> !s32i
// CIR:   %[[CALL:.*]] = cir.call @__RTDynamicCast(%[[RAW_PTR]], %[[DELTA_I32]], %{{.*}}, %{{.*}}, %{{.*}})
// CIR:   %[[RET:.*]] = cir.cast bitcast %[[CALL]] : !cir.ptr<!void> -> !cir.ptr<!rec_T>
// CIR:   cir.store %[[RET]]
// CIR:   cir.return

T* test4(V* x) { return dynamic_cast<T*>(x); }
// CIR-LABEL: cir.func {{.*}}@"?test4@@YAPEAUT@@PEAUV@@@Z"(
// CIR-NOT: cir.ternary
// CIR:   %[[RAW_U8:.*]] = cir.cast bitcast %{{.*}} : !cir.ptr<!rec_V> -> !cir.ptr<!u8i>
// CIR:   %[[RAW_PTR:.*]] = cir.cast bitcast %[[RAW_U8]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR:   %[[ZERO:.*]] = cir.const #cir.int<0> : !s32i
// CIR:   %[[IS_REF:.*]] = cir.const #cir.int<0> : !s32i
// CIR:   %[[CALL:.*]] = cir.call @__RTDynamicCast(%[[RAW_PTR]], %[[ZERO]], %{{.*}}, %{{.*}}, %[[IS_REF]])
// CIR:   %[[RET:.*]] = cir.cast bitcast %[[CALL]] : !cir.ptr<!void> -> !cir.ptr<!rec_T>
// CIR:   cir.store %[[RET]]
// CIR:   cir.return

T* test5(A* x) { return dynamic_cast<T*>(x); }
// CIR-LABEL: cir.func {{.*}}@"?test5@@YAPEAUT@@PEAUA@@@Z"(
// CIR:   %[[NULL_A:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!rec_A>
// CIR:   %[[IS_NULL:.*]] = cir.cmp eq %{{.*}}, %[[NULL_A]]
// CIR:   %[[TERNARY:.*]] = cir.ternary(%[[IS_NULL]], true {
// CIR:     %[[NULL:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!rec_T>
// CIR:     cir.yield %[[NULL]] : !cir.ptr<!rec_T>
// CIR:   }, false {
// CIR:     %[[CALL:.*]] = cir.call @__RTDynamicCast
// CIR:     %[[RES:.*]] = cir.cast bitcast %[[CALL]] : !cir.ptr<!void> -> !cir.ptr<!rec_T>
// CIR:     cir.yield %[[RES]] : !cir.ptr<!rec_T>
// CIR:   }) : (!cir.bool) -> !cir.ptr<!rec_T>
// CIR:   cir.store %[[TERNARY]]
// CIR:   cir.return

T* test6(B* x) { return dynamic_cast<T*>(x); }
// CIR-LABEL: cir.func {{.*}}@"?test6@@YAPEAUT@@PEAUB@@@Z"(
// CIR:   %[[NULL_B:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!rec_B>
// CIR:   %[[IS_NULL:.*]] = cir.cmp eq %{{.*}}, %[[NULL_B]]
// CIR:   %[[TERNARY:.*]] = cir.ternary(%[[IS_NULL]], true {
// CIR:     %[[NULL:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!rec_T>
// CIR:     cir.yield %[[NULL]] : !cir.ptr<!rec_T>
// CIR:   }, false {
// CIR:     %[[CALL:.*]] = cir.call @__RTDynamicCast
// CIR:     %[[RES:.*]] = cir.cast bitcast %[[CALL]] : !cir.ptr<!void> -> !cir.ptr<!rec_T>
// CIR:     cir.yield %[[RES]] : !cir.ptr<!rec_T>
// CIR:   }) : (!cir.bool) -> !cir.ptr<!rec_T>
// CIR:   cir.store %[[TERNARY]]
// CIR:   cir.return

void* test7(V* x) { return dynamic_cast<void*>(x); }
// CIR-LABEL: cir.func {{.*}}@"?test7@@YAPEAXPEAUV@@@Z"(
// CIR-NOT: cir.ternary
// CIR:   %[[RAW_U8:.*]] = cir.cast bitcast %{{.*}} : !cir.ptr<!rec_V> -> !cir.ptr<!u8i>
// CIR:   %[[RAW_PTR:.*]] = cir.cast bitcast %[[RAW_U8]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR:   %[[CALL:.*]] = cir.call @__RTCastToVoid(%[[RAW_PTR]])
// CIR:   cir.store %[[CALL]]
// CIR:   cir.return

void* test8(A* x) { return dynamic_cast<void*>(x); }
// CIR-LABEL: cir.func {{.*}}@"?test8@@YAPEAXPEAUA@@@Z"(
// CIR:   %[[NULL_A:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!rec_A>
// CIR:   %[[IS_NULL:.*]] = cir.cmp eq %{{.*}}, %[[NULL_A]]
// CIR:   %[[TERNARY:.*]] = cir.ternary(%[[IS_NULL]], true {
// CIR:     %[[NULL:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!void>
// CIR:     cir.yield %[[NULL]] : !cir.ptr<!void>
// CIR:   }, false {
// CIR:     %[[CALL:.*]] = cir.call @__RTCastToVoid
// CIR:     cir.yield %[[CALL]] : !cir.ptr<!void>
// CIR:   }) : (!cir.bool) -> !cir.ptr<!void>
// CIR:   cir.store %[[TERNARY]]
// CIR:   cir.return

void* test9(B* x) { return dynamic_cast<void*>(x); }
// CIR-LABEL: cir.func {{.*}}@"?test9@@YAPEAXPEAUB@@@Z"(
// CIR:   %[[NULL_B:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!rec_B>
// CIR:   %[[IS_NULL:.*]] = cir.cmp eq %{{.*}}, %[[NULL_B]]
// CIR:   %[[TERNARY:.*]] = cir.ternary(%[[IS_NULL]], true {
// CIR:     %[[NULL:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!void>
// CIR:     cir.yield %[[NULL]] : !cir.ptr<!void>
// CIR:   }, false {
// CIR:     %[[CALL:.*]] = cir.call @__RTCastToVoid
// CIR:     cir.yield %[[CALL]] : !cir.ptr<!void>
// CIR:   }) : (!cir.bool) -> !cir.ptr<!void>
// CIR:   cir.store %[[TERNARY]]
// CIR:   cir.return
