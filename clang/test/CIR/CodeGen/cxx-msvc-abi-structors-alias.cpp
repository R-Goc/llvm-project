// RUN: %clang_cc1 -fclangir -emit-cir %s -o - -triple=i386-pc-win32 -fno-rtti -mconstructor-aliases -O1 | FileCheck %s

//===----------------------------------------------------------------------===//
// Module-level Globals (emitted at the top of MLIR module)
//===----------------------------------------------------------------------===//

// CHECK-DAG: cir.global "private" constant weak_odr comdat dso_local @"??_7?$B@H@test3@@6B@" = #cir.vtable<

namespace test1 {
template <typename T> class A {
  ~A() {}
};
template class A<char>;
// CHECK-DAG: cir.func {{.*}}comdat {{.*}}weak_odr dso_local @"??1?$A@D@test1@@AAE@XZ"
}

namespace test2 {
struct A {
  virtual ~A();
};
struct B : A {
  B();
  virtual ~B();
};

A::~A() {}
B::~B() {}
void foo() {
  B b;
}
// CHECK-DAG: cir.func {{.*}}dso_local @"??1A@test2@@UAE@XZ"
// CHECK-DAG: cir.func private dso_local @"??1B@test2@@UAE@XZ"(!cir.ptr<!rec_test23A3AB>) alias(@"??1A@test2@@UAE@XZ")
}

namespace test3 {
struct A { virtual ~A(); };
A::~A() {}
}
// CHECK-DAG: cir.func {{.*}}dso_local @"??1A@test3@@UAE@XZ"(
namespace test3 {
template <typename T>
struct B : A {
  virtual ~B() { }
};
template struct B<int>;
}
// This has to be weak, and emitting weak aliases is fragile, so we don't do the
// aliasing.
// CHECK-DAG: cir.func {{.*}}comdat {{.*}}weak_odr dso_local @"??1?$B@H@test3@@UAE@XZ"(

namespace test4 {
struct A {
  virtual ~A() {}
};
struct B : A {
  virtual ~B() {}
};
struct C : B {
  virtual ~C() {}
};

void bar() {
  C c;
}
}
// CHECK-LABEL: cir.func dso_local @"?bar@test4@@YAXXZ"()
// CHECK:         %[[ALLOCA:.*]] = cir.alloca "c"
// CHECK:         %[[CAST:.*]] = cir.cast bitcast %[[ALLOCA]] : !cir.ptr<!rec_test43A3AC> -> !cir.ptr<!rec_test43A3AA>
// CHECK:         cir.call @"??1A@test4@@UAE@XZ"(%[[CAST]])
// CHECK:       cir.func {{.*}} @"??1A@test4@@UAE@XZ"(
