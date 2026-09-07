// RUN: %clang_cc1 -fno-rtti -triple=i386-pc-windows-msvc -std=c++17 -fclangir -emit-cir %s -o - | FileCheck %s

struct A {
  virtual void f();
};

struct B {
  virtual void f();
};

struct C : A, B {};

struct D : virtual C {
  D();
  ~D();
  virtual void f();
  void g();
  int xxx;
};

D::D() {}  // Forces vftable emission.

// Note that the vtordisp is applied before really adjusting to D*.
// CHECK-LABEL: cir.func {{.*}} @"?f@D@@$4PPPPPPPM@A@AEXXZ"
// CHECK:   cir.ptr_stride
// CHECK:   cir.load
// CHECK:   cir.minus
// CHECK:   cir.ptr_stride
// CHECK:   cir.call @"?f@D@@UAEXXZ"

// CHECK-LABEL: cir.func {{.*}} @"?f@D@@$4PPPPPPPI@3AEXXZ"
// CHECK:   cir.ptr_stride
// CHECK:   cir.load
// CHECK:   cir.minus
// CHECK:   cir.ptr_stride
// CHECK:   cir.ptr_stride
// CHECK:   cir.call @"?f@D@@UAEXXZ"

struct E : virtual A {
  virtual void f();
  ~E();
};

struct F {
  virtual void z();
};

struct G : virtual F, virtual E {
  int ggg;
  G();
  ~G();
};

G::G() {}  // Forces vftable emission.

// CHECK-LABEL: cir.func {{.*}} @"?f@E@@$R4BA@M@PPPPPPPM@7AEXXZ"
// CHECK:   cir.ptr_stride
// CHECK:   cir.load
// CHECK:   cir.minus
// CHECK:   cir.ptr_stride
// CHECK:   cir.ptr_stride
// CHECK:   cir.load
// CHECK:   cir.ptr_stride
// CHECK:   cir.load
// CHECK:   cir.ptr_stride
// CHECK:   cir.ptr_stride
// CHECK:   cir.call @"?f@E@@UAEXXZ"
