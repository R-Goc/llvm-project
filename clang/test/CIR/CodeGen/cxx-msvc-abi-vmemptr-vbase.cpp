// RUN: %clang_cc1 -triple i386-pc-windows-msvc -std=c++11 -fno-rtti -emit-cir %s -o - | FileCheck %s --check-prefix=X86
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++11 -fno-rtti -emit-cir %s -o - | FileCheck %s --check-prefix=X64

namespace PR23452 {
struct A {
  virtual void f();
};
struct B : virtual A {
  virtual void f();
};
void (B::*MemPtr)(void) = &B::f;

void call_memptr(B *b) {
  (b->*MemPtr)();
}
}

// X86-DAG: cir.global external dso_local @"?MemPtr@PR23452@@3P8B@1@AEXXZQ21@" = #cir.const_record<{#cir.global_view<@"??_9B@PR23452@@$BA@AE"> : !cir.ptr<!void>, #cir.int<0> : !s32i, #cir.int<4> : !s32i}>

// X64-DAG: cir.global external dso_local @"?MemPtr@PR23452@@3P8B@1@EAAXXZEQ21@" = #cir.const_record<{#cir.global_view<@"??_9B@PR23452@@$BA@AA"> : !cir.ptr<!void>, #cir.int<0> : !s32i, #cir.int<4> : !s32i}>

// X86-LABEL: cir.func no_inline dso_local @"?call_memptr@PR23452@@YAXPAUB@1@@Z"
// X86:   %[[B_VAR:.*]] = cir.alloca "b"
// X86:   cir.store %arg0, %[[B_VAR]]
// X86:   %[[B_VAL:.*]] = cir.load align(4) %[[B_VAR]]
// X86:   %[[MEMPTR_GLOBAL:.*]] = cir.get_global @"?MemPtr@PR23452@@3P8B@1@AEXXZQ21@"
// X86:   %[[MEMPTR:.*]] = cir.load align(4) %[[MEMPTR_GLOBAL]]
// X86:   %[[FN_PTR:.*]] = cir.extract_member %[[MEMPTR]][0] : !rec_anon_struct -> !cir.ptr<!void>
// X86:   %[[THIS_ADJ:.*]] = cir.extract_member %[[MEMPTR]][1] : !rec_anon_struct -> !s32i
// X86:   %[[THIS_BYTES:.*]] = cir.cast bitcast %[[B_VAL]] : !cir.ptr<!rec_PR234523A3AB> -> !cir.ptr<!u8i>
// X86:   %[[VBTABLE_OFFS:.*]] = cir.extract_member %[[MEMPTR]][2] : !rec_anon_struct -> !s32i
// X86:   %[[VBPTR:.*]] = cir.cast bitcast %[[THIS_BYTES]] : !cir.ptr<!u8i> -> !cir.ptr<!cir.ptr<!s32i>>
// X86:   %[[VBTABLE:.*]] = cir.load %[[VBPTR]] : !cir.ptr<!cir.ptr<!s32i>>, !cir.ptr<!s32i>
// X86:   %[[TWO:.*]] = cir.const #cir.int<2> : !s32i
// X86:   %[[VBT_INDEX:.*]] = cir.shift(right, %[[VBTABLE_OFFS]] : !s32i, %[[TWO]] : !s32i) -> !s32i
// X86:   %[[ENTRY_PTR:.*]] = cir.ptr_stride %[[VBTABLE]], %[[VBT_INDEX]] : (!cir.ptr<!s32i>, !s32i) -> !cir.ptr<!s32i>
// X86:   %[[VBASE_OFFS:.*]] = cir.load %[[ENTRY_PTR]] : !cir.ptr<!s32i>, !s32i
// X86:   %[[VBASE_ADJ:.*]] = cir.ptr_stride %[[THIS_BYTES]], %[[VBASE_OFFS]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>
// X86:   %[[ADJ_VBASE:.*]] = cir.ptr_stride %[[VBASE_ADJ]], %[[THIS_ADJ]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>
// X86:   %[[ADJ_NOVBASE:.*]] = cir.ptr_stride %[[THIS_BYTES]], %[[THIS_ADJ]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>
// X86:   %[[ZERO:.*]] = cir.const #cir.int<0> : !s32i
// X86:   %[[IS_ZERO:.*]] = cir.cmp eq %[[VBTABLE_OFFS]], %[[ZERO]] : !s32i
// X86:   %[[SELECTED:.*]] = cir.select if %[[IS_ZERO]] then %[[ADJ_NOVBASE]] else %[[ADJ_VBASE]] : (!cir.bool, !cir.ptr<!u8i>, !cir.ptr<!u8i>) -> !cir.ptr<!u8i>
// X86:   %[[ADJ_B:.*]] = cir.cast bitcast %[[SELECTED]] : !cir.ptr<!u8i> -> !cir.ptr<!rec_PR234523A3AB>
// X86:   %[[CALLEE:.*]] = cir.cast bitcast %[[FN_PTR]] : !cir.ptr<!void> -> !cir.ptr<!cir.func<(!cir.ptr<!void>)>>
// X86:   %[[ADJ_VOID:.*]] = cir.cast bitcast %[[ADJ_B]] : !cir.ptr<!rec_PR234523A3AB> -> !cir.ptr<!void>
// X86:   cir.call %[[CALLEE]](%[[ADJ_VOID]]) cc(x86_thiscall) : (!cir.ptr<!cir.func<(!cir.ptr<!void>)>>, !cir.ptr<!void> {llvm.noundef}) -> ()
// X86:   cir.return

