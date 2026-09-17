// RUN: %clang_cc1 -std=c++17 -fno-rtti -fclangir -emit-cir -triple x86_64-pc-windows-msvc -Wno-inaccessible-base %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir --check-prefixes=CIR,CIR-X64 %s
// RUN: %clang_cc1 -std=c++17 -fno-rtti -fclangir -emit-cir -triple i386-pc-windows-msvc -Wno-inaccessible-base %s -o %t86.cir
// RUN: FileCheck --input-file=%t86.cir --check-prefixes=CIR,CIR-X86 %s

//===----------------------------------------------------------------------===//
// Module-level globals (VFTables)
//===----------------------------------------------------------------------===//

// test1: Single inheritance return adjustment vftable
// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7X@test1@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@X@test1@@QEAAPEAUB@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?z@D@test1@@UEAAXXZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@X@test1@@UEAAPEAUC@2@XZ"> : !cir.ptr<!u8i>]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7X@test1@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@X@test1@@QAEPAUB@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?z@D@test1@@UAEXXZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@X@test1@@UAEPAUC@2@XZ"> : !cir.ptr<!u8i>]>

// test2: Multi-level return adjustment vftable
// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7X@test2@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@X@test2@@QEAAPEAUB@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?z@D@test2@@UEAAXXZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@X@test2@@QEAAPEAUC@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@X@test2@@UEAAPEAUF@2@XZ"> : !cir.ptr<!u8i>]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7X@test2@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@X@test2@@QAEPAUB@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?z@D@test2@@UAEXXZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@X@test2@@QAEPAUC@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@X@test2@@UAEPAUF@2@XZ"> : !cir.ptr<!u8i>]>

// test4: Multiple non-virtual inheritance primary and secondary vftables
// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7X@test4@@6BD@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@X@test4@@QEAAPEAUB@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?z@D@test4@@UEAAXXZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@X@test4@@UEAAPEAUF@2@XZ"> : !cir.ptr<!u8i>]>
// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7X@test4@@6BE@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@X@test4@@W7EAAPEAUB@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?z@D@test4@@UEAAXXZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@X@test4@@W7EAAPEAUC@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@X@test4@@W7EAAPEAUF@2@XZ"> : !cir.ptr<!u8i>]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7X@test4@@6BD@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@X@test4@@QAEPAUB@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?z@D@test4@@UAEXXZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@X@test4@@UAEPAUF@2@XZ"> : !cir.ptr<!u8i>]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7X@test4@@6BE@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@X@test4@@W3AEPAUB@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?z@D@test4@@UAEXXZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@X@test4@@W3AEPAUC@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@X@test4@@W3AEPAUF@2@XZ"> : !cir.ptr<!u8i>]>

// test5: Shifted vfptr vftables
// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7X@test5@@6BA@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?g@A@test5@@UEAAXXZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?h@A@test5@@UEAAXXZ"> : !cir.ptr<!u8i>]>
// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7X@test5@@6BD@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@X@test5@@QEAAPEAUB@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?z@D@test5@@UEAAXXZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@X@test5@@UEAAPEAUC@2@XZ"> : !cir.ptr<!u8i>]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7X@test5@@6BA@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?g@A@test5@@UAEXXZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?h@A@test5@@UAEXXZ"> : !cir.ptr<!u8i>]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7X@test5@@6BD@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@X@test5@@QAEPAUB@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?z@D@test5@@UAEXXZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?foo@X@test5@@UAEPAUC@2@XZ"> : !cir.ptr<!u8i>]>

// pr20444: Diamond inheritance primary and secondary vftables
// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7C@pr20444@@6BA@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?f@C@pr20444@@UEAAPEAU12@XZ"> : !cir.ptr<!u8i>]>
// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7C@pr20444@@6BB@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?f@C@pr20444@@W7EAAPEAUB@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?f@C@pr20444@@W7EAAPEAU12@XZ"> : !cir.ptr<!u8i>]>
// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7D@pr20444@@6BA@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?f@D@pr20444@@UEAAPEAU12@XZ"> : !cir.ptr<!u8i>]>
// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7D@pr20444@@6BB@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?f@D@pr20444@@W7EAAPEAUB@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?f@D@pr20444@@W7EAAPEAUC@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?f@D@pr20444@@W7EAAPEAU12@XZ"> : !cir.ptr<!u8i>]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7C@pr20444@@6BA@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?f@C@pr20444@@UAEPAU12@XZ"> : !cir.ptr<!u8i>]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7C@pr20444@@6BB@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?f@C@pr20444@@W3AEPAUB@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?f@C@pr20444@@W3AEPAU12@XZ"> : !cir.ptr<!u8i>]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7D@pr20444@@6BA@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?f@D@pr20444@@UAEPAU12@XZ"> : !cir.ptr<!u8i>]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7D@pr20444@@6BB@1@@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?f@D@pr20444@@W3AEPAUB@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?f@D@pr20444@@W3AEPAUC@2@XZ"> : !cir.ptr<!u8i>, #cir.global_view<@"?f@D@pr20444@@W3AEPAU12@XZ"> : !cir.ptr<!u8i>]>

