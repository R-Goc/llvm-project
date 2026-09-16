// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fclangir -emit-cir %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir %s --check-prefix=X64
// RUN: %clang_cc1 -std=c++17 -triple i386-pc-windows-msvc -fclangir -emit-cir %s -o %t32.cir
// RUN: FileCheck --input-file=%t32.cir %s --check-prefix=X86

// Global variable DAG checks must come before function-level checks.
// X64-DAG: cir.global "private" internal dso_local @h = #cir.int<0> : !u16i {alignment = 4 : i64}
// X86-DAG: cir.global "private" internal dso_local @h = #cir.int<0> : !u16i {alignment = 2 : i64}

class F {
public:
  F(wchar_t *);
};
using a = F;
struct A {};
struct b {
  b(a, F, A);
};
template <typename, typename> struct c : b {
  c(const a &p1, const A &d) : b(p1, 0, d) {}
};
template <typename e> struct B : c<e, b> {
  using c<e, b>::c;
};
class f {
public:
  f(...);
}

typedef g;
class C {
public:
  C(g, f);
};
static wchar_t h;
class D {
public:
  static C E();
};

C D::E() {
  C i(B<bool>(&h, {}), f());
  return i;
}

// Inheriting constructor has internal linkage without comdat and forwards arguments.
// X64-LABEL: cir.func no_inline internal private dso_local @"??0?$B@_N@@QEAA@AEBVF@@AEBUA@@@Z"
// X64-NOT: comdat
// X64:         %[[THIS:.+]] = cir.load %{{.+}} : !cir.ptr<!cir.ptr<!rec_B3Cbool3E>>, !cir.ptr<!rec_B3Cbool3E>
// X64:         %[[BASE:.+]] = cir.base_class_addr %[[THIS]] : !cir.ptr<!rec_B3Cbool3E> nonnull [0] -> !cir.ptr<!rec_c3Cbool2C_b3E>
// X64:         cir.call @"??0?$c@_NUb@@@@QEAA@AEBVF@@AEBUA@@@Z"(%[[BASE]], %{{.+}}, %{{.+}})
// X64:         cir.return

// X86-LABEL: cir.func no_inline internal private dso_local @"??0?$B@_N@@QAE@ABVF@@ABUA@@@Z"
// X86-NOT: comdat
// X86:         %[[THIS:.+]] = cir.load %{{.+}} : !cir.ptr<!cir.ptr<!rec_B3Cbool3E>>, !cir.ptr<!rec_B3Cbool3E>
// X86:         %[[BASE:.+]] = cir.base_class_addr %[[THIS]] : !cir.ptr<!rec_B3Cbool3E> nonnull [0] -> !cir.ptr<!rec_c3Cbool2C_b3E>
// X86:         cir.call @"??0?$c@_NUb@@@@QAE@ABVF@@ABUA@@@Z"(%[[BASE]], %{{.+}}, %{{.+}}) cc(x86_thiscall)
// X86:         cir.return

// Test inheriting constructor with callee-cleanup parameter
struct NonTrivialParam {
  int val;
  ~NonTrivialParam();
};

struct BaseWithCleanup {
  BaseWithCleanup(NonTrivialParam);
};

struct DerivedWithCleanup : BaseWithCleanup {
  using BaseWithCleanup::BaseWithCleanup;
};

void test_callee_cleanup() {
  DerivedWithCleanup d(NonTrivialParam{1});
}

// Callee cleanup inheriting constructor inlined into caller.
// Verifies return value slot retval.inhctor allocation and base constructor call.
// X64-LABEL: cir.func no_inline dso_local @"?test_callee_cleanup@@YAXXZ"
// X64:         %[[RETVAL_SLOT:.+]] = cir.alloca "retval.inhctor"
// X64:         %[[THIS:.+]] = cir.load %{{.+}} : !cir.ptr<!cir.ptr<!rec_DerivedWithCleanup>>, !cir.ptr<!rec_DerivedWithCleanup>
// X64:         cir.store align(8) %[[THIS]], %[[RETVAL_SLOT]]
// X64:         %[[BASE:.+]] = cir.base_class_addr %[[THIS]] : !cir.ptr<!rec_DerivedWithCleanup> nonnull [0] -> !cir.ptr<!rec_BaseWithCleanup>
// X64:         cir.call @"??0BaseWithCleanup@@QEAA@UNonTrivialParam@@@Z"(%[[BASE]], %{{.+}})
// X64:         cir.return

// X86-LABEL: cir.func no_inline dso_local @"?test_callee_cleanup@@YAXXZ"
// X86:         %[[RETVAL_SLOT:.+]] = cir.alloca "retval.inhctor"
// X86:         %[[THIS:.+]] = cir.load %{{.+}} : !cir.ptr<!cir.ptr<!rec_DerivedWithCleanup>>, !cir.ptr<!rec_DerivedWithCleanup>
// X86:         cir.store align(4) %[[THIS]], %[[RETVAL_SLOT]]
// X86:         %[[BASE:.+]] = cir.base_class_addr %[[THIS]] : !cir.ptr<!rec_DerivedWithCleanup> nonnull [0] -> !cir.ptr<!rec_BaseWithCleanup>
// X86:         cir.call @"??0BaseWithCleanup@@QAE@UNonTrivialParam@@@Z"(%[[BASE]], %{{.+}}) cc(x86_thiscall)
// X86:         cir.return

// Non-inheriting constructor has linkonce_odr and comdat.
// X64-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"??0?$c@_NUb@@@@QEAA@AEBVF@@AEBUA@@@Z"
// X86-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"??0?$c@_NUb@@@@QAE@ABVF@@ABUA@@@Z"
