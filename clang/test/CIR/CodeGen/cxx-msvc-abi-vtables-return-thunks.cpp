// RUN: %clang_cc1 -std=c++17 -fno-rtti -triple x86_64-pc-windows-msvc -fclangir -emit-cir %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir --check-prefixes=CIR,CIR-X64 %s
// RUN: %clang_cc1 -std=c++17 -fno-rtti -triple i386-pc-windows-msvc -fclangir -emit-cir %s -o %t32.cir
// RUN: FileCheck --input-file=%t32.cir --check-prefixes=CIR,CIR-X86 %s

//===----------------------------------------------------------------------===//
// Module-level Globals (emitted at top of MLIR module)
//===----------------------------------------------------------------------===//

// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7J@test1@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@J@test1@@QEAAPEAUB@2@XZ">{{.*}}#cir.global_view<@"?foo@J@test1@@QEAAPEAUC@2@XZ">{{.*}}#cir.global_view<@"?foo@J@test1@@UEAAPEAUD@2@XZ">{{.*}}]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7J@test1@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@J@test1@@QAEPAUB@2@XZ">{{.*}}#cir.global_view<@"?foo@J@test1@@QAEPAUC@2@XZ">{{.*}}#cir.global_view<@"?foo@J@test1@@UAEPAUD@2@XZ">{{.*}}]>

// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7K@test1@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@K@test1@@QEAAPEAUB@2@XZ">{{.*}}#cir.global_view<@"?foo@K@test1@@QEAAPEAUC@2@XZ">{{.*}}#cir.global_view<@"?foo@K@test1@@QEAAPEAUD@2@XZ">{{.*}}#cir.global_view<@"?foo@K@test1@@UEAAPEAUE@2@XZ">{{.*}}]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7K@test1@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@K@test1@@QAEPAUB@2@XZ">{{.*}}#cir.global_view<@"?foo@K@test1@@QAEPAUC@2@XZ">{{.*}}#cir.global_view<@"?foo@K@test1@@QAEPAUD@2@XZ">{{.*}}#cir.global_view<@"?foo@K@test1@@UAEPAUE@2@XZ">{{.*}}]>

// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7J@test2@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@J@test2@@QEAAPEAUB@2@XZ">{{.*}}#cir.global_view<@"?foo@J@test2@@UEAAPEAUD@2@XZ">{{.*}}]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7J@test2@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@J@test2@@QAEPAUB@2@XZ">{{.*}}#cir.global_view<@"?foo@J@test2@@UAEPAUD@2@XZ">{{.*}}]>

// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7K@test2@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@K@test2@@QEAAPEAUB@2@XZ">{{.*}}#cir.global_view<@"?foo@K@test2@@QEAAPEAUD@2@XZ">{{.*}}#cir.global_view<@"?foo@K@test2@@UEAAPEAUE@2@XZ">{{.*}}]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7K@test2@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@K@test2@@QAEPAUB@2@XZ">{{.*}}#cir.global_view<@"?foo@K@test2@@QAEPAUD@2@XZ">{{.*}}#cir.global_view<@"?foo@K@test2@@UAEPAUE@2@XZ">{{.*}}]>

// CIR-X64-DAG: cir.global constant linkonce_odr comdat dso_local @"??_8C@pr20479@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<8> : !s32i]>
// CIR-X86-DAG: cir.global constant linkonce_odr comdat dso_local @"??_8C@pr20479@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<4> : !s32i]>
// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7C@pr20479@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?f@B@pr20479@@QEAAPEAUA@2@XZ">{{.*}}#cir.global_view<@"?f@B@pr20479@@UEAAPEAU12@XZ">{{.*}}]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7C@pr20479@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?f@B@pr20479@@QAEPAUA@2@XZ">{{.*}}#cir.global_view<@"?f@B@pr20479@@UAEPAU12@XZ">{{.*}}]>

// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7C@pr21073@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?f@B@pr21073@@WPPPPPPPA@EAAPEAUA@2@XZ">{{.*}}#cir.global_view<@"?f@B@pr21073@@WPPPPPPPA@EAAPEAU12@XZ">{{.*}}]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7C@pr21073@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?f@B@pr21073@@WPPPPPPPI@AEPAUA@2@XZ">{{.*}}#cir.global_view<@"?f@B@pr21073@@WPPPPPPPI@AEPAU12@XZ">{{.*}}]>

// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7D@pr21073_2@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@C@pr21073_2@@QEAAPEAUA@2@XZ">{{.*}}#cir.global_view<@"?foo@C@pr21073_2@@UEAAPEAU12@XZ">{{.*}}]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7D@pr21073_2@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?foo@C@pr21073_2@@QAEPAUA@2@XZ">{{.*}}#cir.global_view<@"?foo@C@pr21073_2@@UAEPAU12@XZ">{{.*}}]>

// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7D@test3@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?fn@D@test3@@$4PPPPPPPM@A@EAAPEAUA@2@XZ">{{.*}}#cir.global_view<@"?fn@D@test3@@$4PPPPPPPM@A@EAAPEAUB@2@XZ">{{.*}}#cir.global_view<@"?fn@D@test3@@$4PPPPPPPM@A@EAAPEAU12@XZ">{{.*}}]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7D@test3@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"?fn@D@test3@@$4PPPPPPPM@A@AEPAUA@2@XZ">{{.*}}#cir.global_view<@"?fn@D@test3@@$4PPPPPPPM@A@AEPAUB@2@XZ">{{.*}}#cir.global_view<@"?fn@D@test3@@$4PPPPPPPM@A@AEPAU12@XZ">{{.*}}]>

// CIR-X64-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7C@pr34302@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"??_EC@pr34302@@UEAAPEAXI@Z">{{.*}}#cir.global_view<@"?f@C@pr34302@@QEAAPEAUB@2@XZ">{{.*}}#cir.global_view<@"?f@C@pr34302@@UEAAPEAU12@XZ">{{.*}}]>
// CIR-X86-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7C@pr34302@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}#cir.global_view<@"??_EC@pr34302@@UAEPAXI@Z">{{.*}}#cir.global_view<@"?f@C@pr34302@@QAEPAUB@2@XZ">{{.*}}#cir.global_view<@"?f@C@pr34302@@UAEPAU12@XZ">{{.*}}]>

namespace test1 {

// Some covariant types.
struct A { int a; };
struct B { int b; };
struct C : A, B { int c; };
struct D : C { int d; };
struct E : D { int e; };

// One base class and two overrides, all with covariant return types.
struct H     { virtual B *foo(); };
struct I : H { virtual C *foo(); };
struct J : I { virtual D *foo(); J(); };
struct K : J { virtual E *foo(); K(); };

J::J() {}
K::K() {}

// This thunk has a return adjustment (B* is at offset 4 inside E).
// CIR-X64-LABEL: cir.func no_inline comdat weak_odr dso_local @"?foo@K@test1@@QEAAPEAUB@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat weak_odr dso_local @"?foo@K@test1@@QAEPAUB@2@XZ"
// CIR:   %[[CALL_RES:[0-9]+]] = cir.call @"?foo@K@test1@@{{UAEPAUE|UEAAPEAUE}}@2@XZ"
// CIR:   %[[NULL_PTR:[0-9]+]] = cir.const #cir.ptr<null>
// CIR:   %[[IS_NOT_NULL:[0-9]+]] = cir.cmp ne %[[CALL_RES]], %[[NULL_PTR]]
// CIR:   %[[TERNARY_RES:[0-9]+]] = cir.ternary(%[[IS_NOT_NULL]], true {
// CIR:     %[[RAW_PTR:[0-9]+]] = cir.cast bitcast %[[CALL_RES]]
// CIR:     %[[DELTA:[0-9]+]] = cir.const #cir.int<4> : !s32i
// CIR:     %[[ADJ_PTR:[0-9]+]] = cir.ptr_stride %[[RAW_PTR]], %[[DELTA]]
// CIR:     %[[CAST_BACK:[0-9]+]] = cir.cast bitcast %[[ADJ_PTR]]
// CIR:     cir.yield %[[CAST_BACK]]
// CIR:   }, false {
// CIR:     %[[NULL_RET:[0-9]+]] = cir.const #cir.ptr<null>
// CIR:     cir.yield %[[NULL_RET]]
// CIR:   })
// CIR:   cir.return %{{[0-9]+}}

// These two thunks have 0-offset return adjustments (C* and D* start at offset 0).
// CIR-X64-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?foo@K@test1@@QEAAPEAUC@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?foo@K@test1@@QAEPAUC@2@XZ"
// CIR:   cir.call @"?foo@K@test1@@{{UAEPAUE|UEAAPEAUE}}@2@XZ"
// CIR-NOT: cir.ternary
// CIR-NOT: cir.ptr_stride
// CIR:   cir.return %{{[0-9]+}}

// CIR-X64-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?foo@K@test1@@QEAAPEAUD@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?foo@K@test1@@QAEPAUD@2@XZ"
// CIR:   cir.call @"?foo@K@test1@@{{UAEPAUE|UEAAPEAUE}}@2@XZ"
// CIR-NOT: cir.ternary
// CIR-NOT: cir.ptr_stride
// CIR:   cir.return %{{[0-9]+}}

} // namespace test1

