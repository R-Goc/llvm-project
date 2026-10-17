// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fms-extensions -fms-compatibility -fno-rtti -fclangir -emit-cir %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir --check-prefix=CIR %s
// RUN: %clang_cc1 -std=c++17 -triple i386-pc-windows-msvc -fms-extensions -fms-compatibility -fno-rtti -fclangir -emit-cir %s -o %t32.cir
// RUN: FileCheck --input-file=%t32.cir --check-prefix=CIR %s

// Adapted from Clang CodeGenCXX/ms-novtable.cpp.
// __declspec(novtable) suppresses VFTable pointer stores in constructors and
// destructors of classes with this attribute, avoiding unnecessary vtable
// pointer updates for abstract interfaces.

struct __declspec(novtable) A1 {
  virtual void a();
};

struct A2 {
  virtual void a();
};

struct __declspec(novtable) B1 : virtual A1 {} b1;
struct                      B2 : virtual A1 {} b2;
struct __declspec(novtable) C  : virtual A2 {} c;

//===----------------------------------------------------------------------===//
// Module-level Globals (emitted at the top of MLIR module)
//===----------------------------------------------------------------------===//

// Non-novtable classes A2 and B2 have their VFTables emitted:
// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7A2@@6B@" = #cir.vtable<
// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7B2@@6B@" = #cir.vtable<

// Virtual base tables are still emitted for classes with virtual bases,
// including novtable classes B1 and C:
// CIR-DAG: cir.global constant linkonce_odr comdat dso_local @"??_8B1@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<{{.*}}> : !s32i]>
// CIR-DAG: cir.global constant linkonce_odr comdat dso_local @"??_8B2@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<{{.*}}> : !s32i]>
// CIR-DAG: cir.global constant linkonce_odr comdat dso_local @"??_8C@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<{{.*}}> : !s32i]>

// VFTables for novtable classes A1, B1, and C are NOT emitted:
// CIR-NOT: @"??_7A1@@6B@"
// CIR-NOT: @"??_7B1@@6B@"
// CIR-NOT: @"??_7C@@6B@"

//===----------------------------------------------------------------------===//
// Constructors
//===----------------------------------------------------------------------===//

// B1 is __declspec(novtable): initializes VBPtr, calls A1 ctor, but does NOT store VFPtr.
// CIR-LABEL: cir.func {{.*}} @"??0B1@@{{.*}}"
// CIR:   %[[VB1:.*]] = cir.get_global @"??_8B1@@7B@"
// CIR:   %[[VB1_DECAY:.*]] = cir.cast array_to_ptrdecay %[[VB1]]
// CIR:   cir.store {{.*}}%[[VB1_DECAY]]
// CIR:   cir.call @"??0A1@@{{.*}}"
// CIR-NOT: cir.vtable.address_point
// CIR-NOT: cir.vtable.get_vptr
// CIR:   cir.return

// B2 is NOT novtable: initializes VBPtr, calls A1 ctor, AND stores VFPtr.
// CIR-LABEL: cir.func {{.*}} @"??0B2@@{{.*}}"
// CIR:   %[[VB2:.*]] = cir.get_global @"??_8B2@@7B@"
// CIR:   %[[VB2_DECAY:.*]] = cir.cast array_to_ptrdecay %[[VB2]]
// CIR:   cir.store {{.*}}%[[VB2_DECAY]]
// CIR:   cir.call @"??0A1@@{{.*}}"
// CIR:   %[[AP_B2:.*]] = cir.vtable.address_point(@"??_7B2@@6B@",
// CIR:   %[[VPTR_B2:.*]] = cir.vtable.get_vptr
// CIR:   cir.store {{.*}}%[[AP_B2]], %[[VPTR_B2]]
// CIR:   cir.return

// C is __declspec(novtable): initializes VBPtr, calls A2 ctor, but does NOT store VFPtr to ??_7C.
// CIR-LABEL: cir.func {{.*}} @"??0C@@{{.*}}"
// CIR:   %[[VBC:.*]] = cir.get_global @"??_8C@@7B@"
// CIR:   %[[VBC_DECAY:.*]] = cir.cast array_to_ptrdecay %[[VBC]]
// CIR:   cir.store {{.*}}%[[VBC_DECAY]]
// CIR:   cir.call @"??0A2@@{{.*}}"
// CIR-NOT: cir.vtable.address_point
// CIR-NOT: cir.vtable.get_vptr
// CIR:   cir.return

// A1 is __declspec(novtable): constructor does NOT store VFPtr.
// CIR-LABEL: cir.func {{.*}} @"??0A1@@{{.*}}"
// CIR-NOT: cir.vtable.address_point
// CIR-NOT: cir.vtable.get_vptr
// CIR:   cir.return

// A2 is NOT novtable: constructor stores VFPtr to ??_7A2.
// CIR-LABEL: cir.func {{.*}} @"??0A2@@{{.*}}"
// CIR:   %[[AP_A2:.*]] = cir.vtable.address_point(@"??_7A2@@6B@",
// CIR:   %[[VPTR_A2:.*]] = cir.vtable.get_vptr
// CIR:   cir.store {{.*}}%[[AP_A2]], %[[VPTR_A2]]
// CIR:   cir.return
