// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -fms-extensions -fclangir -emit-cir %s -o - | FileCheck %s --check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -fms-extensions -fclangir -emit-llvm %s -o - | FileCheck %s --check-prefix=LLVM

#pragma detect_mismatch("test_name", "test_value")
#pragma comment(lib, "msvcrt.lib")
#pragma comment(linker, "/include:my_symbol")

void foo() {}

// CIR: module {{.*}} attributes {
// CIR-SAME: cir.linker_options = [
// CIR-DAG: "/FAILIFMISMATCH:\22test_name=test_value\22"
// CIR-DAG: "/DEFAULTLIB:msvcrt.lib"
// CIR-DAG: "/include:my_symbol"
// CIR-SAME: ]

// LLVM: !llvm.linker.options = !{
// LLVM-DAG: !{!"/FAILIFMISMATCH:\22test_name=test_value\22"}
// LLVM-DAG: !{!"/DEFAULTLIB:msvcrt.lib"}
// LLVM-DAG: !{!"/include:my_symbol"}
