// RUN: %clang_cc1 -triple x86_64-apple-darwin10 -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir --check-prefix=CIR %s

// Check packed record layouts with ms_struct attribute and #pragma ms_struct in ClangIR.

#pragma GCC diagnostic ignored "-Wincompatible-ms-struct"
#define ATTR __attribute__((__ms_struct__))

struct ATTR VBase {
  virtual void foo() = 0;
};

struct ATTR Base : virtual VBase {
  virtual void bar() = 0;
};

struct ATTR Derived : Base {
  Derived();
  void foo();
  void bar();
  int value;
};

// CIR-DAG: !rec_VBase = !cir.struct<"VBase" {data !cir.vptr}>
// CIR-DAG: !rec_Base = !cir.struct<"Base" {data !rec_VBase}>
// CIR-DAG: !rec_Derived = !cir.struct<"Derived" packed {data !rec_Base, data !s32i, pad !cir.array<!u8i x 4>}>

// CIR-LABEL: cir.func no_inline alignment(2) @_ZN7DerivedC2Ev(
// CIR: %[[THIS_ADDR:.*]] = cir.alloca "this"
// CIR: %[[THIS:.*]] = cir.load %[[THIS_ADDR]]
// CIR: cir.call @_ZN4BaseC2Ev
// CIR: %[[VALUE_PTR:.*]] = cir.get_member %[[THIS]][1] {name = "value"} : !cir.ptr<!rec_Derived> -> !cir.ptr<!s32i>
// CIR: %[[VAL:.*]] = cir.const #cir.int<20> : !s32i
// CIR: cir.store align(8) %[[VAL]], %[[VALUE_PTR]] : !s32i, !cir.ptr<!s32i>
Derived::Derived() : value(20) {}
