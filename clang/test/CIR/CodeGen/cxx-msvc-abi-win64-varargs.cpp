// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -emit-cir %s -o - | FileCheck %s

enum class U8 : unsigned char { a, b };
enum class S8 : signed char { a, b };
enum class U16 : unsigned short { a, b };
enum class S16 : short { c, d };
enum class U32 : unsigned int { a, b };
enum ClassicU8 : unsigned char { cu_a };

extern "C" void foo(int x, ...);

// Fixed parameters should continue to be passed without extension.
// CHECK-LABEL: cir.func no_inline dso_local @fixed(%arg0: !u8i {llvm.noundef, llvm.zeroext}
extern "C" void fixed(U8) {}

// Named parameters of variadic functions are still fixed arguments.
// CHECK-LABEL: cir.func no_inline dso_local @named_variadic(%arg0: !u8i {llvm.noundef, llvm.zeroext} {{.*}}, ...)
extern "C" void named_variadic(U8, ...) {}

// CHECK-LABEL: cir.func no_inline dso_local @test(
// CHECK-SAME: %arg0: !u8i {llvm.noundef, llvm.zeroext}
// CHECK-SAME: %arg1: !s8i {llvm.noundef, llvm.signext}
// CHECK-SAME: %arg2: !u16i {llvm.noundef, llvm.zeroext}
// CHECK-SAME: %arg3: !s16i {llvm.noundef, llvm.signext}
// CHECK-SAME: %arg4: !u32i {llvm.noundef}
// CHECK-SAME: %arg5: !u8i {llvm.noundef, llvm.zeroext}
// CHECK-SAME: %arg6: !u8i {llvm.noundef, llvm.zeroext}
// CHECK:        %[[PLAIN_CAST:.*]] = cir.cast integral %{{.*}} : !u8i -> !s32i
// CHECK:        %[[CLASSIC_CAST:.*]] = cir.cast integral %{{.*}} : !u8i -> !s32i
// CHECK:        %[[U32:.*]] = cir.load {{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK:        %[[S16:.*]] = cir.load {{.*}} : !cir.ptr<!s16i>, !s16i
// CHECK:        %[[U16:.*]] = cir.load {{.*}} : !cir.ptr<!u16i>, !u16i
// CHECK:        %[[S8:.*]] = cir.load {{.*}} : !cir.ptr<!s8i>, !s8i
// CHECK:        %[[U8:.*]] = cir.load {{.*}} : !cir.ptr<!u8i>, !u8i
// CHECK:        %[[ZERO:.*]] = cir.const #cir.int<0> : !s32i
// Scoped enums do not undergo default argument promotions, so they retain their
// !u8i / !s8i / !u16i / !s16i IR types with llvm.zeroext / llvm.signext attributes.
// Regular integer types and unscoped enums undergo integer promotion to !s32i.
// CHECK:        cir.call @foo(%[[ZERO]], %[[U8]], %[[S8]], %[[U16]], %[[S16]], %[[U32]], %[[CLASSIC_CAST]], %[[PLAIN_CAST]]) : (!s32i {llvm.noundef}, !u8i {llvm.noundef, llvm.zeroext}, !s8i {llvm.noundef, llvm.signext}, !u16i {llvm.noundef, llvm.zeroext}, !s16i {llvm.noundef, llvm.signext}, !u32i {llvm.noundef}, !s32i {llvm.noundef}, !s32i {llvm.noundef}) -> ()
extern "C" void test(U8 u8, S8 s8, U16 u16, S16 s16, U32 u32,
                     ClassicU8 classic_u8, unsigned char plain_u8) {
  foo(0, u8, s8, u16, s16, u32, classic_u8, plain_u8);
}

// CHECK: cir.func private {{.*}}@foo(!s32i {llvm.noundef}, ...)

// CHECK-LABEL: cir.func no_inline dso_local @test_constants()
// CHECK:        %[[C_S16:.*]] = cir.const #cir.int<0> : !s16i
// CHECK:        %[[C_U8:.*]] = cir.const #cir.int<0> : !u8i
// CHECK:        %[[C_ZERO:.*]] = cir.const #cir.int<0> : !s32i
// CHECK:        cir.call @foo(%[[C_ZERO]], %[[C_U8]], %[[C_S16]]) : (!s32i {llvm.noundef}, !u8i {llvm.noundef, llvm.zeroext}, !s16i {llvm.noundef, llvm.signext}) -> ()
extern "C" void test_constants() {
  foo(0, U8::a, S16::c);
}