namespace test2 {

// Covariant types. D* is not trivially convertible to C*.
struct A { int a; };
struct B { int b; };
struct C : B { int c; };
struct D : A, C { int d; };
struct E : D { int e; };

struct H     { virtual B *foo(); };
struct I : H { virtual C *foo(); };
struct J : I { virtual D *foo(); J(); };
struct K : J { virtual E *foo(); K(); };

J::J() {}
K::K() {}

} // namespace test2

namespace pr20479 {
struct A {
  virtual A *f();
};

struct B : virtual A {
  virtual B *f();
};

struct C : virtual A, B {
  C();
};

C::C() {}

// Covariant return adjustment with virtual base (vbase #1).
// CIR-X64-LABEL: cir.func no_inline comdat weak_odr dso_local @"?f@B@pr20479@@QEAAPEAUA@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat weak_odr dso_local @"?f@B@pr20479@@QAEPAUA@2@XZ"
// CIR:   %[[CALL_RES:[0-9]+]] = cir.call @"?f@B@pr20479@@{{UAEPAU12|UEAAPEAU12}}@XZ"
// CIR:   %[[IS_NOT_NULL:[0-9]+]] = cir.cmp ne %[[CALL_RES]]
// CIR:   %[[TERNARY_RES:[0-9]+]] = cir.ternary(%[[IS_NOT_NULL]], true {
// CIR:     %[[RAW_PTR:[0-9]+]] = cir.cast bitcast %[[CALL_RES]]
// CIR:     %[[VBPTR_OFFS:[0-9]+]] = cir.const #cir.int<0> : !s32i
// CIR:     %[[VBPTR:[0-9]+]] = cir.ptr_stride %[[RAW_PTR]], %[[VBPTR_OFFS]]
// CIR:     cir.cast bitcast %[[VBPTR]]
// CIR:     cir.load
// CIR:     cir.cast bitcast
// CIR:     %[[VBINDEX:[0-9]+]] = cir.const #cir.int<1> : !s32i
// CIR:     %[[VBASE_OFFS_PTR:[0-9]+]] = cir.ptr_stride %{{[0-9]+}}, %[[VBINDEX]]
// CIR:     %[[VBASE_OFFS:[0-9]+]] = cir.load {{.*}} %[[VBASE_OFFS_PTR]]
// CIR:     %[[ADJ:[0-9]+]] = cir.ptr_stride %[[VBPTR]], %[[VBASE_OFFS]]
// CIR:     cir.yield %{{[0-9]+}}
// CIR:   }, false {
// CIR:     cir.yield
// CIR:   })
// CIR:   cir.return %{{[0-9]+}}

} // namespace pr20479

