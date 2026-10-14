// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -fms-volatile -fclangir -emit-cir %s -o - | FileCheck %s --check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -fms-volatile -fclangir -emit-llvm %s -o - | FileCheck %s --check-prefix=LLVM

void test(volatile int *p, volatile int *q) {
  *p = *q;
}

// CIR: cir.func {{.*}}test
// CIR: cir.load volatile align(4) atomic(acquire) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CIR: cir.store volatile align(4) atomic(release) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>

// LLVM: define {{.*}} @{{.*}}test
// LLVM: load atomic volatile i32, ptr %{{.*}} acquire, align 4
// LLVM: store atomic volatile i32 %{{.*}}, ptr %{{.*}} release, align 4
