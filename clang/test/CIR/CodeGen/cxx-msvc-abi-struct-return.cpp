// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fclangir -emit-cir %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir %s -check-prefix=X64
// RUN: %clang_cc1 -std=c++17 -triple i686-pc-windows-msvc -fclangir -emit-cir %s -o %t32.cir
// RUN: FileCheck --input-file=%t32.cir %s -check-prefix=X86

struct SmallTrivial {
  int a;
  int b;
};

struct LargeStruct {
  int a;
  int b;
  int c;
};

struct NonTrivial {
  int x;
  NonTrivial();
  ~NonTrivial();
};

struct Holder {
  SmallTrivial method_small_trivial();
  NonTrivial method_nontrivial();
};

// Free function returning trivial <= 8-byte struct (direct return).
// X64-LABEL: cir.func no_inline dso_local @"?ret_small_trivial@@YA?AUSmallTrivial@@XZ"() -> !rec_SmallTrivial
// X86-LABEL: cir.func no_inline dso_local @"?ret_small_trivial@@YA?AUSmallTrivial@@XZ"() -> !rec_SmallTrivial
SmallTrivial ret_small_trivial() {
  return SmallTrivial{1, 2};
}

// Free function returning > 8-byte struct (indirect sret at Argument 0).
// X64-LABEL: cir.func no_inline dso_local @"?ret_large@@YA?AULargeStruct@@XZ"(%arg0: !cir.ptr<!rec_LargeStruct>
// X86-LABEL: cir.func no_inline dso_local @"?ret_large@@YA?AULargeStruct@@XZ"(%arg0: !cir.ptr<!rec_LargeStruct>
LargeStruct ret_large() {
  return LargeStruct{1, 2, 3};
}

// Free function returning non-trivial struct (indirect sret at Argument 0).
// X64-LABEL: cir.func no_inline dso_local @"?ret_nontrivial@@YA?AUNonTrivial@@XZ"(%arg0: !cir.ptr<!rec_NonTrivial>
// X86-LABEL: cir.func no_inline dso_local @"?ret_nontrivial@@YA?AUNonTrivial@@XZ"(%arg0: !cir.ptr<!rec_NonTrivial>
NonTrivial ret_nontrivial() {
  NonTrivial nt;
  return nt;
}

// Instance method returning trivial <= 8-byte struct (indirect sret at Argument 1, this at Argument 0).
// X64-LABEL: cir.func no_inline dso_local @"?method_small_trivial@Holder@@QEAA?AUSmallTrivial@@XZ"(%arg0: !cir.ptr<!rec_Holder> {{.*}}, %arg1: !cir.ptr<!rec_SmallTrivial>
// X86-LABEL: cir.func no_inline dso_local @"?method_small_trivial@Holder@@QAE?AUSmallTrivial@@XZ"(%arg0: !cir.ptr<!rec_Holder> {{.*}}, %arg1: !cir.ptr<!rec_SmallTrivial>
SmallTrivial Holder::method_small_trivial() {
  return SmallTrivial{1, 2};
}

// Instance method returning non-trivial struct (indirect sret at Argument 1, this at Argument 0).
// X64-LABEL: cir.func no_inline dso_local @"?method_nontrivial@Holder@@QEAA?AUNonTrivial@@XZ"(%arg0: !cir.ptr<!rec_Holder> {{.*}}, %arg1: !cir.ptr<!rec_NonTrivial>
// X86-LABEL: cir.func no_inline dso_local @"?method_nontrivial@Holder@@QAE?AUNonTrivial@@XZ"(%arg0: !cir.ptr<!rec_Holder> {{.*}}, %arg1: !cir.ptr<!rec_NonTrivial>
NonTrivial Holder::method_nontrivial() {
  NonTrivial nt;
  return nt;
}

