// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fms-extensions -fclangir -emit-cir %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir --check-prefix=CIR-X64 %s
// RUN: %clang_cc1 -std=c++17 -triple i386-pc-windows-msvc -fms-extensions -fclangir -emit-cir %s -o %t32.cir
// RUN: FileCheck --input-file=%t32.cir --check-prefix=CIR-X86 %s

// Adapted from Clang CodeGenCXX test: msvc-vector-deleting-dtors-sized-delete.cpp

//===----------------------------------------------------------------------===//
// Module-level Globals (emitted at top of MLIR module)
//===----------------------------------------------------------------------===//

// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7Test@@6B@" = #cir.vtable<{#cir.const_array<[#cir.global_view<@"??_R0?AUTest@@@8"> : !cir.ptr<!void>, #cir.global_view<@"??_ETest@@UEAAPEAXI@Z"> : !cir.ptr<!u8i>]> : !cir.array<!cir.ptr<!u8i> x 2>}>
// CIR-X64-DAG: cir.global "private" constant external @"??_R0?AUTest@@@8" : !cir.ptr<!void>
// CIR-X64-DAG: cir.global "private" appending @llvm.used = #cir.const_array<[#cir.global_view<@"?__empty_global_delete@@YAXPEAX_K@Z"> : !cir.ptr<!void>]>

// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7Test@@6B@" = #cir.vtable<{#cir.const_array<[#cir.global_view<@"??_R0?AUTest@@@8"> : !cir.ptr<!void>, #cir.global_view<@"??_ETest@@UAEPAXI@Z"> : !cir.ptr<!u8i>]> : !cir.array<!cir.ptr<!u8i> x 2>}>
// CIR-X86-DAG: cir.global "private" constant external @"??_R0?AUTest@@@8" : !cir.ptr<!void>
// CIR-X86-DAG: cir.global "private" appending @llvm.used = #cir.const_array<[#cir.global_view<@"?__empty_global_delete@@YAXPAXI@Z"> : !cir.ptr<!void>]>

using size_t = __SIZE_TYPE__;
void operator delete[](void *ptr, size_t sz) {}

struct Test {
  virtual ~Test() {}
  void operator delete[](void *ptr, size_t sz) {}
  int x;
  int y;
};

void test() {
  Test *a = new Test[10];
  delete[] a;
}

//===----------------------------------------------------------------------===//
// Vector Deleting Destructor Sized Deallocation
//===----------------------------------------------------------------------===//

// CIR-X64-LABEL: cir.func {{.*}} @"??_ETest@@UEAAPEAXI@Z"(%arg0: !cir.ptr<!rec_Test> {{.*}}, %arg1: !s32i {{.*}}) -> (!cir.ptr<!void>
// CIR-X64:   %[[TWO:.*]] = cir.const #cir.int<2> : !s32i
// CIR-X64:   %[[AND_VEC:.*]] = cir.and %{{.*}}, %[[TWO]] : !s32i
// CIR-X64:   %[[IS_VEC:.*]] = cir.cmp ne %[[AND_VEC]], %{{.*}} : !s32i
// CIR-X64:   cir.if %[[IS_VEC]] {
// CIR-X64:     %[[BYTE_PTR:.*]] = cir.cast bitcast %{{.*}} : !cir.ptr<!rec_Test> -> !cir.ptr<!u8i>
// CIR-X64:     %[[NEG_COOKIE:.*]] = cir.const #cir.int<-8> : !s32i
// CIR-X64:     %[[COOKIE_PTR:.*]] = cir.ptr_stride %[[BYTE_PTR]], %[[NEG_COOKIE]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>
// CIR-X64:     %[[COOKIE_SIZE_PTR:.*]] = cir.cast bitcast %[[COOKIE_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!u64i>
// CIR-X64:     %[[HOWMANY:.*]] = cir.load align(8) %[[COOKIE_SIZE_PTR]] : !cir.ptr<!u64i>, !u64i
// CIR-X64:     cir.if %{{.*}} {
// CIR-X64:       cir.if %{{.*}} {
// CIR-X64:         %[[RAW_PTR1:.*]] = cir.cast bitcast %[[COOKIE_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR-X64:         %[[ELEM_SIZE1:.*]] = cir.const #cir.int<16> : !u64i
// CIR-X64:         %[[ARRAY_SIZE1:.*]] = cir.mul %[[ELEM_SIZE1]], %[[HOWMANY]] : !u64i
// CIR-X64:         %[[COOKIE_SIZE1:.*]] = cir.const #cir.int<8> : !u64i
// CIR-X64:         %[[TOTAL_SIZE1:.*]] = cir.add %[[ARRAY_SIZE1]], %[[COOKIE_SIZE1]] : !u64i
// CIR-X64:         cir.call @"?__global_array_delete@@YAXPEAX_K@Z"(%[[RAW_PTR1]], %[[TOTAL_SIZE1]]) nothrow : (!cir.ptr<!void> {llvm.noundef}, !u64i {llvm.noundef}) -> ()
// CIR-X64:       } else {
// CIR-X64:         %[[RAW_PTR2:.*]] = cir.cast bitcast %[[COOKIE_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR-X64:         %[[ELEM_SIZE2:.*]] = cir.const #cir.int<16> : !u64i
// CIR-X64:         %[[ARRAY_SIZE2:.*]] = cir.mul %[[ELEM_SIZE2]], %[[HOWMANY]] : !u64i
// CIR-X64:         %[[COOKIE_SIZE2:.*]] = cir.const #cir.int<8> : !u64i
// CIR-X64:         %[[TOTAL_SIZE2:.*]] = cir.add %[[ARRAY_SIZE2]], %[[COOKIE_SIZE2]] : !u64i
// CIR-X64:         cir.call @"??_VTest@@SAXPEAX_K@Z"(%[[RAW_PTR2]], %[[TOTAL_SIZE2]]) nothrow : (!cir.ptr<!void> {llvm.noundef}, !u64i {llvm.noundef}) -> ()
// CIR-X64:       }

