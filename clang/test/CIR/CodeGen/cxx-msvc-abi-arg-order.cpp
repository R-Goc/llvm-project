// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR

int get_a();
int get_b();
int get_c();

void free_fn(int a, int b, int c);

// CIR-LABEL: cir.func no_inline dso_local @"?test_free_call@@YAXXZ"()
void test_free_call() {
  // MSVC ABI evaluates function arguments right-to-left.
  // CIR: %[[C:[0-9]+]] = cir.call @"?get_c@@YAHXZ"()
  // CIR: %[[B:[0-9]+]] = cir.call @"?get_b@@YAHXZ"()
  // CIR: %[[A:[0-9]+]] = cir.call @"?get_a@@YAHXZ"()
  // CIR: cir.call @"?free_fn@@YAXHHH@Z"(%[[A]], %[[B]], %[[C]])
  free_fn(get_a(), get_b(), get_c());
}

struct MemberTest {
  void method(int a, int b, int c);
};

// CIR-LABEL: cir.func no_inline dso_local @"?test_member_call@@YAXAEAUMemberTest@@@Z"
void test_member_call(MemberTest &obj) {
  // Member call arguments are also evaluated right-to-left.
  // CIR: %[[C:[0-9]+]] = cir.call @"?get_c@@YAHXZ"()
  // CIR: %[[B:[0-9]+]] = cir.call @"?get_b@@YAHXZ"()
  // CIR: %[[A:[0-9]+]] = cir.call @"?get_a@@YAHXZ"()
  // CIR: cir.call @"?method@MemberTest@@QEAAXHHH@Z"(%{{[0-9]+}}, %[[A]], %[[B]], %[[C]])
  obj.method(get_a(), get_b(), get_c());
}

struct CtorTest {
  CtorTest(int a, int b, int c);
};

// CIR-LABEL: cir.func no_inline dso_local @"?test_ctor_direct@@YAXXZ"()
void test_ctor_direct() {
  // Direct initialization evaluates arguments right-to-left.
  // CIR: %[[C:[0-9]+]] = cir.call @"?get_c@@YAHXZ"()
  // CIR: %[[B:[0-9]+]] = cir.call @"?get_b@@YAHXZ"()
  // CIR: %[[A:[0-9]+]] = cir.call @"?get_a@@YAHXZ"()
  // CIR: cir.call @"??0CtorTest@@QEAA@HHH@Z"(%{{[0-9]+}}, %[[A]], %[[B]], %[[C]])
  CtorTest obj(get_a(), get_b(), get_c());
}

// CIR-LABEL: cir.func no_inline dso_local @"?test_ctor_list@@YAXXZ"()
void test_ctor_list() {
  // List initialization in C++ guarantees left-to-right evaluation order.
  // CIR: %[[A:[0-9]+]] = cir.call @"?get_a@@YAHXZ"()
  // CIR: %[[B:[0-9]+]] = cir.call @"?get_b@@YAHXZ"()
  // CIR: %[[C:[0-9]+]] = cir.call @"?get_c@@YAHXZ"()
  // CIR: cir.call @"??0CtorTest@@QEAA@HHH@Z"(%{{[0-9]+}}, %[[A]], %[[B]], %[[C]])
  CtorTest obj{get_a(), get_b(), get_c()};
}

struct AssignTest {
  AssignTest &operator=(const AssignTest &);
};
AssignTest get_lhs();
AssignTest get_rhs();

// CIR-LABEL: cir.func no_inline dso_local @"?test_assignment@@YAXXZ"()
void test_assignment() {
  // Assignment operator requires RHS evaluated before LHS.
  // CIR: cir.call @"?get_rhs@@YA?AUAssignTest@@XZ"()
  // CIR: cir.call @"?get_lhs@@YA?AUAssignTest@@XZ"()
  // CIR: cir.call @"??4AssignTest@@QEAAAEAU0@AEBU0@@Z"
  get_lhs() = get_rhs();
}

struct StreamTest {
  StreamTest &operator<<(int);
};

// CIR-LABEL: cir.func no_inline dso_local @"?test_shift@@YAXAEAUStreamTest@@@Z"
void test_shift(StreamTest &stream) {
  // Shift operators require left-to-right evaluation.
  // CIR: %[[STREAM:[0-9]+]] = cir.load %0
  // CIR: %[[A:[0-9]+]] = cir.call @"?get_a@@YAHXZ"()
  // CIR: cir.call @"??6StreamTest@@QEAAAEAU0@H@Z"(%[[STREAM]], %[[A]])
  stream << get_a();
}

void vararg_fn(int x, ...);

// CIR-LABEL: cir.func no_inline dso_local @"?test_vararg@@YAXXZ"()
void test_vararg() {
  // On Win64, MSVC implicitly widens null pointer constant to pointer-sized int.
  // CIR: %[[X:[0-9]+]] = cir.const #cir.int<42> : !s32i
  // CIR: %[[ZERO64:[0-9]+]] = cir.const #cir.int<0> : !s64i
  // CIR: cir.call @"?vararg_fn@@YAXHZZ"(%[[X]], %[[ZERO64]]) : (!s32i {{.*}}, !s64i {{.*}}) -> ()
  vararg_fn(42, 0);
}