namespace pr21073 {
struct A {
  virtual A *f();
};

struct B : virtual A {
  virtual B *f();
};

struct C : virtual A, virtual B {
  C();
};

C::C() {}

// This adjustment combined with virtual base return adjustment.
// CIR-X64-LABEL: cir.func no_inline comdat weak_odr dso_local @"?f@B@pr21073@@WPPPPPPPA@EAAPEAUA@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat weak_odr dso_local @"?f@B@pr21073@@WPPPPPPPI@AEPAUA@2@XZ"
// CIR-X64: %[[THIS_DELTA:[0-9]+]] = cir.const #cir.int<16> : !s32i
// CIR-X86: %[[THIS_DELTA:[0-9]+]] = cir.const #cir.int<8> : !s32i
// CIR:     %[[ADJ_THIS:[0-9]+]] = cir.ptr_stride %{{[0-9]+}}, %[[THIS_DELTA]]
// CIR:     %[[CALL_RES:[0-9]+]] = cir.call @"?f@B@pr21073@@{{UAEPAU12|UEAAPEAU12}}@XZ"(%{{.*}})
// CIR:     %[[IS_NOT_NULL:[0-9]+]] = cir.cmp ne %[[CALL_RES]]
// CIR:     %{{[0-9]+}} = cir.ternary(%[[IS_NOT_NULL]], true {
// CIR:       %[[VBINDEX:[0-9]+]] = cir.const #cir.int<1> : !s32i
// CIR:       %[[VBASE_OFFS_PTR:[0-9]+]] = cir.ptr_stride %{{[0-9]+}}, %[[VBINDEX]]
// CIR:       %[[VBASE_OFFS:[0-9]+]] = cir.load {{.*}} %[[VBASE_OFFS_PTR]]
// CIR:       cir.ptr_stride %{{.*}}, %[[VBASE_OFFS]]
// CIR:       cir.yield
// CIR:     }, false {
// CIR:       cir.yield
// CIR:     })
// CIR:     cir.return %{{[0-9]+}}

} // namespace pr21073

namespace pr21073_2 {
struct A { virtual A *foo(); };
struct B : virtual A {};
struct C : virtual A { virtual C *foo(); };
struct D : B, C { D(); };
D::D() {}
} // namespace pr21073_2

