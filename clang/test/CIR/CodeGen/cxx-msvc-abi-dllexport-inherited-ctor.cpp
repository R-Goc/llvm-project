// RUN: %clang_cc1 -std=c++20 -triple x86_64-pc-windows-msvc -fms-extensions -fms-compatibility -fclangir -emit-cir %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir --check-prefixes=CIR,CIR64 %s
// RUN: %clang_cc1 -std=c++20 -triple i686-pc-windows-msvc -fms-extensions -fms-compatibility -fclangir -emit-cir %s -o %t32.cir
// RUN: FileCheck --input-file=%t32.cir --check-prefixes=CIR,CIR32 %s

// Inherited constructors via 'using Base::Base' in a dllexport class
// must be emitted and marked dllexport. Non-exported classes should
// not emit inherited constructors unless used.

struct Base {
  Base(int);
};

struct __declspec(dllexport) Child : Base {
  using Base::Base;
};

struct NonExported : Base {
  using Base::Base;
};

// CIR64: cir.func {{.*}} comdat weak_odr dso_local dllexport @"??0Child@@QEAA@H@Z"
// CIR32: cir.func {{.*}} comdat weak_odr dso_local dllexport @"??0Child@@QAE@H@Z"

// CIR-NOT: ??0NonExported@@
