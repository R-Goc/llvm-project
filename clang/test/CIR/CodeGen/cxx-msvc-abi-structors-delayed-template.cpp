// RUN: %clang_cc1 -fclangir -emit-cir -fdelayed-template-parsing -std=c++11 -triple=i386-pc-win32 %s -o - | FileCheck %s

//===----------------------------------------------------------------------===//
// Module-level Globals (emitted at the top of MLIR module)
//===----------------------------------------------------------------------===//

// CHECK-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7ImplicitCtor@vtable_referenced_from_template@@6B@" = #cir.vtable<

// PR20671
namespace vtable_referenced_from_template {
struct ImplicitCtor {
  virtual ~ImplicitCtor();
};
template <class T> void foo(T t) { new ImplicitCtor; }
void bar() { foo(0); }

// CHECK-LABEL: cir.func {{.*}} @"??0ImplicitCtor@vtable_referenced_from_template@@QAE@XZ"
// CHECK:   %[[AP:.*]] = cir.vtable.address_point(@"??_7ImplicitCtor@vtable_referenced_from_template@@6B@", address_point = <index = 0, offset = 0>)
// CHECK:   %[[VPTR:.*]] = cir.vtable.get_vptr %{{.*}}
// CHECK:   cir.store {{.*}} %[[AP]], %[[VPTR]]
}
