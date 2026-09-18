// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fms-extensions -fms-compatibility -fclangir -emit-cir %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir --check-prefix=CIR %s

//===----------------------------------------------------------------------===//
// Module-level Globals (emitted at the top of MLIR module)
//===----------------------------------------------------------------------===//

// CIR-DAG: cir.global constant weak_odr comdat dso_local dllexport @"??_8B@test1@@7B@" =
// CIR-DAG: cir.global "private" constant weak_odr comdat dso_local dllexport @"??_7A@@6B@" =
// CIR-DAG: cir.global "private" constant weak_odr comdat dso_local dllexport @"??_7B@@6B@" =
// CIR-DAG: cir.global "private" constant weak_odr comdat dso_local dllexport @"??_7C@@6BA@@@" =
// CIR-DAG: cir.global "private" constant weak_odr comdat dso_local dllexport @"??_7C@@6BB@@@" =

namespace test1 {
struct A { ~A(); };
struct __declspec(dllexport) B : virtual A { };
// CIR-LABEL: cir.func {{.*}} weak_odr dso_local dllexport @"??1B@test1@@QEAA@XZ"
// Complete destructor ??_D for class with virtual base has dllexport:
// CIR-LABEL: cir.func {{.*}} weak_odr dso_local dllexport @"??_DB@test1@@QEAAXXZ"
}

struct __declspec(dllexport) A { virtual ~A(); };
struct __declspec(dllexport) B { virtual ~B(); };
struct __declspec(dllexport) C : A, B { virtual ~C(); };
C::~C() {}

// Base destructor has dllexport:
// CIR-LABEL: cir.func {{.*}} dso_local dllexport @"??1C@@UEAA@XZ"

// Vector deleting destructor must NOT have dllexport:
// CIR-LABEL: cir.func {{.*}} comdat linkonce_odr dso_local @"??_EC@@UEAAPEAXI@Z"

// This-adjustment thunk for B's destructor must NOT have dllexport:
// CIR-LABEL: cir.func {{.*}} comdat linkonce_odr dso_local @"??_EC@@W7EAAPEAXI@Z"
