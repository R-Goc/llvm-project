// RUN: %clang_cc1 -std=c++20 -fno-rtti -fclangir -emit-cir -triple x86_64-pc-windows-msvc %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir --check-prefixes=CIR,CIR-X64 %s
// RUN: %clang_cc1 -std=c++20 -fno-rtti -fclangir -emit-cir -triple i386-pc-windows-msvc %s -o %t86.cir
// RUN: FileCheck --input-file=%t86.cir --check-prefixes=CIR,CIR-X86 %s

//===----------------------------------------------------------------------===//
// Module-level globals (VFTables & VBTables)
//===----------------------------------------------------------------------===//

// For classes instantiated with a local/lambda type, or classes defined locally
// within a function, VFTables and VBTables must have internal linkage and must
// NOT have a COMDAT attribute (in COFF, internal symbols cannot be in a COMDAT
// group).

// CIR-DAG: cir.global "private" constant internal dso_local @"??_7?$TClass@V<lambda_0>@?0??main@@9@@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}]> : !cir.array<!cir.ptr<!u8i> x 1>}>
// CIR-DAG: cir.global "private" constant internal dso_local @"??_7LocalClass@?1??test_local_struct@@YAXXZ@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}]> : !cir.array<!cir.ptr<!u8i> x 1>}>
// CIR-DAG: cir.global "private" constant internal dso_local @"??_8?$TVBClass@V<lambda_1>@?0??test_local_struct@@YAXXZ@@@7B@" = #cir.const_array<[{{.*}}]> : !cir.array<!s32i x 2>
// CIR-DAG: cir.global "private" constant internal dso_local @"??_7?$TVBClass@V<lambda_1>@?0??test_local_struct@@YAXXZ@@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}]> : !cir.array<!cir.ptr<!u8i> x 1>}>
// CIR-DAG: cir.global "private" constant internal dso_local @"??_7?$TClass@V<lambda_1>@?0??test_local_struct@@YAXXZ@@@6B@" = #cir.vtable<{#cir.const_array<[{{.*}}]> : !cir.array<!cir.ptr<!u8i> x 1>}>

//===----------------------------------------------------------------------===//
// Test cases
//===----------------------------------------------------------------------===//

template <typename T>
struct TClass {
  virtual ~TClass() {}
};

int main() {
  TClass<decltype([]{})> a;
}

// CIR-LABEL: cir.func no_inline dso_local @main()
// CIR-X64:   cir.call @"??0?$TClass@V<lambda_0>@?0??main@@9@@@QEAA@XZ"
// CIR-X86:   cir.call @"??0?$TClass@V<lambda_0>@?0??main@@9@@@QAE@XZ"(%{{.*}}) nothrow cc(x86_thiscall)
// CIR-X64:   cir.call @"??1?$TClass@V<lambda_0>@?0??main@@9@@@UEAA@XZ"
// CIR-X86:   cir.call @"??1?$TClass@V<lambda_0>@?0??main@@9@@@UAE@XZ"(%{{.*}}) nothrow cc(x86_thiscall)

template <typename T>
struct TVBClass : virtual TClass<T> {
  virtual ~TVBClass() {}
};

void test_local_struct() {
  struct LocalClass {
    virtual void f() {}
  };
  LocalClass lc;
  lc.f();

  TVBClass<decltype([]{})> b;
}

// CIR-LABEL: cir.func no_inline dso_local @"?test_local_struct@@YAXXZ"()
