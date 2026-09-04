// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s

// Adapted from Clang CodeGenCXX/microsoft-abi-methods.cpp,
// microsoft-abi-structors.cpp, and microsoft-abi-vftables.cpp.

//===----------------------------------------------------------------------===//
// Section 1: Member Functions & Calling Conventions
//===----------------------------------------------------------------------===//

class MethodsTest {
public:
  void simple_method() {}
  void vararg_method(const char *fmt, ...) {}
  static void static_method() {}
};

void call_methods() {
  MethodsTest obj;
  obj.simple_method();
  obj.vararg_method("test");
  MethodsTest::static_method();
}

// CIR-DAG: cir.global {{.*}} linkonce_odr {{.*}} @"??_C@_
// CIR-DAG: cir.func {{.*}} @"?simple_method@MethodsTest@@QEAAXXZ"(%{{.*}}: !cir.ptr<!rec_MethodsTest>
// CIR-DAG: cir.func {{.*}} @"?vararg_method@MethodsTest@@QEAAXPEBDZZ"(%{{.*}}: !cir.ptr<!rec_MethodsTest>{{.*}}, %{{.*}}: !cir.ptr<!s8i>{{.*}}, ...)
// CIR-DAG: cir.func {{.*}} @"?static_method@MethodsTest@@SAXXZ"()

//===----------------------------------------------------------------------===//
// Section 2: Structors and 'this' Return Calling Convention
//===----------------------------------------------------------------------===//

class BasicA {
  int x;
public:
  BasicA() { x = 42; }
  ~BasicA();
};

BasicA::~BasicA() {}

// Destructors return void.
// CIR-LABEL: cir.func {{.*}} @"??1BasicA@@QEAA@XZ"

void test_basic() {
  BasicA a;
}

// In the MSVC ABI, constructors return 'this' (hasThisReturn is true).
// CIR-LABEL: cir.func {{.*}} @"??0BasicA@@QEAA@XZ"
// CIR-SAME: (%[[THIS_ARG:.*]]: !cir.ptr<!rec_BasicA>{{.*}}) -> (!cir.ptr<!rec_BasicA>
// CIR:   %[[THIS_ALLOCA:.*]] = cir.alloca "this"
// CIR:   %[[RETVAL:.*]] = cir.alloca "__retval"
// CIR:   cir.store %[[THIS_ARG]], %[[THIS_ALLOCA]]
// CIR:   %[[LOADED_THIS:.*]] = cir.load %[[THIS_ALLOCA]]
// CIR:   cir.store {{.*}}%[[LOADED_THIS]], %[[RETVAL]]
// CIR:   cir.const #cir.int<42>
// CIR:   %[[LOADED_RET:.*]] = cir.load %[[RETVAL]]
// CIR:   cir.return %[[LOADED_RET]] : !cir.ptr<!rec_BasicA>
// CIR: }

// Constructor & Destructor Inheritance chain:
class Base {
public:
  Base();
  ~Base();
};

class Derived : public Base {
public:
  Derived();
  ~Derived();
};

Derived::Derived() {}
Derived::~Derived() {}

// Derived ctor calls Base ctor and returns this:
// CIR-LABEL: cir.func {{.*}} @"??0Derived@@QEAA@XZ"
// CIR-SAME: (%[[THIS:.*]]: !cir.ptr<!rec_Derived>{{.*}}) -> (!cir.ptr<!rec_Derived>
// CIR:   cir.call @"??0Base@@QEAA@XZ"
// CIR:   cir.return %{{.*}} : !cir.ptr<!rec_Derived>

// Derived dtor calls Base dtor:
// CIR-LABEL: cir.func {{.*}} @"??1Derived@@QEAA@XZ"
// CIR-SAME: (%[[THIS:.*]]: !cir.ptr<!rec_Derived>
// CIR:   cir.call @"??1Base@@QEAA@XZ"

//===----------------------------------------------------------------------===//
// Section 3: Virtual Functions and Vector/Scalar Deleting Destructors
//===----------------------------------------------------------------------===//

class Shape {
public:
  virtual ~Shape();
  virtual void draw();
};

void call_virtual(Shape *s) {
  s->draw();
}