// CIR-X64: cir.func weak private @"?__global_array_delete@@YAXPEAX_K@Z"(!cir.ptr<!void>, !u64i) alias(@"?__empty_global_delete@@YAXPEAX_K@Z")

// CIR-X86-LABEL: cir.func {{.*}} @"??_ETest@@UAEPAXI@Z"(%arg0: !cir.ptr<!rec_Test> {{.*}}, %arg1: !s32i {{.*}}) -> (!cir.ptr<!void>
// CIR-X86:   %[[TWO:.*]] = cir.const #cir.int<2> : !s32i
// CIR-X86:   %[[AND_VEC:.*]] = cir.and %{{.*}}, %[[TWO]] : !s32i
// CIR-X86:   %[[IS_VEC:.*]] = cir.cmp ne %[[AND_VEC]], %{{.*}} : !s32i
// CIR-X86:   cir.if %[[IS_VEC]] {
// CIR-X86:     %[[BYTE_PTR:.*]] = cir.cast bitcast %{{.*}} : !cir.ptr<!rec_Test> -> !cir.ptr<!u8i>
// CIR-X86:     %[[NEG_COOKIE:.*]] = cir.const #cir.int<-4> : !s32i
// CIR-X86:     %[[COOKIE_PTR:.*]] = cir.ptr_stride %[[BYTE_PTR]], %[[NEG_COOKIE]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>
// CIR-X86:     %[[COOKIE_SIZE_PTR:.*]] = cir.cast bitcast %[[COOKIE_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!u32i>
// CIR-X86:     %[[HOWMANY:.*]] = cir.load align(4) %[[COOKIE_SIZE_PTR]] : !cir.ptr<!u32i>, !u32i
// CIR-X86:     cir.if %{{.*}} {
// CIR-X86:       cir.if %{{.*}} {
// CIR-X86:         %[[RAW_PTR1:.*]] = cir.cast bitcast %[[COOKIE_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR-X86:         %[[ELEM_SIZE1:.*]] = cir.const #cir.int<12> : !u32i
// CIR-X86:         %[[ARRAY_SIZE1:.*]] = cir.mul %[[ELEM_SIZE1]], %[[HOWMANY]] : !u32i
// CIR-X86:         %[[COOKIE_SIZE1:.*]] = cir.const #cir.int<4> : !u32i
// CIR-X86:         %[[TOTAL_SIZE1:.*]] = cir.add %[[ARRAY_SIZE1]], %[[COOKIE_SIZE1]] : !u32i
// CIR-X86:         cir.call @"?__global_array_delete@@YAXPAXI@Z"(%[[RAW_PTR1]], %[[TOTAL_SIZE1]]) nothrow : (!cir.ptr<!void> {llvm.noundef}, !u32i {llvm.noundef}) -> ()
// CIR-X86:       } else {
// CIR-X86:         %[[RAW_PTR2:.*]] = cir.cast bitcast %[[COOKIE_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR-X86:         %[[ELEM_SIZE2:.*]] = cir.const #cir.int<12> : !u32i
// CIR-X86:         %[[ARRAY_SIZE2:.*]] = cir.mul %[[ELEM_SIZE2]], %[[HOWMANY]] : !u32i
// CIR-X86:         %[[COOKIE_SIZE2:.*]] = cir.const #cir.int<4> : !u32i
// CIR-X86:         %[[TOTAL_SIZE2:.*]] = cir.add %[[ARRAY_SIZE2]], %[[COOKIE_SIZE2]] : !u32i
// CIR-X86:         cir.call @"??_VTest@@SAXPAXI@Z"(%[[RAW_PTR2]], %[[TOTAL_SIZE2]]) nothrow : (!cir.ptr<!void> {llvm.noundef}, !u32i {llvm.noundef}) -> ()
// CIR-X86:       }

// CIR-X86: cir.func weak private @"?__global_array_delete@@YAXPAXI@Z"(!cir.ptr<!void>, !u32i) alias(@"?__empty_global_delete@@YAXPAXI@Z")
