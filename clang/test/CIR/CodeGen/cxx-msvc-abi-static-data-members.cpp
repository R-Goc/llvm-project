// RUN: %clang_cc1 -std=c++17 -triple x86_64-windows-msvc -fms-extensions -fclangir -emit-cir %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir --check-prefix=CIR %s
// RUN: %clang_cc1 -std=c++17 -triple i386-pc-windows-msvc -fms-extensions -fclangir -emit-cir %s -o %t32.cir
// RUN: FileCheck --input-file=%t32.cir --check-prefix=CIR %s

enum Enum { zero, one, two };

struct S {
  enum Color { Red, Green, Blue };

  static const int NoInit_Ref;
  static const int Inline_NotDef_NotRef = 5;
  static const int Inline_NotDef_Ref = 5;
  static const int Inline_Def_NotRef = 5;
  static const int Inline_Def_Ref = 5;
  static const int OutOfLine_Def_NotRef;
  static const int OutOfLine_Def_Ref;

  static const Color Inline_Enum_NotRef = Blue;
  static const Color Inline_Enum_Ref = Green;
};

const int *foo1() {
  return &S::NoInit_Ref;
}

const int *foo2() {
  return &S::Inline_NotDef_Ref;
}

const int *foo3() {
  return &S::Inline_Def_Ref;
}

const int *foo4() {
  return &S::OutOfLine_Def_Ref;
}

const S::Color *foo5() {
  return &S::Inline_Enum_Ref;
}

const int S::Inline_Def_NotRef;
const int S::Inline_Def_Ref;
const int S::OutOfLine_Def_NotRef = 5;
const int S::OutOfLine_Def_Ref = 5;

struct __declspec(dllexport) ExportedS {
  // In MS compatibility mode, this counts as a definition.
  // Since it is exported, it must be emitted even if it is unreferenced.
  static const short x = 42;
  static const Enum y = two;

  struct NonExported {
    // dllexport is not inherited by this nested class.
    // Since z is unreferenced, it should not be emitted.
    static const int z = 42;
  };
};

//===----------------------------------------------------------------------===//
// Module-level Globals (emitted at the top of MLIR module)
//===----------------------------------------------------------------------===//

// CIR-DAG: cir.global "private" constant external dso_local @"?NoInit_Ref@S@@2HB" : !s32i
// CIR-DAG: cir.global constant linkonce_odr comdat dso_local @"?Inline_NotDef_Ref@S@@2HB" = #cir.int<5> : !s32i
// CIR-DAG: cir.global constant linkonce_odr comdat dso_local @"?Inline_Def_Ref@S@@2HB" = #cir.int<5> : !s32i
// CIR-DAG: cir.global constant external dso_local @"?OutOfLine_Def_Ref@S@@2HB" = #cir.int<5> : !s32i
// CIR-DAG: cir.global constant external dso_local @"?OutOfLine_Def_NotRef@S@@2HB" = #cir.int<5> : !s32i
// CIR-DAG: cir.global constant linkonce_odr comdat dso_local @"?Inline_Enum_Ref@S@@2W4Color@1@B" = #cir.int<1> : !s32i
// CIR-DAG: cir.global constant weak_odr comdat dso_local @"?x@ExportedS@@2FB" = #cir.int<42> : !s16i
// CIR-DAG: cir.global constant weak_odr comdat dso_local @"?y@ExportedS@@2W4Enum@@B" = #cir.int<2> : !s32i

// CIR-NOT: Inline_NotDef_NotRef
// CIR-NOT: Inline_Def_NotRef
// CIR-NOT: Inline_Enum_NotRef
// CIR-NOT: NonExported

//===----------------------------------------------------------------------===//
// Functions referencing static data members
//===----------------------------------------------------------------------===//

// CIR-LABEL: cir.func {{.*}} @"?foo1@@YA{{.*}}"
// CIR:   cir.get_global @"?NoInit_Ref@S@@2HB"

// CIR-LABEL: cir.func {{.*}} @"?foo2@@YA{{.*}}"
// CIR:   cir.get_global @"?Inline_NotDef_Ref@S@@2HB"

// CIR-LABEL: cir.func {{.*}} @"?foo3@@YA{{.*}}"
// CIR:   cir.get_global @"?Inline_Def_Ref@S@@2HB"

// CIR-LABEL: cir.func {{.*}} @"?foo4@@YA{{.*}}"
// CIR:   cir.get_global @"?OutOfLine_Def_Ref@S@@2HB"

// CIR-LABEL: cir.func {{.*}} @"?foo5@@YA{{.*}}"
// CIR:   cir.get_global @"?Inline_Enum_Ref@S@@2W4Color@1@B"
