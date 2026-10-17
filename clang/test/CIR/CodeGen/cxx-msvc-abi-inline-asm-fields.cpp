// RUN: %clang_cc1 %s -triple i386-apple-darwin10 -fasm-blocks -fclangir -emit-cir -o %t.cir
// RUN: FileCheck --input-file=%t.cir --check-prefix=CIR %s

// Check that MS-style inline asm field access emits Intel flavor assembly,
// lowers member expressions via cir.get_member, and correctly rewrites
// input constraint operand references ($1) when a return register output
// is added.

struct A {
  int a1;
  int a2;
  struct B {
    int b1;
    int b2;
  } a3;
};

namespace asdf {
A a_global;
}

// CIR-DAG: cir.global external @_ZN4asdf8a_globalE = #cir.zero : !rec_A align(4)

extern "C" int test_param_field(A p) {
// CIR-LABEL: cir.func no_inline @test_param_field(
// CIR: %[[RETVAL:.*]] = cir.alloca "__retval"
// CIR: %[[P:.*]] = cir.alloca "p"
// CIR: %[[RET_CAST:.*]] = cir.cast bitcast %[[RETVAL]]
// CIR: %[[A1:.*]] = cir.get_member %[[P]][0] {name = "a1"}
// CIR: %[[RES:.*]] = cir.asm(x86_intel,
// CIR: in = [%[[A1]] : !cir.ptr<!s32i> (maybe_memory)],
// CIR: in_out = [],
// CIR: {"mov eax, $1" "=&{eax},*m,~{dirflag},~{fpsr},~{flags}"}) side_effects -> !u32i
// CIR: cir.store align(4) %[[RES]], %[[RET_CAST]]
// CIR: %[[LOAD:.*]] = cir.load %[[RETVAL]]
// CIR: cir.return %[[LOAD]] : !s32i
  __asm mov eax, p.a1
}

extern "C" int test_namespace_global() {
// CIR-LABEL: cir.func no_inline @test_namespace_global()
// CIR: %[[RETVAL:.*]] = cir.alloca "__retval"
// CIR: %[[RET_CAST:.*]] = cir.cast bitcast %[[RETVAL]]
// CIR: %[[GLOBAL:.*]] = cir.get_global @_ZN4asdf8a_globalE
// CIR: %[[A3:.*]] = cir.get_member %[[GLOBAL]][2] {name = "a3"}
// CIR: %[[B2:.*]] = cir.get_member %[[A3]][1] {name = "b2"}
// CIR: %[[RES:.*]] = cir.asm(x86_intel,
// CIR: in = [%[[B2]] : !cir.ptr<!s32i> (maybe_memory)],
// CIR: in_out = [],
// CIR: {"mov eax, $1" "=&{eax},*m,~{dirflag},~{fpsr},~{flags}"}) side_effects -> !u32i
// CIR: cir.store align(4) %[[RES]], %[[RET_CAST]]
// CIR: %[[LOAD:.*]] = cir.load %[[RETVAL]]
// CIR: cir.return %[[LOAD]] : !s32i
  __asm mov eax, asdf::a_global.a3.b2
}

template <bool Signed>
struct make_storage_type {
  struct type {
    struct B {
      int a;
      int x;
    } b;
  };
};

template <bool Signed>
struct msvc_dcas_x86 {
  typedef typename make_storage_type<Signed>::type storage_type;
  void store() __asm("PR26001") {
    storage_type p;
    __asm mov edx, p.b.x;
  }
};

template void msvc_dcas_x86<false>::store();
// CIR-LABEL: cir.func {{.*}} @"\01PR26001"(
// CIR: %[[P:.*]] = cir.alloca "p"
// CIR: %[[B:.*]] = cir.get_member %[[P]][0] {name = "b"}
// CIR: %[[X:.*]] = cir.get_member %[[B]][1] {name = "x"}
// CIR: cir.asm(x86_intel,
// CIR: in = [%[[X]] : !cir.ptr<!s32i> (maybe_memory)],
// CIR: in_out = [],
// CIR: {"mov edx, $0" "*m,~{edx},~{dirflag},~{fpsr},~{flags}"}) side_effects
// CIR: cir.return