// Virtual call uses vtable lookup:
// CIR-LABEL: cir.func {{.*}} @"?call_virtual@@YAXPEAVShape@@@Z"
// CIR-SAME: (%[[ARG:.*]]: !cir.ptr<!rec_Shape>{{.*}})
// CIR:   %[[VPTR:.*]] = cir.vtable.get_vptr
// CIR:   %[[VTABLE_PTR:.*]] = cir.load align(8) %[[VPTR]]
// CIR:   %[[VFN_SLOT:.*]] = cir.vtable.get_virtual_fn_addr %[[VTABLE_PTR]][1]
// CIR:   %[[VFN:.*]] = cir.load align(8) %[[VFN_SLOT]]
// CIR:   cir.call %[[VFN]](%{{.*}})

void delete_virtual(Shape *s) {
  delete s;
}

// Virtual destructor call triggers vector/scalar deleting destructor (?_E or ?_G)
// with implicit should_call_delete flag passed as an argument:
// CIR-LABEL: cir.func {{.*}} @"?delete_virtual@@YAXPEAVShape@@@Z"
// CIR-SAME: (%[[ARG:.*]]: !cir.ptr<!rec_Shape>{{.*}})
// CIR:   %[[FLAG:.*]] = cir.const #cir.int<1> : !s32i
// CIR:   %[[VPTR:.*]] = cir.vtable.get_vptr
// CIR:   %[[VTABLE_PTR:.*]] = cir.load align(8) %[[VPTR]]
// CIR:   %[[VFN_SLOT:.*]] = cir.vtable.get_virtual_fn_addr %[[VTABLE_PTR]][0]
// CIR:   %[[VFN:.*]] = cir.load align(8) %[[VFN_SLOT]]
// CIR:   cir.call %[[VFN]](%{{.*}}, %[[FLAG]])

// When a class with a virtual destructor is defined, its scalar deleting
// destructor thunk (??_G) and vector deleting destructor alias (??_E) are emitted.
class HasVirtualDtor {
public:
  HasVirtualDtor();
  virtual ~HasVirtualDtor();
};

HasVirtualDtor::HasVirtualDtor() {}
HasVirtualDtor::~HasVirtualDtor() {}

// When a class with a virtual destructor is used with delete[], its vector
// deleting destructor (??_E) is emitted as a real function instead of an alias,
// and the delete[] expression reads the array cookie.
class HasVectorDtor {
public:
  HasVectorDtor();
  virtual ~HasVectorDtor();
};

HasVectorDtor::HasVectorDtor() {}
HasVectorDtor::~HasVectorDtor() {}

void delete_array_virtual(HasVectorDtor *p) {
  delete[] p;
}

// CIR-LABEL: cir.func {{.*}} @"?delete_array_virtual@@YAXPEAVHasVectorDtor@@@Z"(
// CIR:   %[[COOKIE_OFFSET:.*]] = cir.const #cir.int<-8> : !s32i
// CIR:   %[[COOKIE_PTR:.*]] = cir.ptr_stride %{{.*}}, %[[COOKIE_OFFSET]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>
// CIR:   %[[COOKIE_SIZE_PTR:.*]] = cir.cast bitcast %[[COOKIE_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!u64i>
// CIR:   %[[NUM_ELTS:.*]] = cir.load align(8) %[[COOKIE_SIZE_PTR]]
// CIR:   %[[ZERO:.*]] = cir.const #cir.int<0> : !u64i
// CIR:   %[[IS_EMPTY:.*]] = cir.cmp eq %[[NUM_ELTS]], %[[ZERO]] : !u64i
// CIR:   cir.if %[[IS_EMPTY]] {
// CIR:     cir.call @"??_V@YAXPEAX_K@Z"(%{{.*}})
// CIR:   } else {
// CIR:     %[[FLAGS:.*]] = cir.const #cir.int<3> : !s32i
// CIR:     %[[VPTR:.*]] = cir.vtable.get_vptr
// CIR:     %[[VTABLE:.*]] = cir.load align(8) %[[VPTR]]
// CIR:     %[[VFN_SLOT:.*]] = cir.vtable.get_virtual_fn_addr %[[VTABLE]][0]
// CIR:     %[[VFN:.*]] = cir.load align(8) %[[VFN_SLOT]]
// CIR:     cir.call %[[VFN]](%{{.*}}, %[[FLAGS]])
// CIR:   }