// X64-LABEL: cir.func no_inline dso_local @"?call_memptr@PR23452@@YAXPEAUB@1@@Z"
// X64:   %[[B_VAR:.*]] = cir.alloca "b"
// X64:   cir.store %arg0, %[[B_VAR]]
// X64:   %[[B_VAL:.*]] = cir.load align(8) %[[B_VAR]]
// X64:   %[[MEMPTR_GLOBAL:.*]] = cir.get_global @"?MemPtr@PR23452@@3P8B@1@EAAXXZEQ21@"
// X64:   %[[MEMPTR:.*]] = cir.load align(8) %[[MEMPTR_GLOBAL]]
// X64:   %[[FN_PTR:.*]] = cir.extract_member %[[MEMPTR]][0] : !rec_anon_struct -> !cir.ptr<!void>
// X64:   %[[THIS_ADJ:.*]] = cir.extract_member %[[MEMPTR]][1] : !rec_anon_struct -> !s32i
// X64:   %[[THIS_BYTES:.*]] = cir.cast bitcast %[[B_VAL]] : !cir.ptr<!rec_PR234523A3AB> -> !cir.ptr<!u8i>
// X64:   %[[VBTABLE_OFFS:.*]] = cir.extract_member %[[MEMPTR]][2] : !rec_anon_struct -> !s32i
// X64:   %[[VBPTR:.*]] = cir.cast bitcast %[[THIS_BYTES]] : !cir.ptr<!u8i> -> !cir.ptr<!cir.ptr<!s32i>>
// X64:   %[[VBTABLE:.*]] = cir.load %[[VBPTR]] : !cir.ptr<!cir.ptr<!s32i>>, !cir.ptr<!s32i>
// X64:   %[[TWO:.*]] = cir.const #cir.int<2> : !s32i
// X64:   %[[VBT_INDEX:.*]] = cir.shift(right, %[[VBTABLE_OFFS]] : !s32i, %[[TWO]] : !s32i) -> !s32i
// X64:   %[[ENTRY_PTR:.*]] = cir.ptr_stride %[[VBTABLE]], %[[VBT_INDEX]] : (!cir.ptr<!s32i>, !s32i) -> !cir.ptr<!s32i>
// X64:   %[[VBASE_OFFS:.*]] = cir.load %[[ENTRY_PTR]] : !cir.ptr<!s32i>, !s32i
// X64:   %[[VBASE_ADJ:.*]] = cir.ptr_stride %[[THIS_BYTES]], %[[VBASE_OFFS]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>
// X64:   %[[ADJ_VBASE:.*]] = cir.ptr_stride %[[VBASE_ADJ]], %[[THIS_ADJ]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>
// X64:   %[[ADJ_NOVBASE:.*]] = cir.ptr_stride %[[THIS_BYTES]], %[[THIS_ADJ]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>
// X64:   %[[ZERO:.*]] = cir.const #cir.int<0> : !s32i
// X64:   %[[IS_ZERO:.*]] = cir.cmp eq %[[VBTABLE_OFFS]], %[[ZERO]] : !s32i
// X64:   %[[SELECTED:.*]] = cir.select if %[[IS_ZERO]] then %[[ADJ_NOVBASE]] else %[[ADJ_VBASE]] : (!cir.bool, !cir.ptr<!u8i>, !cir.ptr<!u8i>) -> !cir.ptr<!u8i>
// X64:   %[[ADJ_B:.*]] = cir.cast bitcast %[[SELECTED]] : !cir.ptr<!u8i> -> !cir.ptr<!rec_PR234523A3AB>
// X64:   %[[CALLEE:.*]] = cir.cast bitcast %[[FN_PTR]] : !cir.ptr<!void> -> !cir.ptr<!cir.func<(!cir.ptr<!void>)>>
// X64:   %[[ADJ_VOID:.*]] = cir.cast bitcast %[[ADJ_B]] : !cir.ptr<!rec_PR234523A3AB> -> !cir.ptr<!void>
// X64:   cir.call %[[CALLEE]](%[[ADJ_VOID]]) : (!cir.ptr<!cir.func<(!cir.ptr<!void>)>>, !cir.ptr<!void> {llvm.noundef}) -> ()
// X64:   cir.return