//===----------------------------------------------------------------------===//
// test1: Single inheritance return adjustment
//===----------------------------------------------------------------------===//
namespace test1 {
struct A { virtual void g(); virtual void h(); };
struct B { virtual void g(); };
struct C : A, B { virtual void g(); };
struct D { virtual B* foo(); virtual void z(); };
struct X : D {
  virtual C* foo();
  X();
};
X::X() {}

// CIR-X64-LABEL: cir.func no_inline comdat weak_odr dso_local @"?foo@X@test1@@QEAAPEAUB@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat weak_odr dso_local @"?foo@X@test1@@QAEPAUB@2@XZ"
// CIR:   %[[CALL_RES:[0-9]+]] = cir.call @"?foo@X@test1@@{{UEAAPEA|UAEPA}}UC@2@XZ"(%{{[0-9]+}})
// CIR:   %[[IS_NOT_NULL:[0-9]+]] = cir.cmp ne %[[CALL_RES]]
// CIR:   %{{[0-9]+}} = cir.ternary(%[[IS_NOT_NULL]], true {
// CIR:     %[[RES_PTR:[0-9]+]] = cir.cast bitcast %[[CALL_RES]] : !cir.ptr<{{.*}}> -> !cir.ptr<!u8i>
// CIR-X64: %[[ADJ_OFFS:[0-9]+]] = cir.const #cir.int<8> : !s32i
// CIR-X86: %[[ADJ_OFFS:[0-9]+]] = cir.const #cir.int<4> : !s32i
// CIR:     %[[ADJ_PTR:[0-9]+]] = cir.ptr_stride %[[RES_PTR]], %[[ADJ_OFFS]]
// CIR:     %[[CAST_BACK:[0-9]+]] = cir.cast bitcast %[[ADJ_PTR]]
// CIR:     cir.yield %[[CAST_BACK]]
// CIR:   }, false {
// CIR:     %[[NULL_PTR:[0-9]+]] = cir.const #cir.ptr<null>
// CIR:     cir.yield %[[NULL_PTR]]
// CIR:   })
// CIR:   cir.return %{{[0-9]+}}
} // namespace test1

//===----------------------------------------------------------------------===//
// test2: Multi-level return adjustment
//===----------------------------------------------------------------------===//
namespace test2 {
struct A { virtual void g(); virtual void h(); };
struct B { virtual void g(); };
struct C : A, B { virtual void g(); };
struct D { virtual B* foo(); virtual void z(); };
struct E : D { virtual C* foo(); };
struct F : C { };
struct X : E {
  virtual F* foo();
  X();
};
X::X() {}

// Thunk for C* return adjustment (0-byte return adjustment, direct forward).
// CIR-X64-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?foo@X@test2@@QEAAPEAUC@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?foo@X@test2@@QAEPAUC@2@XZ"
// CIR:   %[[RES:[0-9]+]] = cir.call @"?foo@X@test2@@{{UEAAPEA|UAEPA}}UF@2@XZ"(%{{[0-9]+}})
// CIR-NOT: cir.ternary
// CIR:   cir.return %{{[0-9]+}}
} // namespace test2

