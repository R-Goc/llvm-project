// RUN: %clang_cc1 -std=c++17 -fclangir -emit-cir %s -triple x86_64-windows-msvc -o - | FileCheck %s

// Make sure virtual base destructors get referenced and emitted if
// necessary when the complete ("vbase") destructor is emitted.
struct HasDtor { ~HasDtor(); };
struct DefaultedDtor {
  ~DefaultedDtor() = default;
  HasDtor o;
};
struct HasCompleteDtor : virtual DefaultedDtor {
  ~HasCompleteDtor();
};
void useCompleteDtor(HasCompleteDtor *p) { delete p; }

// CHECK-LABEL: cir.func {{.*}} @"?useCompleteDtor@@YAXPEAUHasCompleteDtor@@@Z"
// CHECK:         cir.call @"??_DHasCompleteDtor@@QEAAXXZ"

// CHECK-LABEL: cir.func {{.*}} comdat linkonce_odr {{.*}} @"??_DHasCompleteDtor@@QEAAXXZ"
// CHECK:         cir.call @"??1HasCompleteDtor@@QEAA@XZ"
// CHECK:         cir.call @"??1DefaultedDtor@@QEAA@XZ"

// CHECK-LABEL: cir.func {{.*}} comdat linkonce_odr {{.*}} @"??1DefaultedDtor@@QEAA@XZ"
// CHECK:         cir.call @"??1HasDtor@@QEAA@XZ"
