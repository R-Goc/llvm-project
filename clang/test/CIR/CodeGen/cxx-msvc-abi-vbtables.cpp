// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s

namespace Test1 {
struct A {
  int a;
};

struct B : virtual A {
  int b;
  B();
};

B::B() {}

void foo() {
  B b;
  b.a = 42;
}
}

namespace Test2 {
struct A {
  int a;
};

struct B : virtual A {
  int b;
};

struct C : virtual A {
  int c;
};

struct D : B, C {
  int d;
  D();
};

D::D() {}

void bar() {
  D d;
  d.a = 100;
}
}

namespace Test3 {
struct A { int a; };
struct B { int b; };
struct C : virtual A, virtual B {
  int c;
  C();
};
C::C() {}
}

// Check VBTables
// CIR-DAG: cir.global constant linkonce_odr comdat dso_local @"??_8B@Test1@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<16> : !s32i]> : !cir.array<!s32i x 2>
// CIR-DAG: cir.global constant linkonce_odr comdat dso_local @"??_8D@Test2@@7BB@1@@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<40> : !s32i]> : !cir.array<!s32i x 2>
// CIR-DAG: cir.global constant linkonce_odr comdat dso_local @"??_8D@Test2@@7BC@1@@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<24> : !s32i]> : !cir.array<!s32i x 2>
// CIR-DAG: cir.global constant linkonce_odr comdat dso_local @"??_8C@Test3@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<16> : !s32i, #cir.int<20> : !s32i]> : !cir.array<!s32i x 3>

// CIR-LABEL: cir.func {{.*}} @"??0B@Test1@@QEAA@XZ"(%arg0: !cir.ptr<{{.*}}> {{.*}}, %arg1: !s32i {{.*}}) -> (!cir.ptr<{{.*}}> {{.*}})
// CIR:   %[[IS_MOST_DERIVED:.*]] = cir.load {{.*}} : !cir.ptr<!s32i>, !s32i
// CIR:   %[[ZERO:.*]] = cir.const #cir.int<0> : !s32i
// CIR:   %[[COND:.*]] = cir.cmp ne %[[IS_MOST_DERIVED]], %[[ZERO]] : !s32i
// CIR:   cir.if %[[COND]] {
// CIR:     %[[VB_GLOBAL:.*]] = cir.get_global @"??_8B@Test1@@7B@" : !cir.ptr<!cir.array<!s32i x 2>>
// CIR:     %[[VB_DECAY:.*]] = cir.cast array_to_ptrdecay %[[VB_GLOBAL]] : !cir.ptr<!cir.array<!s32i x 2>> -> !cir.ptr<!s32i>
// CIR:     cir.store {{.*}} %[[VB_DECAY]], {{.*}} : !cir.ptr<!s32i>, !cir.ptr<!cir.ptr<!s32i>>
// CIR:   }

// CIR-LABEL: cir.func {{.*}} @"?foo@Test1@@YAXXZ"()
// CIR:   %[[B:.*]] = cir.alloca "b" {{.*}}
// CIR:   %[[ONE:.*]] = cir.const #cir.int<1> : !s32i
// CIR:   cir.call @"??0B@Test1@@QEAA@XZ"(%[[B]], %[[ONE]])
// CIR:   %[[FORTY_TWO:.*]] = cir.const #cir.int<42> : !s32i
// CIR:   cir.load {{.*}} : !cir.ptr<!cir.ptr<!u8i>>, !cir.ptr<!u8i>
// CIR:   cir.ptr_stride
// CIR:   cir.load {{.*}} : !cir.ptr<!s32i>, !s32i
// CIR:   cir.store {{.*}} %[[FORTY_TWO]], {{.*}} : !s32i, !cir.ptr<!s32i>

// CIR-LABEL: cir.func {{.*}} @"??0D@Test2@@QEAA@XZ"
// CIR:   cir.if {{.*}} {
// CIR:     %[[VBT_B:.*]] = cir.get_global @"??_8D@Test2@@7BB@1@@" : !cir.ptr<!cir.array<!s32i x 2>>
// CIR:     %[[VBT_B_DECAY:.*]] = cir.cast array_to_ptrdecay %[[VBT_B]] : !cir.ptr<!cir.array<!s32i x 2>> -> !cir.ptr<!s32i>
// CIR:     cir.store {{.*}} %[[VBT_B_DECAY]], {{.*}}
// CIR:     %[[VBT_C:.*]] = cir.get_global @"??_8D@Test2@@7BC@1@@" : !cir.ptr<!cir.array<!s32i x 2>>
// CIR:     %[[VBT_C_DECAY:.*]] = cir.cast array_to_ptrdecay %[[VBT_C]] : !cir.ptr<!cir.array<!s32i x 2>> -> !cir.ptr<!s32i>
// CIR:     cir.store {{.*}} %[[VBT_C_DECAY]], {{.*}}
// CIR:   }
// CIR:   cir.call @"??0B@Test2@@QEAA@XZ"({{.*}})
// CIR:   cir.call @"??0C@Test2@@QEAA@XZ"({{.*}})
