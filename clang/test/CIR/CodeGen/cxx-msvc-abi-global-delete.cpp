// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fms-extensions -fclangir -emit-cir -DTEST_NO_FORWARDING %s -o %t64-nofwd.cir
// RUN: FileCheck --input-file=%t64-nofwd.cir --check-prefixes=CIR-NOFWD,X64-NOFWD %s
// RUN: %clang_cc1 -std=c++17 -triple i386-pc-windows-msvc -fms-extensions -fclangir -emit-cir -DTEST_NO_FORWARDING %s -o %t32-nofwd.cir
// RUN: FileCheck --input-file=%t32-nofwd.cir --check-prefixes=CIR-NOFWD,X86-NOFWD %s
// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fms-extensions -fclangir -emit-cir -DTEST_FORWARDING %s -o %t64-fwd.cir
// RUN: FileCheck --input-file=%t64-fwd.cir --check-prefixes=CIR-FWD,X64-FWD %s
// RUN: %clang_cc1 -std=c++17 -triple i386-pc-windows-msvc -fms-extensions -fclangir -emit-cir -DTEST_FORWARDING %s -o %t32-fwd.cir
// RUN: FileCheck --input-file=%t32-fwd.cir --check-prefixes=CIR-FWD,X86-FWD %s

// Adapted from Clang CodeGenCXX tests:
// - msvc-global-delete-forwarding-at-delete-site.cpp
// - msvc-no-global-delete-forwarding.cpp
// - msvc-global-delete-scalar-array-split.cpp
// - msvc-global-delete-scope-no-dtor.cpp
// - msvc-global-delete-llvm-used.cpp

//===----------------------------------------------------------------------===//
// Module-level Globals and Declarations (emitted at the top of MLIR module)
//===----------------------------------------------------------------------===//

// X64-NOFWD-DAG: cir.global "private" appending @llvm.used = #cir.const_array<[#cir.global_view<@"?__empty_global_delete@@YAXPEAX_K@Z"> : !cir.ptr<!void>]>
// X86-NOFWD-DAG: cir.global "private" appending @llvm.used = #cir.const_array<[#cir.global_view<@"?__empty_global_delete@@YAXPAXI@Z"> : !cir.ptr<!void>]>

// X64-FWD-DAG: cir.global "private" appending @llvm.used = #cir.const_array<[#cir.global_view<@"?__empty_global_delete@@YAXPEAX_K@Z"> : !cir.ptr<!void>]>
// X86-FWD-DAG: cir.global "private" appending @llvm.used = #cir.const_array<[#cir.global_view<@"?__empty_global_delete@@YAXPAXI@Z"> : !cir.ptr<!void>]>

#ifdef TEST_NO_FORWARDING

// Plain `delete`/`delete[]` (no `::`) and `::delete` on primitive/trivial types
// do NOT trigger forwarding body emission. The wrapper remains a weak alias
// pointing to ?__empty_global_delete@@...

struct BaseNoFwd {
  void *operator new(__SIZE_TYPE__);
  void operator delete(void *);
  void operator delete[](void *);
  virtual ~BaseNoFwd();
};
struct DerivedNoFwd : BaseNoFwd {
  virtual ~DerivedNoFwd();
};
BaseNoFwd::~BaseNoFwd() {}
DerivedNoFwd::~DerivedNoFwd() {}

void test_plain_delete() {
  BaseNoFwd *p = new DerivedNoFwd[2];
  delete[] p;
}

// Explicit ::delete on primitive type: does not trigger forwarding body
void test_primitive_delete(int *p) {
  ::delete p;
}

// Explicit ::delete on trivial struct: does not trigger forwarding body
struct Trivial {
  int x;
};
void test_trivial_delete(Trivial *p) {
  ::delete p;
}

// Trapping empty fallback is emitted with cir.trap:
// X64-NOFWD-LABEL: cir.func comdat linkonce_odr private @"?__empty_global_delete@@YAXPEAX_K@Z"
// X64-NOFWD-NEXT:    cir.trap
// X86-NOFWD-LABEL: cir.func comdat linkonce_odr private @"?__empty_global_delete@@YAXPAXI@Z"
// X86-NOFWD-NEXT:    cir.trap

// Both wrappers default to weak aliases to ?__empty_global_delete:
// X64-NOFWD-DAG: cir.func weak private @"?__global_array_delete@@YAXPEAX_K@Z"(!cir.ptr<!void>, !u64i) alias(@"?__empty_global_delete@@YAXPEAX_K@Z")
// X64-NOFWD-DAG: cir.func weak private @"?__global_delete@@YAXPEAX_K@Z"(!cir.ptr<!void>, !u64i) alias(@"?__empty_global_delete@@YAXPEAX_K@Z")
// X86-NOFWD-DAG: cir.func weak private @"?__global_array_delete@@YAXPAXI@Z"(!cir.ptr<!void>, !u32i) alias(@"?__empty_global_delete@@YAXPAXI@Z")
// X86-NOFWD-DAG: cir.func weak private @"?__global_delete@@YAXPAXI@Z"(!cir.ptr<!void>, !u32i) alias(@"?__empty_global_delete@@YAXPAXI@Z")

