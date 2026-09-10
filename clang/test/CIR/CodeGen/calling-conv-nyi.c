// RUN: not %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir \
// RUN:   -DREGCALL %s -o %t.cir 2>&1 | FileCheck %s --check-prefix=REGCALL
// RUN: not %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir \
// RUN:   -DMS_ABI %s -o %t.cir 2>&1 | FileCheck %s --check-prefix=MS_ABI
// RUN: not %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir \
// RUN:   -x c++ -DREGCALL %s -o %t.cir 2>&1 | FileCheck %s --check-prefix=REGCALL
// RUN: not %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir \
// RUN:   -x c++ -DMS_ABI %s -o %t.cir 2>&1 | FileCheck %s --check-prefix=MS_ABI

// Target-specific calling conventions that are not representable in CIR yet. Make
// sure they are diagnosed instead of silently falling back to the C calling
// convention.

#if defined(REGCALL)
#define CC __attribute__((regcall))
#elif defined(MS_ABI)
#define CC __attribute__((ms_abi))
#endif

int CC callee(int x);

int caller(int x) { return callee(x); }

// REGCALL: ClangIR code gen Not Yet Implemented: calling convention: regcall
// MS_ABI: ClangIR code gen Not Yet Implemented: calling convention: ms_abi
