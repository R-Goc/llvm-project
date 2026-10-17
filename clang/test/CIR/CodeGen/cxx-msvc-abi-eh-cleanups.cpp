// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -fexceptions -fcxx-exceptions -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -fexceptions -fcxx-exceptions -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s --check-prefix=LLVM
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -fclangir -emit-cir %s -o %t-noeh.cir
// RUN: FileCheck --input-file=%t-noeh.cir %s --check-prefix=CIR-NOEH
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -fclangir -emit-llvm %s -o %t-noeh.ll
// RUN: FileCheck --input-file=%t-noeh.ll %s --check-prefix=LLVM-NOEH

struct S {
  ~S();
};

void mayThrow();

// CIR-LABEL: cir.func no_inline dso_local @"?test_cleanup@@YAXXZ"()
// CIR:         %[[S_ADDR:.*]] = cir.alloca "s"
// CIR:         cir.cleanup.scope {
// CIR:           cir.call @"?mayThrow@@YAXXZ"()
// CIR:           cir.yield
// CIR:         } cleanup all {
// CIR:           cir.call @"??1S@@QEAA@XZ"(%[[S_ADDR]]) nothrow
// CIR:           cir.yield
// CIR:         }
// CIR:         cir.return

// LLVM-LABEL: define dso_local void @"?test_cleanup@@YAXXZ"() {{.*}} personality ptr @__CxxFrameHandler3
// LLVM:         %[[S_ALLOCA:.*]] = alloca %struct.S
// LLVM:         invoke void @"?mayThrow@@YAXXZ"()
// LLVM:                 to label %[[CONT:.*]] unwind label %[[CLEANUP:.*]]
// LLVM:       [[CLEANUP]]:
// LLVM-NEXT:    %[[PAD:.*]] = cleanuppad within none []
// LLVM-NEXT:    call void @"??1S@@QEAA@XZ"(ptr {{.*}} %[[S_ALLOCA]]) {{.*}} [ "funclet"(token %[[PAD]]) ]
// LLVM-NEXT:    cleanupret from %[[PAD]] unwind to caller
void test_cleanup() {
  S s;
  mayThrow();
}

// CIR-LABEL: cir.func no_inline dso_local @"?test_terminate@@YAXXZ"() personality(@__CxxFrameHandler3)
// CIR:         cir.try {
// CIR:           cir.call @"?mayThrow@@YAXXZ"()
// CIR:           cir.yield
// CIR:         } catch all (%[[EH_TOKEN:.*]]: !cir.eh_token
// CIR:           cir.eh.terminate %[[EH_TOKEN]] : !cir.eh_token
// CIR:         }
// CIR:         cir.return

// LLVM-LABEL: define dso_local void @"?test_terminate@@YAXXZ"() {{.*}} personality ptr @__CxxFrameHandler3
// LLVM:         invoke void @"?mayThrow@@YAXXZ"()
// LLVM:                 to label %[[TERM_CONT:.*]] unwind label %[[TERM_PAD:.*]]
// LLVM:       [[TERM_PAD]]:
// LLVM-NEXT:    %[[TPAD:.*]] = cleanuppad within none []
// LLVM-NEXT:    call void @__std_terminate() {{.*}} [ "funclet"(token %[[TPAD]]) ]
// LLVM-NEXT:    unreachable
void test_terminate() noexcept {
  mayThrow();
}

// CIR-NOEH-LABEL: cir.func no_inline dso_local @"?test_cleanup@@YAXXZ"()
// CIR-NOEH:         cir.cleanup.scope {
// CIR-NOEH:           cir.call @"?mayThrow@@YAXXZ"()
// CIR-NOEH:           cir.yield
// CIR-NOEH:         } cleanup normal {
// CIR-NOEH:           cir.call @"??1S@@QEAA@XZ"(
// CIR-NOEH:           cir.yield
// CIR-NOEH:         }
// CIR-NOEH:         cir.return

// CIR-NOEH-LABEL: cir.func no_inline dso_local @"?test_terminate@@YAXXZ"()
// CIR-NOEH-NOT:   personality
// CIR-NOEH-NOT:   cir.try
// CIR-NOEH:       cir.call @"?mayThrow@@YAXXZ"()
// CIR-NOEH:       cir.return

// LLVM-NOEH-LABEL: define dso_local void @"?test_cleanup@@YAXXZ"()
// LLVM-NOEH-NOT:   personality
// LLVM-NOEH-NOT:   invoke
// LLVM-NOEH:       call void @"?mayThrow@@YAXXZ"()
// LLVM-NOEH:       call void @"??1S@@QEAA@XZ"(
// LLVM-NOEH:       ret void

// LLVM-NOEH-LABEL: define dso_local void @"?test_terminate@@YAXXZ"()
// LLVM-NOEH-NOT:   personality
// LLVM-NOEH-NOT:   invoke
// LLVM-NOEH:       call void @"?mayThrow@@YAXXZ"()
// LLVM-NOEH:       ret void