//===----------------------------------------------------------------------===//
// test4: Multiple non-virtual inheritance with secondary vftables
//===----------------------------------------------------------------------===//
namespace test4 {
struct A { virtual void g(); virtual void h(); };
struct B { virtual void g(); };
struct C : A, B { virtual void g(); };
struct D { virtual B* foo(); virtual void z(); };
struct E : D { virtual C* foo(); };
struct F : A, C { };
struct X : D, E {
  virtual F* foo();
  X();
};
X::X() {}

// Primary vftable thunk: return adjustment to B* (16 on x64, 8 on x86).
// CIR-X64-LABEL: cir.func no_inline comdat weak_odr dso_local @"?foo@X@test4@@QEAAPEAUB@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat weak_odr dso_local @"?foo@X@test4@@QAEPAUB@2@XZ"
// CIR:   %[[CALL_RES:[0-9]+]] = cir.call @"?foo@X@test4@@{{UEAAPEA|UAEPA}}UF@2@XZ"(%{{[0-9]+}})
// CIR:   %[[IS_NOT_NULL:[0-9]+]] = cir.cmp ne %[[CALL_RES]]
// CIR:   %{{[0-9]+}} = cir.ternary(%[[IS_NOT_NULL]], true {
// CIR:     %[[RES_PTR:[0-9]+]] = cir.cast bitcast %[[CALL_RES]] : !cir.ptr<{{.*}}> -> !cir.ptr<!u8i>
// CIR-X64: %[[ADJ_OFFS:[0-9]+]] = cir.const #cir.int<16> : !s32i
// CIR-X86: %[[ADJ_OFFS:[0-9]+]] = cir.const #cir.int<8> : !s32i
// CIR:     %[[ADJ_PTR:[0-9]+]] = cir.ptr_stride %[[RES_PTR]], %[[ADJ_OFFS]]
// CIR:     %[[CAST_BACK:[0-9]+]] = cir.cast bitcast %[[ADJ_PTR]]
// CIR:     cir.yield %[[CAST_BACK]]
// CIR:   }, false {
// CIR:     %[[NULL_PTR:[0-9]+]] = cir.const #cir.ptr<null>
// CIR:     cir.yield %[[NULL_PTR]]
// CIR:   })
// CIR:   cir.return %{{[0-9]+}}

// Secondary vftable thunk: this adjustment (-8 on x64, -4 on x86) + return adjustment to B* (16 on x64, 8 on x86).
// CIR-X64-LABEL: cir.func no_inline comdat weak_odr dso_local @"?foo@X@test4@@W7EAAPEAUB@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat weak_odr dso_local @"?foo@X@test4@@W3AEPAUB@2@XZ"
// CIR:   %[[THIS_PTR:[0-9]+]] = cir.cast bitcast %{{[0-9]+}} : !cir.ptr<{{.*}}> -> !cir.ptr<!u8i>
// CIR-X64: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-8> : !s32i
// CIR-X86: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-4> : !s32i
// CIR:   %[[ADJ_THIS:[0-9]+]] = cir.ptr_stride %[[THIS_PTR]], %[[THIS_OFFS]]
// CIR:   %[[CALL_RES:[0-9]+]] = cir.call @"?foo@X@test4@@{{UEAAPEA|UAEPA}}UF@2@XZ"(%{{[0-9]+}})
// CIR:   %[[IS_NOT_NULL:[0-9]+]] = cir.cmp ne %[[CALL_RES]]
// CIR:   %{{[0-9]+}} = cir.ternary(%[[IS_NOT_NULL]], true {
// CIR:     %[[RES_PTR:[0-9]+]] = cir.cast bitcast %[[CALL_RES]] : !cir.ptr<{{.*}}> -> !cir.ptr<!u8i>
// CIR-X64: %[[RET_OFFS:[0-9]+]] = cir.const #cir.int<16> : !s32i
// CIR-X86: %[[RET_OFFS:[0-9]+]] = cir.const #cir.int<8> : !s32i
// CIR:     %[[ADJ_PTR:[0-9]+]] = cir.ptr_stride %[[RES_PTR]], %[[RET_OFFS]]
// CIR:     %[[CAST_BACK:[0-9]+]] = cir.cast bitcast %[[ADJ_PTR]]
// CIR:     cir.yield %[[CAST_BACK]]
// CIR:   }, false {
// CIR:     %[[NULL_PTR:[0-9]+]] = cir.const #cir.ptr<null>
// CIR:     cir.yield %[[NULL_PTR]]
// CIR:   })
// CIR:   cir.return %{{[0-9]+}}

// Secondary vftable thunk: this adjustment (-8 on x64, -4 on x86) + return adjustment to C* (8 on x64, 4 on x86).
// CIR-X64-LABEL: cir.func no_inline comdat weak_odr dso_local @"?foo@X@test4@@W7EAAPEAUC@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat weak_odr dso_local @"?foo@X@test4@@W3AEPAUC@2@XZ"
// CIR:   %[[THIS_PTR:[0-9]+]] = cir.cast bitcast %{{[0-9]+}} : !cir.ptr<{{.*}}> -> !cir.ptr<!u8i>
// CIR-X64: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-8> : !s32i
// CIR-X86: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-4> : !s32i
// CIR:   %[[ADJ_THIS:[0-9]+]] = cir.ptr_stride %[[THIS_PTR]], %[[THIS_OFFS]]
// CIR:   %[[CALL_RES:[0-9]+]] = cir.call @"?foo@X@test4@@{{UEAAPEA|UAEPA}}UF@2@XZ"(%{{[0-9]+}})
// CIR:   %[[IS_NOT_NULL:[0-9]+]] = cir.cmp ne %[[CALL_RES]]
// CIR:   %{{[0-9]+}} = cir.ternary(%[[IS_NOT_NULL]], true {
// CIR:     %[[RES_PTR:[0-9]+]] = cir.cast bitcast %[[CALL_RES]] : !cir.ptr<{{.*}}> -> !cir.ptr<!u8i>
// CIR-X64: %[[RET_OFFS:[0-9]+]] = cir.const #cir.int<8> : !s32i
// CIR-X86: %[[RET_OFFS:[0-9]+]] = cir.const #cir.int<4> : !s32i
// CIR:     %[[ADJ_PTR:[0-9]+]] = cir.ptr_stride %[[RES_PTR]], %[[RET_OFFS]]
// CIR:     %[[CAST_BACK:[0-9]+]] = cir.cast bitcast %[[ADJ_PTR]]
// CIR:     cir.yield %[[CAST_BACK]]
// CIR:   }, false {
// CIR:     %[[NULL_PTR:[0-9]+]] = cir.const #cir.ptr<null>
// CIR:     cir.yield %[[NULL_PTR]]
// CIR:   })
// CIR:   cir.return %{{[0-9]+}}

// Secondary vftable thunk: this adjustment (-8 on x64, -4 on x86) with 0-byte return adjustment.
// CIR-X64-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?foo@X@test4@@W7EAAPEAUF@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?foo@X@test4@@W3AEPAUF@2@XZ"
// CIR:   %[[THIS_PTR:[0-9]+]] = cir.cast bitcast %{{[0-9]+}} : !cir.ptr<{{.*}}> -> !cir.ptr<!u8i>
// CIR-X64: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-8> : !s32i
// CIR-X86: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-4> : !s32i
// CIR:   %[[ADJ_THIS:[0-9]+]] = cir.ptr_stride %[[THIS_PTR]], %[[THIS_OFFS]]
// CIR:   %[[RES:[0-9]+]] = cir.call @"?foo@X@test4@@{{UEAAPEA|UAEPA}}UF@2@XZ"(%{{[0-9]+}})
// CIR-NOT: cir.ternary
// CIR:   cir.return %{{[0-9]+}}
} // namespace test4