// Neither wrapper emits a forwarding body:
// CIR-NOFWD-NOT: cir.func comdat linkonce_odr private @"?__global_delete@@
// CIR-NOFWD-NOT: cir.func comdat linkonce_odr private @"?__global_array_delete@@

#endif // TEST_NO_FORWARDING

#ifdef TEST_FORWARDING

// ::delete on a class with non-trivial destructor triggers emission of strong
// forwarding bodies. Distinct wrappers are used for scalar (?__global_delete)
// vs array (?__global_array_delete), sharing the same __empty_global_delete fallback.

using sz = decltype(sizeof(0));

struct Scalar {
  void *operator new(sz);
  void operator delete(void *, sz);
  virtual ~Scalar();
};
struct ScalarD : Scalar {
  virtual ~ScalarD();
};
Scalar::~Scalar() {}
ScalarD::~ScalarD() {}

struct Array {
  void *operator new[](sz);
  void operator delete[](void *, sz);
  virtual ~Array();
};
struct ArrayD : Array {
  virtual ~ArrayD();
};
Array::~Array() {}
ArrayD::~ArrayD() {}

void test_scalar_array_forwarding() {
  ::delete new ScalarD();
  ArrayD *a = new ArrayD[2];
  ::delete[] a;
}

// A `::delete` on a class whose destructor is NOT defined in this TU (only
// declared) must still emit a strong __global_delete forwarding body.
struct ExtClass {
  virtual ~ExtClass();
  void operator delete(void *, sz);
};
void test_delete_site_only(ExtClass *p) {
  ::delete p;
}

// Trapping empty fallback is emitted with cir.trap:
// X64-FWD-LABEL: cir.func comdat linkonce_odr private @"?__empty_global_delete@@YAXPEAX_K@Z"
// X64-FWD-NEXT:    cir.trap
// X86-FWD-LABEL: cir.func comdat linkonce_odr private @"?__empty_global_delete@@YAXPAXI@Z"
// X86-FWD-NEXT:    cir.trap

// Scalar wrapper emits a strong forwarding body calling scalar ::operator delete (??3@):
// X64-FWD-LABEL: cir.func comdat linkonce_odr private @"?__global_delete@@YAXPEAX_K@Z"(
// X64-FWD-SAME:  %[[ARG0:.*]]: !cir.ptr<!void>{{.*}}, %[[ARG1:.*]]: !u64i{{.*}}) {
// X64-FWD-NEXT:    cir.call @"??3@YAXPEAX_K@Z"(%[[ARG0]], %[[ARG1]])
// X64-FWD-NEXT:    cir.return
// X86-FWD-LABEL: cir.func comdat linkonce_odr private @"?__global_delete@@YAXPAXI@Z"(
// X86-FWD-SAME:  %[[ARG0:.*]]: !cir.ptr<!void>{{.*}}, %[[ARG1:.*]]: !u32i{{.*}}) {
// X86-FWD-NEXT:    cir.call @"??3@YAXPAXI@Z"(%[[ARG0]], %[[ARG1]])
// X86-FWD-NEXT:    cir.return

// Array wrapper emits a strong forwarding body calling array ::operator delete[] (??_V@):
// X64-FWD-LABEL: cir.func comdat linkonce_odr private @"?__global_array_delete@@YAXPEAX_K@Z"(
// X64-FWD-SAME:  %[[AARG0:.*]]: !cir.ptr<!void>{{.*}}, %[[AARG1:.*]]: !u64i{{.*}}) {
// X64-FWD-NEXT:    cir.call @"??_V@YAXPEAX_K@Z"(%[[AARG0]], %[[AARG1]])
// X64-FWD-NEXT:    cir.return
// X86-FWD-LABEL: cir.func comdat linkonce_odr private @"?__global_array_delete@@YAXPAXI@Z"(
// X86-FWD-SAME:  %[[AARG0:.*]]: !cir.ptr<!void>{{.*}}, %[[AARG1:.*]]: !u32i{{.*}}) {
// X86-FWD-NEXT:    cir.call @"??_V@YAXPAXI@Z"(%[[AARG0]], %[[AARG1]])
// X86-FWD-NEXT:    cir.return

// No deleting destructor for ExtClass is emitted in this TU:
// CIR-FWD-NOT: cir.func {{.*}}@"??_EExtClass@@

#endif // TEST_FORWARDING