// Verify call sites
// X64-LABEL: cir.func no_inline dso_local @"?test_calls@@YAXAEAUHolder@@@Z"
// X86-LABEL: cir.func no_inline dso_local @"?test_calls@@YAXAAUHolder@@@Z"
void test_calls(Holder &h) {
  // Direct call to free function returning <= 8-byte trivial struct:
  // X64: %[[DIRECT:[0-9]+]] = cir.call @"?ret_small_trivial@@YA?AUSmallTrivial@@XZ"() : () -> !rec_SmallTrivial
  // X86: %[[DIRECT:[0-9]+]] = cir.call @"?ret_small_trivial@@YA?AUSmallTrivial@@XZ"() : () -> !rec_SmallTrivial
  SmallTrivial s = ret_small_trivial();

  // Indirect call to free function returning > 8-byte struct (passed directly to sret slot):
  // X64: cir.call @"?ret_large@@YA?AULargeStruct@@XZ"(%{{[0-9]+}}) : (!cir.ptr<!rec_LargeStruct>) -> ()
  // X86: cir.call @"?ret_large@@YA?AULargeStruct@@XZ"(%{{[0-9]+}}) : (!cir.ptr<!rec_LargeStruct>) -> ()
  LargeStruct l = ret_large();

  // Indirect call to free function returning non-trivial struct:
  // X64: cir.call @"?ret_nontrivial@@YA?AUNonTrivial@@XZ"(%{{[0-9]+}}) : (!cir.ptr<!rec_NonTrivial>) -> ()
  // X86: cir.call @"?ret_nontrivial@@YA?AUNonTrivial@@XZ"(%{{[0-9]+}}) : (!cir.ptr<!rec_NonTrivial>) -> ()
  NonTrivial nt = ret_nontrivial();

  // Indirect call to instance method returning trivial <= 8-byte struct (this then sret):
  // X64: %[[THIS_H:[0-9]+]] = cir.load
  // X64: cir.call @"?method_small_trivial@Holder@@QEAA?AUSmallTrivial@@XZ"(%[[THIS_H]], %{{[0-9]+}})
  // X86: %[[THIS_H:[0-9]+]] = cir.load
  // X86: cir.call @"?method_small_trivial@Holder@@QAE?AUSmallTrivial@@XZ"(%[[THIS_H]], %{{[0-9]+}}) cc(x86_thiscall)
  SmallTrivial ms = h.method_small_trivial();

  // Indirect call to instance method returning non-trivial struct (this then sret):
  // X64: %[[THIS_H2:[0-9]+]] = cir.load
  // X64: cir.call @"?method_nontrivial@Holder@@QEAA?AUNonTrivial@@XZ"(%[[THIS_H2]], %{{[0-9]+}})
  // X86: %[[THIS_H2:[0-9]+]] = cir.load
  // X86: cir.call @"?method_nontrivial@Holder@@QAE?AUNonTrivial@@XZ"(%[[THIS_H2]], %{{[0-9]+}}) cc(x86_thiscall)
  NonTrivial mnt = h.method_nontrivial();
}

// Verify call sites with temporary sret slots
// X64-LABEL: cir.func no_inline dso_local @"?test_temp_calls@@YAXAEAUHolder@@@Z"
// X86-LABEL: cir.func no_inline dso_local @"?test_temp_calls@@YAXAAUHolder@@@Z"
void test_temp_calls(Holder &h) {
  // Discarded calls allocate temporary sret slots in the entry block:
  // X64: %[[TMP_L:[0-9]+]] = cir.alloca "agg.tmp0"
  // X64: %[[TMP_MS:[0-9]+]] = cir.alloca "agg.tmp1"
  // X64: cir.call @"?ret_large@@YA?AULargeStruct@@XZ"(%[[TMP_L]])
  // X64: %[[THIS_H:[0-9]+]] = cir.load
  // X64: cir.call @"?method_small_trivial@Holder@@QEAA?AUSmallTrivial@@XZ"(%[[THIS_H]], %[[TMP_MS]])
  // X86: %[[TMP_L:[0-9]+]] = cir.alloca "agg.tmp0"
  // X86: %[[TMP_MS:[0-9]+]] = cir.alloca "agg.tmp1"
  // X86: cir.call @"?ret_large@@YA?AULargeStruct@@XZ"(%[[TMP_L]])
  // X86: %[[THIS_H:[0-9]+]] = cir.load
  // X86: cir.call @"?method_small_trivial@Holder@@QAE?AUSmallTrivial@@XZ"(%[[THIS_H]], %[[TMP_MS]]) cc(x86_thiscall)
  ret_large();
  h.method_small_trivial();
}