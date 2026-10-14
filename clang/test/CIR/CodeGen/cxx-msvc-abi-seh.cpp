// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fms-extensions -fclangir -emit-cir %s -o - | FileCheck %s --check-prefix=CIR
// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fms-extensions -fclangir -emit-llvm %s -o - | FileCheck %s --check-prefix=LLVM

extern "C" void might_throw();
extern "C" void recover();
extern "C" void take_code(int);

// CIR-LABEL: cir.func no_inline dso_local @test_seh_simple() personality(@__C_specific_handler)
// CIR:         %[[CODE:.*]] = cir.alloca "__exception_code"
// CIR:         cir.try {
// CIR:           cir.call @might_throw()
// CIR:           cir.yield
// CIR:         } catch all (%[[ARG:.*]]: !cir.eh_token
// CIR:           %[[CT:.*]], %{{.*}} = cir.begin_catch %[[ARG]] flags 0 : !cir.eh_token -> (!cir.catch_token, !cir.ptr<!void>)
// CIR:           cir.cleanup.scope {
// CIR:             cir.call @recover()
// CIR:             cir.yield
// CIR:           } cleanup normal {
// CIR:             cir.end_catch %[[CT]] : !cir.catch_token
// CIR:             cir.yield
// CIR:           }
// CIR:           cir.yield
// CIR:         }

// LLVM-LABEL: define dso_local void @test_seh_simple() #{{[0-9]+}} personality ptr @__C_specific_handler
// LLVM:         invoke void @might_throw()
// LLVM:                 to label %[[CONT:.*]] unwind label %[[CS_BB:.*]]
// LLVM:       [[CS_BB]]:
// LLVM-NEXT:    %[[CS:.*]] = catchswitch within none [label %[[CP_BB:.*]]] unwind to caller
// LLVM:       [[CP_BB]]:
// LLVM-NEXT:    %[[CP:.*]] = catchpad within %[[CS]] [ptr null]
// LLVM:         call void @recover(){{.*}}[ "funclet"(token %[[CP]]) ]
// LLVM:         catchret from %[[CP]] to label %{{.*}}
extern "C" void test_seh_simple() {
  __try {
    might_throw();
  } __except (1) {
    recover();
  }
}

// CIR-LABEL: cir.func no_inline dso_local @test_seh_leave() personality(@__C_specific_handler)
// CIR:         cir.try {
// CIR:           cir.call @might_throw()
// CIR:           cir.br ^[[EXIT:bb[0-9]+]]
// CIR:         ^[[EXIT]]:
// CIR:           cir.yield
// CIR:         ^[[DEAD:bb[0-9]+]]:
// CIR:           cir.call @might_throw()
// CIR:           cir.br ^[[EXIT]]
// CIR:         }

// LLVM-LABEL: define dso_local void @test_seh_leave() #{{[0-9]+}} personality ptr @__C_specific_handler
// LLVM:         invoke void @might_throw()
// LLVM:                 to label %{{.*}} unwind label %[[CS_LEAVE:.*]]
// LLVM:       [[CS_LEAVE]]:
// LLVM-NEXT:    %[[CS_L:.*]] = catchswitch within none [label %[[CP_LEAVE:.*]]] unwind to caller
// LLVM:       [[CP_LEAVE]]:
// LLVM-NEXT:    %[[CP_L:.*]] = catchpad within %[[CS_L]] [ptr null]
// LLVM:         call void @recover(){{.*}}[ "funclet"(token %[[CP_L]]) ]
// LLVM:         catchret from %[[CP_L]] to label %{{.*}}
extern "C" void test_seh_leave() {
  __try {
    might_throw();
    __leave;
    might_throw();
  } __except (1) {
    recover();
  }
}

// CIR-LABEL: cir.func no_inline dso_local @test_seh_exception_code() personality(@__C_specific_handler)
// CIR:         %[[SLOT:.*]] = cir.alloca "__exception_code"
// CIR:         cir.try {
// CIR:           cir.call @might_throw()
// CIR:           cir.yield
// CIR:         } catch all (
// CIR:           %[[CODE:.*]] = cir.load %[[SLOT]] : !cir.ptr<!s32i>, !s32i
// CIR:           cir.call @take_code(%[[CODE]])

// LLVM-LABEL: define dso_local void @test_seh_exception_code() #{{[0-9]+}} personality ptr @__C_specific_handler
// LLVM:         invoke void @might_throw()
// LLVM:                 to label %{{.*}} unwind label %[[CS_EXC:.*]]
// LLVM:       [[CS_EXC]]:
// LLVM-NEXT:    %[[CS_E:.*]] = catchswitch within none [label %[[CP_EXC:.*]]] unwind to caller
// LLVM:       [[CP_EXC]]:
// LLVM-NEXT:    %[[CP_E:.*]] = catchpad within %[[CS_E]] [ptr null]
// LLVM:         %[[VAL:.*]] = load i32, ptr %{{.*}}
// LLVM:         call void @take_code(i32 noundef %[[VAL]]){{.*}}[ "funclet"(token %[[CP_E]]) ]
// LLVM:         catchret from %[[CP_E]] to label %{{.*}}
extern "C" void test_seh_exception_code() {
  __try {
    might_throw();
  } __except (1) {
    take_code(_exception_code());
  }
}
