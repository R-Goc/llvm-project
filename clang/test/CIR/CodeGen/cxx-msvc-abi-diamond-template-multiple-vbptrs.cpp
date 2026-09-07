// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s

// Test for the fix in emitNullBaseClassInitialization where the calculation
// of SplitAfterSize was incorrect when multiple vbptrs are present.

namespace test {

class Base {
public:
  virtual ~Base() {}
};

class Left : public virtual Base {
};

class Right : public virtual Base {
};

class Diamond : public Left, public Right {
};

// Test 1: Diamond inheritance in a template that triggers the bug
template<typename T>
class Derived : public Diamond {
public:
  // CIR-LABEL: cir.func {{.*}}@"??0?$Derived@H@test@@QEAA@XZ"
  // Layout of Derived<int>:
  // offset 0: vbptr for Left (8 bytes)
  // offset 8: vbptr for Right (8 bytes)
  // offset 16+: virtual base Base

  // CIR: cir.call @"??0Diamond@test@@QEAA@XZ"
  // emitNullBaseClassInitialization calculates memory regions around
  // vbptrs without hitting negative size assertion.
  // No memset is generated since there are no gaps to zero.
  // CIR-NOT: cir.libc.memset
  // CIR: cir.return

  Derived() : Diamond() {}
};

// Explicit instantiation to trigger code generation
template class Derived<int>;

// Test 2: Diamond in a template, with data members (calls memset)
class DiamondWithData : public Left, public Right {
public:
  int x;
  int y;
};

template<typename T>
class DerivedWithData : public DiamondWithData {
public:
  DerivedWithData();
};

// CIR-LABEL: cir.func {{.*}}@"??0?$DerivedWithData@H@test@@QEAA@XZ"
// Layout of DerivedWithData<int>:
// offset 0: vbptr for Left (8 bytes)
// offset 8: vbptr for Right (8 bytes)
// offset 16: x (4 bytes)
// offset 20: y (4 bytes)
// offset 24+: virtual base Base
//
// emitNullBaseClassInitialization zero-initializes data members [16, 24)
// while skipping both vbptrs [0, 16).
// CIR: %[[MEMSET_OFFSET:.*]] = cir.const #cir.int<16> : !s32i
// CIR: %[[MEMSET_PTR:.*]] = cir.ptr_stride %{{.*}}, %[[MEMSET_OFFSET]]
// CIR: %[[MEMSET_VOID:.*]] = cir.cast bitcast %[[MEMSET_PTR]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR: cir.libc.memset %{{.*}} bytes at %[[MEMSET_VOID]] align(8) to %{{.*}}
// CIR: cir.call @"??0DiamondWithData@test@@QEAA@XZ"
// CIR: cir.return

template<typename T>
DerivedWithData<T>::DerivedWithData() : DiamondWithData() {
}

template struct DerivedWithData<int>;

// Test 3: Three vbptrs test case

// Three separate classes that virtually inherit from Base
class Middle : public virtual Base {
};

// TriDiamond has three vbptrs - one from each base class
class TriDiamond : public Left, public Middle, public Right {
};

// Test 3a: Template instantiation with three vbptrs (no data members)
template<typename T>
class TriDerived : public TriDiamond {
public:
  // CIR-LABEL: cir.func {{.*}}@"??0?$TriDerived@H@test@@QEAA@XZ"
  // Layout of TriDerived<int>:
  // offset 0:  vbptr for Left (8 bytes)
  // offset 8:  vbptr for Middle (8 bytes)
  // offset 16: vbptr for Right (8 bytes)
  // offset 24+: virtual base Base

  // CIR: cir.call @"??0TriDiamond@test@@QEAA@XZ"
  // No memset is generated since there are no gaps to zero.
  // CIR-NOT: cir.libc.memset
  // CIR: cir.return

  TriDerived() : TriDiamond() {}
};

// Explicit instantiation to trigger code generation
template class TriDerived<int>;

// Test 3b: Three vbptrs with data members (calls memset)
class TriDiamondWithData : public Left, public Middle, public Right {
public:
  int a;
  int b;
};

template<typename T>
class TriDerivedWithData : public TriDiamondWithData {
public:
  TriDerivedWithData();
};

// CIR-LABEL: cir.func {{.*}}@"??0?$TriDerivedWithData@H@test@@QEAA@XZ"
// Layout of TriDerivedWithData<int>:
// offset 0:  vbptr for Left (8 bytes)
// offset 8:  vbptr for Middle (8 bytes)
// offset 16: vbptr for Right (8 bytes)
// offset 24: a (4 bytes)
// offset 28: b (4 bytes)
// offset 32+: virtual base Base

// CIR: %[[MEMSET_OFFSET2:.*]] = cir.const #cir.int<24> : !s32i
// CIR: %[[MEMSET_PTR2:.*]] = cir.ptr_stride %{{.*}}, %[[MEMSET_OFFSET2]]
// CIR: %[[MEMSET_VOID2:.*]] = cir.cast bitcast %[[MEMSET_PTR2]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR: cir.libc.memset %{{.*}} bytes at %[[MEMSET_VOID2]] align(8) to %{{.*}}
// CIR: cir.call @"??0TriDiamondWithData@test@@QEAA@XZ"
// CIR: cir.return

template<typename T>
TriDerivedWithData<T>::TriDerivedWithData() : TriDiamondWithData() {
}

template struct TriDerivedWithData<int>;

// Test 4: Another case which triggers the bug (similar to Test 1, non-template)
class Interface {
public:
  virtual ~Interface() {}
};

class Base1If : public virtual Interface {
};

class Base2If : public virtual Interface {
};

class BaseIf : public Base1If, public Base2If {
};

class DerivedClass : public BaseIf {
public:
  // CIR-LABEL: cir.func {{.*}}@"??0DerivedClass@test@@QEAA@XZ"
  // Layout of DerivedClass:
  // offset 0: vbptr for Base1If (8 bytes)
  // offset 8: vbptr for Base2If (8 bytes)
  // offset 16+: virtual base Interface

  // CIR: cir.call @"??0BaseIf@test@@QEAA@XZ"
  // No memset is generated since there are no gaps to zero.
  // CIR-NOT: cir.libc.memset
  // CIR: cir.return

  DerivedClass()
  : BaseIf()
  { }
};

// Instantiate to trigger code generation
DerivedClass d;

// Test 4c: Non-template version with three vbptrs
class TriConcreteClass : public TriDiamond {
public:
  // CIR-LABEL: cir.func {{.*}}@"??0TriConcreteClass@test@@QEAA@XZ"
  // Layout of TriConcreteClass:
  // offset 0:  vbptr for Left (8 bytes)
  // offset 8:  vbptr for Middle (8 bytes)
  // offset 16: vbptr for Right (8 bytes)
  // offset 24+: virtual base Base

  // CIR: cir.call @"??0TriDiamond@test@@QEAA@XZ"
  // No memset is generated since there are no gaps to zero.
  // CIR-NOT: cir.libc.memset
  // CIR: cir.return

  TriConcreteClass() : TriDiamond() {}
};

// Instantiate to trigger code generation
TriConcreteClass tc;

} // namespace test
