// RUN: %clang_cc1 -triple x86_64-windows-msvc %s -fclangir -emit-cir -o - | FileCheck %s


// PR41065: An abstract class can never be used to construct a complete object.
// In the Microsoft ABI, if the class is abstract, don't bother initializing
// its virtual bases. The "is this class the most derived class" check in the
// constructor will never pass, and the virtual base constructor calls are always
// dead. Skip them.

struct A {
  A();
  virtual void f() = 0;
  virtual ~A();
};

// B has an implicit inline dtor, but is still abstract.
struct B : A {
  B(int n);
  int n;
};

// Still abstract
struct C : virtual B {
  C(int n);
};

// Not abstract, D::D calls C::C and B::B.
struct D : C {
  D(int n);
  void f() override;
};

void may_throw();
C::C(int n) : B(n) { may_throw(); }
D::D(int n) : C(n), B(n) { may_throw(); }

// CHECK-DAG: cir.global constant linkonce_odr comdat dso_local @"??_8D@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<16> : !s32i]> : !cir.array<!s32i x 2>
// CHECK-DAG: cir.global constant linkonce_odr comdat dso_local @"??_8C@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<8> : !s32i]> : !cir.array<!s32i x 2>

// CHECK-LABEL: cir.func no_inline {{.*}}dso_local @"??0C@@QEAA@H@Z"(%arg0: !cir.ptr<!rec_C>{{.*}}, %arg1: !s32i{{.*}}, %arg2: !s32i{{.*}})
// CHECK-NOT: cir.if
// CHECK-NOT: cir.call @"??0B@@QEAA@H@Z"
// CHECK: cir.call @"?may_throw@@YAXXZ"()
// CHECK: cir.return

// CHECK: cir.func private {{.*}}@"?may_throw@@YAXXZ"() attributes

// CHECK-LABEL: cir.func no_inline {{.*}}dso_local @"??0D@@QEAA@H@Z"(%arg0: !cir.ptr<!rec_D>{{.*}}, %arg1: !s32i{{.*}}, %arg2: !s32i{{.*}})
// CHECK: %[[IS_MD:.*]] = cir.load {{.*}}%[[IS_MD_ADDR:.*]] : !cir.ptr<!s32i>, !s32i
// CHECK: %[[COND:.*]] = cir.cmp ne %[[IS_MD]], %{{.*}} : !s32i
// CHECK: cir.if %[[COND]] {
// CHECK:   cir.call @"??0B@@QEAA@H@Z"
// CHECK: }
// CHECK: %[[ZERO:.*]] = cir.const #cir.int<0> : !s32i
// CHECK: cir.call @"??0C@@QEAA@H@Z"(%{{.*}}, %{{.*}}, %[[ZERO]])
// CHECK: cir.call @"?may_throw@@YAXXZ"()
// CHECK: cir.return

// CHECK: cir.func private {{.*}}@"??0B@@QEAA@H@Z"(!cir.ptr<!rec_B>{{.*}}, !s32i{{.*}})