//===----------------------------------------------------------------------===//
// test5: Shifted vfptr
//===----------------------------------------------------------------------===//
namespace test5 {
struct A { virtual void g(); virtual void h(); };
struct B { virtual void g(); };
struct C : A, B { virtual void g(); };
struct D { virtual B* foo(); virtual void z(); };
struct X : A, D {
  virtual C* foo();
  X();
};
X::X() {}

// Thunk for return adjustment to B* (8 on x64, 4 on x86).
// CIR-X64-LABEL: cir.func no_inline comdat weak_odr dso_local @"?foo@X@test5@@QEAAPEAUB@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat weak_odr dso_local @"?foo@X@test5@@QAEPAUB@2@XZ"
// CIR:   %[[CALL_RES:[0-9]+]] = cir.call @"?foo@X@test5@@{{UEAAPEA|UAEPA}}UC@2@XZ"(%{{[0-9]+}})
// CIR:   %[[IS_NOT_NULL:[0-9]+]] = cir.cmp ne %[[CALL_RES]]
// CIR:   %{{[0-9]+}} = cir.ternary(%[[IS_NOT_NULL]], true {
// CIR:     %[[RES_PTR:[0-9]+]] = cir.cast bitcast %[[CALL_RES]] : !cir.ptr<{{.*}}> -> !cir.ptr<!u8i>
// CIR-X64: %[[RET_OFFS:[0-9]+]] = cir.const #cir.int<8> : !s32i
// CIR-X86: %[[RET_OFFS:[0-9]+]] = cir.const #cir.int<4> : !s32i
// CIR:     %[[ADJ_PTR:[0-9]+]] = cir.ptr_stride %[[RES_PTR]], %[[RET_OFFS]]
// CIR:     %[[CAST_BACK:[0-9]+]] = cir.cast bitcast %[[ADJ_PTR]]
// CIR:     cir.yield %[[CAST_BACK]]
// CIR:   }, false {
// CIR:     %[[NULL_PTR:[0-9]+]] = cir.const #cir.ptr<null>
// CIR:     cir.yield %[[NULL_PTR]]
// CIR:   })
// CIR:   cir.return %{{[0-9]+}}
} // namespace test5