namespace test3 {
struct A { virtual A *fn(); };
struct B : virtual A { virtual B *fn(); };
struct X : virtual B {};
struct Y : virtual B {};
struct C : X, Y {};
struct D : virtual B, virtual A, C {
  D *fn();
  D();
};
D::D() {}

// vtordisp this-adjustment combined with virtual base return adjustment (vbase #1).
// CIR-X64-LABEL: cir.func no_inline comdat weak_odr dso_local @"?fn@D@test3@@$4PPPPPPPM@A@EAAPEAUA@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat weak_odr dso_local @"?fn@D@test3@@$4PPPPPPPM@A@AEPAUA@2@XZ"
// CIR:   %[[VTORDISP_OFFS:[0-9]+]] = cir.const #cir.int<-4> : !s32i
// CIR:   %[[VTORDISP_PTR:[0-9]+]] = cir.ptr_stride %{{[0-9]+}}, %[[VTORDISP_OFFS]]
// CIR:   %[[DISP_I32_PTR:[0-9]+]] = cir.cast bitcast %[[VTORDISP_PTR]]
// CIR:   %[[DISP:[0-9]+]] = cir.load {{.*}} %[[DISP_I32_PTR]]
// CIR:   %[[NEG_DISP:[0-9]+]] = cir.minus %[[DISP]] : !s32i
// CIR:   %[[ADJ_THIS:[0-9]+]] = cir.ptr_stride %{{[0-9]+}}, %[[NEG_DISP]]
// CIR:   %[[CALL_RES:[0-9]+]] = cir.call @"?fn@D@test3@@{{UAEPAU12|UEAAPEAU12}}@XZ"(%{{[0-9]+}})
// CIR:   %[[IS_NOT_NULL:[0-9]+]] = cir.cmp ne %[[CALL_RES]]
// CIR:   %{{[0-9]+}} = cir.ternary(%[[IS_NOT_NULL]], true {
// CIR:     %[[VBINDEX:[0-9]+]] = cir.const #cir.int<1> : !s32i
// CIR:     %[[VBASE_OFFS_PTR:[0-9]+]] = cir.ptr_stride %{{[0-9]+}}, %[[VBINDEX]]
// CIR:     %[[VBASE_OFFS:[0-9]+]] = cir.load {{.*}} %[[VBASE_OFFS_PTR]]
// CIR:     cir.ptr_stride %{{.*}}, %[[VBASE_OFFS]]
// CIR:     cir.yield
// CIR:   }, false {
// CIR:     cir.yield
// CIR:   })
// CIR:   cir.return %{{[0-9]+}}

// vtordisp this-adjustment combined with virtual base return adjustment (vbase #2).
// CIR-X64-LABEL: cir.func no_inline comdat weak_odr dso_local @"?fn@D@test3@@$4PPPPPPPM@A@EAAPEAUB@2@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat weak_odr dso_local @"?fn@D@test3@@$4PPPPPPPM@A@AEPAUB@2@XZ"
// CIR:   %[[VTORDISP_OFFS:[0-9]+]] = cir.const #cir.int<-4> : !s32i
// CIR:   %[[VTORDISP_PTR:[0-9]+]] = cir.ptr_stride %{{[0-9]+}}, %[[VTORDISP_OFFS]]
// CIR:   %[[DISP_I32_PTR:[0-9]+]] = cir.cast bitcast %[[VTORDISP_PTR]]
// CIR:   %[[DISP:[0-9]+]] = cir.load {{.*}} %[[DISP_I32_PTR]]
// CIR:   %[[NEG_DISP:[0-9]+]] = cir.minus %[[DISP]] : !s32i
// CIR:   %[[ADJ_THIS:[0-9]+]] = cir.ptr_stride %{{[0-9]+}}, %[[NEG_DISP]]
// CIR:   %[[CALL_RES:[0-9]+]] = cir.call @"?fn@D@test3@@{{UAEPAU12|UEAAPEAU12}}@XZ"(%{{[0-9]+}})
// CIR:   %[[IS_NOT_NULL:[0-9]+]] = cir.cmp ne %[[CALL_RES]]
// CIR:   %{{[0-9]+}} = cir.ternary(%[[IS_NOT_NULL]], true {
// CIR:     %[[VBINDEX:[0-9]+]] = cir.const #cir.int<2> : !s32i
// CIR:     %[[VBASE_OFFS_PTR:[0-9]+]] = cir.ptr_stride %{{[0-9]+}}, %[[VBINDEX]]
// CIR:     %[[VBASE_OFFS:[0-9]+]] = cir.load {{.*}} %[[VBASE_OFFS_PTR]]
// CIR:     cir.ptr_stride %{{.*}}, %[[VBASE_OFFS]]
// CIR:     cir.yield
// CIR:   }, false {
// CIR:     cir.yield
// CIR:   })
// CIR:   cir.return %{{[0-9]+}}

// vtordisp this-adjustment with 0-byte return adjustment.
// CIR-X64-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?fn@D@test3@@$4PPPPPPPM@A@EAAPEAU12@XZ"
// CIR-X86-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?fn@D@test3@@$4PPPPPPPM@A@AEPAU12@XZ"
// CIR:   %[[VTORDISP_OFFS:[0-9]+]] = cir.const #cir.int<-4> : !s32i
// CIR:   %[[VTORDISP_PTR:[0-9]+]] = cir.ptr_stride %{{[0-9]+}}, %[[VTORDISP_OFFS]]
// CIR:   %[[DISP_I32_PTR:[0-9]+]] = cir.cast bitcast %[[VTORDISP_PTR]]
// CIR:   %[[DISP:[0-9]+]] = cir.load {{.*}} %[[DISP_I32_PTR]]
// CIR:   %[[NEG_DISP:[0-9]+]] = cir.minus %[[DISP]] : !s32i
// CIR:   %[[ADJ_THIS:[0-9]+]] = cir.ptr_stride %{{[0-9]+}}, %[[NEG_DISP]]
// CIR:   cir.call @"?fn@D@test3@@{{UAEPAU12|UEAAPEAU12}}@XZ"(%{{[0-9]+}})
// CIR-NOT: cir.ternary
// CIR:   cir.return %{{[0-9]+}}

} // namespace test3

namespace pr34302 {
struct A { virtual ~A(); };
struct B : A { virtual B *f(); };
struct C : virtual B { C *f(); };
C c;
} // namespace pr34302