// Vector deleting destructor definition with array destroy and conditional delete:
// CIR-LABEL: cir.func {{.*}} @"??_EHasVectorDtor@@UEAAPEAXI@Z"(
// CIR:   %[[TWO:.*]] = cir.const #cir.int<2> : !s32i
// CIR:   %[[AND_VEC:.*]] = cir.and %{{.*}}, %[[TWO]] : !s32i
// CIR:   %[[IS_VEC:.*]] = cir.cmp ne %[[AND_VEC]], %{{.*}} : !s32i
// CIR:   cir.if %[[IS_VEC]] {
// CIR:     %[[COOKIE_OFFSET:.*]] = cir.const #cir.int<-8> : !s32i
// CIR:     %[[ALLOC_PTR:.*]] = cir.ptr_stride %{{.*}}, %[[COOKIE_OFFSET]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>
// CIR:     %[[NUM_ELTS_PTR:.*]] = cir.cast bitcast %[[ALLOC_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!u64i>
// CIR:     %[[NUM_ELTS:.*]] = cir.load align(8) %[[NUM_ELTS_PTR]]
// CIR:     cir.do {
// CIR:       cir.call @"??1HasVectorDtor@@UEAA@XZ"(
// CIR:       cir.yield
// CIR:     }
// CIR:     %[[ONE:.*]] = cir.const #cir.int<1> : !s32i
// CIR:     %[[AND_DEL:.*]] = cir.and %{{.*}}, %[[ONE]] : !s32i
// CIR:     %[[SHOULD_DEL:.*]] = cir.cmp ne %[[AND_DEL]], %{{.*}} : !s32i
// CIR:     cir.if %[[SHOULD_DEL]] {
// CIR:       cir.call @"??_V@YAXPEAX_K@Z"(%{{.*}})
// CIR:     }
// CIR:   } else {
// CIR:     cir.call @"??1HasVectorDtor@@UEAA@XZ"(
// CIR:     %[[ONE:.*]] = cir.const #cir.int<1> : !s32i
// CIR:     %[[AND_DEL:.*]] = cir.and %{{.*}}, %[[ONE]] : !s32i
// CIR:     %[[SHOULD_DEL:.*]] = cir.cmp ne %[[AND_DEL]], %{{.*}} : !s32i
// CIR:     cir.if %[[SHOULD_DEL]] {
// CIR:       cir.call @{{.*}}3@{{.*}}(
// CIR:     }
// CIR:   }
// CIR:   cir.return %{{.*}} : !cir.ptr<!void>

// Scalar deleting destructor definition:
// CIR-LABEL: cir.func {{.*}} @"??_GHasVirtualDtor@@UEAAPEAXI@Z"(
// CIR-SAME: %[[THIS_ARG:.*]]: !cir.ptr<!rec_HasVirtualDtor>{{.*}}, %[[FLAG_ARG:.*]]: !s32i{{.*}}) -> (!cir.ptr<!void>
// CIR:   %[[RETVAL:.*]] = cir.alloca "__retval"
// CIR:   cir.store {{.*}}%[[RETVAL]]
// CIR:   cir.call @"??1HasVirtualDtor@@UEAA@XZ"(%{{.*}})
// CIR:   %[[ONE:.*]] = cir.const #cir.int<1> : !s32i
// CIR:   %[[AND:.*]] = cir.and %{{.*}}, %[[ONE]] : !s32i
// CIR:   %[[ZERO:.*]] = cir.const #cir.int<0> : !s32i
// CIR:   %[[COND:.*]] = cir.cmp ne %[[AND]], %[[ZERO]] : !s32i
// CIR:   cir.if %[[COND]] {
// CIR:     cir.call @{{.*}}3@{{.*}}(
// CIR:   }
// CIR:   %[[RES:.*]] = cir.load %[[RETVAL]]
// CIR:   cir.return %[[RES]] : !cir.ptr<!void>

// CIR: cir.func {{.*}} @"??_EHasVirtualDtor@@UEAAPEAXI@Z"(!cir.ptr<!rec_HasVirtualDtor>, !s32i) -> !cir.ptr<!void> alias(@"??_GHasVirtualDtor@@UEAAPEAXI@Z")