//===----------------------------------------------------------------------===//
// pr20444: Diamond inheritance return adjustments & secondary vftables
//===----------------------------------------------------------------------===//
namespace pr20444 {
struct A { virtual A* f(); };
struct B { virtual B* f(); };
struct C : A, B {
  virtual C* f();
  C();
};
C::C() {}

struct D : C {
  virtual D* f();
  D();
};
D::D() {}

// C's secondary vftable thunk: this adjustment (-8 on x64, -4 on x86) + return adjustment to B* (8 on x64, 4 on x86).
// CIR-X64-LABEL: cir.func no_inline comdat weak_odr dso_local @"?f@C@pr20444@@W7EAAPEAUB@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat weak_odr dso_local @"?f@C@pr20444@@W3AEPAUB@2@XZ"
// CIR:   %[[THIS_PTR:[0-9]+]] = cir.cast bitcast %{{[0-9]+}} : !cir.ptr<{{.*}}> -> !cir.ptr<!u8i>
// CIR-X64: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-8> : !s32i
// CIR-X86: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-4> : !s32i
// CIR:   %[[ADJ_THIS:[0-9]+]] = cir.ptr_stride %[[THIS_PTR]], %[[THIS_OFFS]]
// CIR:   %[[CALL_RES:[0-9]+]] = cir.call @"?f@C@pr20444@@{{UEAAPEA|UAEPA}}U12@XZ"(%{{[0-9]+}})
// CIR:   %[[IS_NOT_NULL:[0-9]+]] = cir.cmp ne %[[CALL_RES]]
// CIR:   %{{[0-9]+}} = cir.ternary(%[[IS_NOT_NULL]], true {
// CIR:     %[[RES_PTR:[0-9]+]] = cir.cast bitcast %[[CALL_RES]] : !cir.ptr<{{.*}}> -> !cir.ptr<!u8i>
// CIR-X64: %[[RET_OFFS:[0-9]+]] = cir.const #cir.int<8> : !s32i
// CIR-X86: %[[RET_OFFS:[0-9]+]] = cir.const #cir.int<4> : !s32i
// CIR:     %[[ADJ_PTR:[0-9]+]] = cir.ptr_stride %[[RES_PTR]], %[[RET_OFFS]]
// CIR:     %[[CAST_BACK:[0-9]+]] = cir.cast bitcast %[[ADJ_PTR]]
// CIR:     cir.yield %[[CAST_BACK]]
// CIR:   }, false {
// CIR:     %[[NULL_PTR:[0-9]+]] = cir.const #cir.ptr<null>
// CIR:     cir.yield %[[NULL_PTR]]
// CIR:   })
// CIR:   cir.return %{{[0-9]+}}

// C's secondary vftable thunk: this adjustment (-8 on x64, -4 on x86) + 0-byte return adjustment.
// CIR-X64-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?f@C@pr20444@@W7EAAPEAU12@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?f@C@pr20444@@W3AEPAU12@XZ"
// CIR:   %[[THIS_PTR:[0-9]+]] = cir.cast bitcast %{{[0-9]+}} : !cir.ptr<{{.*}}> -> !cir.ptr<!u8i>
// CIR-X64: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-8> : !s32i
// CIR-X86: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-4> : !s32i
// CIR:   %[[ADJ_THIS:[0-9]+]] = cir.ptr_stride %[[THIS_PTR]], %[[THIS_OFFS]]
// CIR:   %[[RES:[0-9]+]] = cir.call @"?f@C@pr20444@@{{UEAAPEA|UAEPA}}U12@XZ"(%{{[0-9]+}})
// CIR-NOT: cir.ternary
// CIR:   cir.return %{{[0-9]+}}

// D's secondary vftable thunk: this adjustment (-8 on x64, -4 on x86) + return adjustment to B* (8 on x64, 4 on x86).
// CIR-X64-LABEL: cir.func no_inline comdat weak_odr dso_local @"?f@D@pr20444@@W7EAAPEAUB@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat weak_odr dso_local @"?f@D@pr20444@@W3AEPAUB@2@XZ"
// CIR:   %[[THIS_PTR:[0-9]+]] = cir.cast bitcast %{{[0-9]+}} : !cir.ptr<{{.*}}> -> !cir.ptr<!u8i>
// CIR-X64: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-8> : !s32i
// CIR-X86: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-4> : !s32i
// CIR:   %[[ADJ_THIS:[0-9]+]] = cir.ptr_stride %[[THIS_PTR]], %[[THIS_OFFS]]
// CIR:   %[[CALL_RES:[0-9]+]] = cir.call @"?f@D@pr20444@@{{UEAAPEA|UAEPA}}U12@XZ"(%{{[0-9]+}})
// CIR:   %[[IS_NOT_NULL:[0-9]+]] = cir.cmp ne %[[CALL_RES]]
// CIR:   %{{[0-9]+}} = cir.ternary(%[[IS_NOT_NULL]], true {
// CIR:     %[[RES_PTR:[0-9]+]] = cir.cast bitcast %[[CALL_RES]] : !cir.ptr<{{.*}}> -> !cir.ptr<!u8i>
// CIR-X64: %[[RET_OFFS:[0-9]+]] = cir.const #cir.int<8> : !s32i
// CIR-X86: %[[RET_OFFS:[0-9]+]] = cir.const #cir.int<4> : !s32i
// CIR:     %[[ADJ_PTR:[0-9]+]] = cir.ptr_stride %[[RES_PTR]], %[[RET_OFFS]]
// CIR:     %[[CAST_BACK:[0-9]+]] = cir.cast bitcast %[[ADJ_PTR]]
// CIR:     cir.yield %[[CAST_BACK]]
// CIR:   }, false {
// CIR:     %[[NULL_PTR:[0-9]+]] = cir.const #cir.ptr<null>
// CIR:     cir.yield %[[NULL_PTR]]
// CIR:   })
// CIR:   cir.return %{{[0-9]+}}

// D's secondary vftable thunk: this adjustment (-8 on x64, -4 on x86) + 0-byte return adjustment to C*.
// CIR-X64-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?f@D@pr20444@@W7EAAPEAUC@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?f@D@pr20444@@W3AEPAUC@2@XZ"
// CIR:   %[[THIS_PTR:[0-9]+]] = cir.cast bitcast %{{[0-9]+}} : !cir.ptr<{{.*}}> -> !cir.ptr<!u8i>
// CIR-X64: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-8> : !s32i
// CIR-X86: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-4> : !s32i
// CIR:   %[[ADJ_THIS:[0-9]+]] = cir.ptr_stride %[[THIS_PTR]], %[[THIS_OFFS]]
// CIR:   %[[RES:[0-9]+]] = cir.call @"?f@D@pr20444@@{{UEAAPEA|UAEPA}}U12@XZ"(%{{[0-9]+}})
// CIR-NOT: cir.ternary
// CIR:   cir.return %{{[0-9]+}}

// D's secondary vftable thunk: this adjustment (-8 on x64, -4 on x86) + 0-byte return adjustment to D*.
// CIR-X64-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?f@D@pr20444@@W7EAAPEAU12@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?f@D@pr20444@@W3AEPAU12@XZ"
// CIR:   %[[THIS_PTR:[0-9]+]] = cir.cast bitcast %{{[0-9]+}} : !cir.ptr<{{.*}}> -> !cir.ptr<!u8i>
// CIR-X64: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-8> : !s32i
// CIR-X86: %[[THIS_OFFS:[0-9]+]] = cir.const #cir.int<-4> : !s32i
// CIR:   %[[ADJ_THIS:[0-9]+]] = cir.ptr_stride %[[THIS_PTR]], %[[THIS_OFFS]]
// CIR:   %[[RES:[0-9]+]] = cir.call @"?f@D@pr20444@@{{UEAAPEA|UAEPA}}U12@XZ"(%{{[0-9]+}})
// CIR-NOT: cir.ternary
// CIR:   cir.return %{{[0-9]+}}
} // namespace pr20444
