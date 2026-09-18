// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fms-extensions -fms-compatibility -fclangir -emit-cir %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir --check-prefix=CIR %s
// RUN: %clang_cc1 -std=c++17 -triple i386-pc-windows-msvc -fms-extensions -fms-compatibility -fclangir -emit-cir %s -o %t32.cir
// RUN: FileCheck --input-file=%t32.cir --check-prefix=CIR %s

//===----------------------------------------------------------------------===//
// Module-level Globals & Declarations (emitted at the top of MLIR module)
//===----------------------------------------------------------------------===//

// CIR-DAG: cir.global external dso_local dllexport @"?exported_var@@3HA" = #cir.int<42> : !s32i
// CIR-DAG: cir.global "private" constant weak_odr comdat dso_local dllexport @"??_7ExportedClass@@6B@" = #cir.vtable<
// CIR-DAG: cir.global constant weak_odr comdat dso_local dllexport @"??_8ExportedVBase@@7B@" = #cir.const_array<
// CIR-DAG: cir.global "private" constant weak_odr comdat dso_local dllexport @"??_7ExportedVBase@@6B@" = #cir.vtable<
// CIR-DAG: cir.global weak_odr comdat dso_local dllexport @"?x@?1??inlineStaticLocalsFunc@@YAHXZ@4HA" =
// CIR-DAG: cir.global "private" weak_odr comdat dso_local dllexport @"?$TSS{{.*}}@?1??inlineStaticLocalsFunc@@YAHXZ@4HA" =

__declspec(dllexport) int exported_var = 42;

//===----------------------------------------------------------------------===//
// Exported Functions
//===----------------------------------------------------------------------===//

// CIR-LABEL: cir.func {{.*}} dso_local dllexport @"?exported_func@@YA{{.*}}"()
__declspec(dllexport) void exported_func() {}

// CIR-LABEL: cir.func {{.*}} comdat weak_odr dso_local dllexport @"?inline_exported_func@@YA{{.*}}"()
__declspec(dllexport) inline void inline_exported_func() {}

void use_inline() {
  inline_exported_func();
}

//===----------------------------------------------------------------------===//
// Exported Classes & VFTables
//===----------------------------------------------------------------------===//

struct __declspec(dllexport) ExportedClass {
  virtual ~ExportedClass();
  virtual void foo();
};

// CIR-LABEL: cir.func {{.*}} dso_local dllexport @"?foo@ExportedClass@@{{.*}}"
void ExportedClass::foo() {}

// Base destructor (??1) must have dllexport:
// CIR-LABEL: cir.func {{.*}} dso_local dllexport @"??1ExportedClass@@{{.*}}"
ExportedClass::~ExportedClass() {}

//===----------------------------------------------------------------------===//
// Exported Classes with Virtual Bases & VBTables
//===----------------------------------------------------------------------===//

struct VBase {
  int x;
};

struct __declspec(dllexport) ExportedVBase : virtual VBase {
  virtual ~ExportedVBase();
};

// Complete object destructor (??_D) for class with virtual bases must have dllexport:
// CIR-LABEL: cir.func {{.*}} weak_odr dso_local dllexport @"??_DExportedVBase@@{{.*}}"
ExportedVBase::~ExportedVBase() {}

void use_vbase(ExportedVBase *evb) {
  evb->~ExportedVBase();
}

//===----------------------------------------------------------------------===//
// Exported Inline Functions with Static Locals & Guards
//===----------------------------------------------------------------------===//

int get_val();
inline int __declspec(dllexport) inlineStaticLocalsFunc() {
  static int x = 42;
  static int y = get_val();
  return x++ + y;
}

void use_inline_static() {
  inlineStaticLocalsFunc();
}

// CIR-LABEL: cir.func {{.*}} comdat weak_odr dso_local dllexport @"?inlineStaticLocalsFunc@@YA{{.*}}"()

//===----------------------------------------------------------------------===//
// Explicit Class Template Instantiation
//===----------------------------------------------------------------------===//

template <typename T>
struct S {
  void f() {}
};

template struct __declspec(dllexport) S<int>;

// CIR-LABEL: cir.func {{.*}} comdat weak_odr dso_local dllexport @"?f@?$S@H@@Q{{.*}}XXZ"

// Vector/scalar deleting destructor must NOT have dllexport:
// In 64-bit it's ??_E, in 32-bit it's ??_G. Both must have no dllexport before the symbol.
// CIR-LABEL: cir.func {{.*}} comdat linkonce_odr dso_local @"??_
// CIR-SAME: ExportedClass@@
