// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fms-extensions -fms-compatibility -fclangir -emit-cir %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir --check-prefix=CIR %s
// RUN: %clang_cc1 -std=c++17 -triple i386-pc-windows-msvc -fms-extensions -fms-compatibility -fclangir -emit-cir %s -o %t32.cir
// RUN: FileCheck --input-file=%t32.cir --check-prefix=CIR %s

//===----------------------------------------------------------------------===//
// Module-level Globals (emitted at the top of MLIR module)
//===----------------------------------------------------------------------===//

// CIR-DAG: cir.global "private" external dllimport @"?imported_var@@3HA" : !s32i
// CIR-DAG: cir.global constant available_externally dllimport @"??_8ImportedVBase@@7B@" = #cir.const_array<

__declspec(dllimport) extern int imported_var;
__declspec(dllimport) void imported_func();

// CIR-LABEL: cir.func {{.*}} @"?use_imported@@YA{{.*}}"
// CIR:   cir.call @"?imported_func@@YA{{.*}}"
// CIR:   cir.get_global @"?imported_var@@3HA"
int use_imported() {
  imported_func();
  return imported_var;
}

// Imported function declaration:
// CIR-DAG: cir.func private dllimport @"?imported_func@@YA{{.*}}"

//===----------------------------------------------------------------------===//
// Imported Classes with Virtual Bases & VBTables
//===----------------------------------------------------------------------===//

struct Base {
  int a;
};

struct __declspec(dllimport) ImportedVBase : virtual Base {
  ImportedVBase() {}
  virtual ~ImportedVBase();
};

// CIR-LABEL: cir.func {{.*}} @"?use_imported_vbase@@{{.*}}"
// CIR:   cir.call @"??0ImportedVBase@@{{.*}}"

// CIR-LABEL: cir.func {{.*}} available_externally dllimport @"??0ImportedVBase@@{{.*}}"
// CIR:   cir.get_global @"??_8ImportedVBase@@7B@"

void use_imported_vbase(ImportedVBase *p) {
  ImportedVBase obj;
}
