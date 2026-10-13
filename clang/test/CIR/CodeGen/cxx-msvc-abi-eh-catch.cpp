// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fexceptions -fcxx-exceptions -fclangir -emit-cir %s -o - | FileCheck %s --check-prefix=CIR
// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fexceptions -fcxx-exceptions -fclangir -emit-llvm %s -o - | FileCheck %s --check-prefix=LLVM

// CIR-DAG: cir.global "private" constant external @"??_R0H@8" : !cir.ptr<!void>
// CIR-DAG: cir.global "private" constant external @"??_R0?AUA@@@8" : !cir.ptr<!void>

// LLVM-DAG: @"??_R0H@8" = external constant ptr
// LLVM-DAG: @"??_R0?AUA@@@8" = external constant ptr

extern "C" void might_throw();
extern "C" void recover() noexcept(true);
extern "C" void handle_exception(void *e) noexcept(true);

// CIR-LABEL: cir.func no_inline dso_local @catch_all() personality(@__CxxFrameHandler3)
// CIR:         cir.try {
// CIR:           cir.call @might_throw()
// CIR:           cir.yield
// CIR:         } catch all (%[[ARG:.*]]: !cir.eh_token
// CIR:           %[[CT:.*]], %{{.*}} = cir.begin_catch %[[ARG]] flags 64 : !cir.eh_token -> (!cir.catch_token, !cir.ptr<!void>)
// CIR:           cir.cleanup.scope {
// CIR:             cir.call @recover() nothrow
// CIR:             cir.yield
// CIR:           } cleanup normal {
// CIR:             cir.end_catch %[[CT]] : !cir.catch_token
// CIR:             cir.yield
// CIR:           }
// CIR:           cir.yield
// CIR:         }

// LLVM-LABEL: define dso_local void @catch_all() #{{[0-9]+}} personality ptr @__CxxFrameHandler3
// LLVM:         invoke void @might_throw()
// LLVM:                 to label %[[CONT:.*]] unwind label %[[CS_BB:.*]]
// LLVM:       [[CS_BB]]:
// LLVM-NEXT:    %[[CS:.*]] = catchswitch within none [label %[[CP_BB:.*]]] unwind to caller
// LLVM:       [[CP_BB]]:
// LLVM-NEXT:    %[[CP:.*]] = catchpad within %[[CS]] [ptr null, i32 64, ptr null]
// LLVM:         call void @recover() {{.*}} [ "funclet"(token %[[CP]]) ]
// LLVM:         catchret from %[[CP]] to label %{{.*}}
extern "C" void catch_all() {
  try {
    might_throw();
  } catch (...) {
    recover();
  }
}

// CIR-LABEL: cir.func no_inline dso_local @catch_int() personality(@__CxxFrameHandler3)
// CIR:         %[[E_ADDR:.*]] = cir.alloca "e"
// CIR:         cir.try {
// CIR:           cir.call @might_throw()
// CIR:           cir.yield
// CIR:         } catch [type #cir.global_view<@"??_R0H@8"> : !cir.ptr<!void>] (%[[ARG:.*]]: !cir.eh_token
// CIR:           %[[CT:.*]], %[[EXN:.*]] = cir.begin_catch %[[ARG]] param %[[E_ADDR]] : !cir.ptr<!s32i> type <@"??_R0H@8"> flags 0 : !cir.eh_token -> (!cir.catch_token, !cir.ptr<!void>)
// CIR:           cir.cleanup.scope {
// CIR:             cir.call @handle_exception(
// CIR:             cir.yield
// CIR:           } cleanup normal {
// CIR:             cir.end_catch %[[CT]] : !cir.catch_token
// CIR:             cir.yield
// CIR:           }
// CIR:           cir.yield
// CIR:         }

// LLVM-LABEL: define dso_local void @catch_int() #{{[0-9]+}} personality ptr @__CxxFrameHandler3
// LLVM:         %[[E:.*]] = alloca i32
// LLVM:         invoke void @might_throw()
// LLVM:                 to label %{{.*}} unwind label %[[CS_BB:.*]]
// LLVM:       [[CS_BB]]:
// LLVM-NEXT:    %[[CS:.*]] = catchswitch within none [label %[[CP_BB:.*]]] unwind to caller
// LLVM:       [[CP_BB]]:
// LLVM-NEXT:    %[[CP:.*]] = catchpad within %[[CS]] [ptr @"??_R0H@8", i32 0, ptr %[[E]]]
// LLVM:         call void @handle_exception(ptr noundef %[[E]]) {{.*}} [ "funclet"(token %[[CP]]) ]
// LLVM:         catchret from %[[CP]] to label %{{.*}}
extern "C" void catch_int() {
  try {
    might_throw();
  } catch (int e) {
    handle_exception(&e);
  }
}

// CIR-LABEL: cir.func no_inline dso_local @catch_int_unnamed() personality(@__CxxFrameHandler3)
// CIR:         cir.try {
// CIR:           cir.call @might_throw()
// CIR:           cir.yield
// CIR:         } catch [type #cir.global_view<@"??_R0H@8"> : !cir.ptr<!void>] (%[[ARG:.*]]: !cir.eh_token
// CIR:           %[[CT:.*]], %{{.*}} = cir.begin_catch %[[ARG]] type <@"??_R0H@8"> flags 0 : !cir.eh_token -> (!cir.catch_token, !cir.ptr<!void>)
// CIR:           cir.cleanup.scope {
// CIR:             cir.yield
// CIR:           } cleanup normal {
// CIR:             cir.end_catch %[[CT]] : !cir.catch_token
// CIR:             cir.yield
// CIR:           }
// CIR:           cir.yield
// CIR:         }

// LLVM-LABEL: define dso_local void @catch_int_unnamed() #{{[0-9]+}} personality ptr @__CxxFrameHandler3
// LLVM:         invoke void @might_throw()
// LLVM:                 to label %{{.*}} unwind label %[[CS_BB:.*]]
// LLVM:       [[CS_BB]]:
// LLVM-NEXT:    %[[CS:.*]] = catchswitch within none [label %[[CP_BB:.*]]] unwind to caller
// LLVM:       [[CP_BB]]:
// LLVM-NEXT:    %[[CP:.*]] = catchpad within %[[CS]] [ptr @"??_R0H@8", i32 0, ptr null]
// LLVM:         catchret from %[[CP]] to label %{{.*}}
extern "C" void catch_int_unnamed() {
  try {
    might_throw();
  } catch (int) {
  }
}

struct A {
  A();
  A(const A &o);
  ~A();
  int a;
};

// CIR-LABEL: cir.func no_inline dso_local @catch_a_byval() personality(@__CxxFrameHandler3)
// CIR:         %[[E_ADDR:.*]] = cir.alloca "e"
// CIR:         cir.try {
// CIR:           cir.call @might_throw()
// CIR:           cir.yield
// CIR:         } catch [type #cir.global_view<@"??_R0?AUA@@@8"> : !cir.ptr<!void>] (%[[ARG:.*]]: !cir.eh_token
// CIR:           %[[CT:.*]], %[[EXN:.*]] = cir.begin_catch %[[ARG]] param %[[E_ADDR]] : !cir.ptr<!rec_A> type <@"??_R0?AUA@@@8"> flags 0 : !cir.eh_token -> (!cir.catch_token, !cir.ptr<!void>)
// CIR:           cir.cleanup.scope {
// CIR:             cir.call @handle_exception(
// CIR:             cir.yield
// CIR:           } cleanup normal {
// CIR:             cir.end_catch %[[CT]] : !cir.catch_token
// CIR:             cir.yield
// CIR:           }
// CIR:           cir.yield
// CIR:         }

// LLVM-LABEL: define dso_local void @catch_a_byval() #{{[0-9]+}} personality ptr @__CxxFrameHandler3
// LLVM:         %[[E:.*]] = alloca %struct.A
// LLVM:         invoke void @might_throw()
// LLVM:                 to label %{{.*}} unwind label %[[CS_BB:.*]]
// LLVM:       [[CS_BB]]:
// LLVM-NEXT:    %[[CS:.*]] = catchswitch within none [label %[[CP_BB:.*]]] unwind to caller
// LLVM:       [[CP_BB]]:
// LLVM-NEXT:    %[[CP:.*]] = catchpad within %[[CS]] [ptr @"??_R0?AUA@@@8", i32 0, ptr %[[E]]]
// LLVM:         call void @handle_exception(ptr noundef %[[E]]) {{.*}} [ "funclet"(token %[[CP]]) ]
// LLVM:         call void @"??1A@@QEAA@XZ"(ptr {{.*}} %[[E]]) {{.*}} [ "funclet"(token %[[CP]]) ]
// LLVM:         catchret from %[[CP]] to label %{{.*}}
extern "C" void catch_a_byval() {
  try {
    might_throw();
  } catch (A e) {
    handle_exception(&e);
  }
}

// CIR-LABEL: cir.func no_inline dso_local @catch_a_ref() personality(@__CxxFrameHandler3)
// CIR:         %[[E_ADDR:.*]] = cir.alloca "e"
// CIR:         cir.try {
// CIR:           cir.call @might_throw()
// CIR:           cir.yield
// CIR:         } catch [type #cir.global_view<@"??_R0?AUA@@@8"> : !cir.ptr<!void>] (%[[ARG:.*]]: !cir.eh_token
// CIR:           %[[CT:.*]], %[[EXN:.*]] = cir.begin_catch %[[ARG]] param %[[E_ADDR]] : !cir.ptr<!cir.ptr<!rec_A>> type <@"??_R0?AUA@@@8"> flags 8 : !cir.eh_token -> (!cir.catch_token, !cir.ptr<!void>)
// CIR:           cir.cleanup.scope {
// CIR:             cir.call @handle_exception(
// CIR:             cir.yield
// CIR:           } cleanup normal {
// CIR:             cir.end_catch %[[CT]] : !cir.catch_token
// CIR:             cir.yield
// CIR:           }
// CIR:           cir.yield
// CIR:         }

// LLVM-LABEL: define dso_local void @catch_a_ref() #{{[0-9]+}} personality ptr @__CxxFrameHandler3
// LLVM:         %[[E:.*]] = alloca ptr
// LLVM:         invoke void @might_throw()
// LLVM:                 to label %{{.*}} unwind label %[[CS_BB:.*]]
// LLVM:       [[CS_BB]]:
// LLVM-NEXT:    %[[CS:.*]] = catchswitch within none [label %[[CP_BB:.*]]] unwind to caller
// LLVM:       [[CP_BB]]:
// LLVM-NEXT:    %[[CP:.*]] = catchpad within %[[CS]] [ptr @"??_R0?AUA@@@8", i32 8, ptr %[[E]]]
// LLVM:         %[[EPTR:.*]] = load ptr, ptr %[[E]]
// LLVM:         call void @handle_exception(ptr noundef %[[EPTR]]) {{.*}} [ "funclet"(token %[[CP]]) ]
// LLVM:         catchret from %[[CP]] to label %{{.*}}
extern "C" void catch_a_ref() {
  try {
    might_throw();
  } catch (A &e) {
    handle_exception(&e);
  }
}

// CIR-LABEL: cir.func no_inline dso_local @catch_nested() personality(@__CxxFrameHandler3)
// CIR:         cir.try {
// CIR:           cir.call @might_throw()
// CIR:           cir.yield
// CIR:         } catch [type #cir.global_view<@"??_R0H@8"> : !cir.ptr<!void>] (%[[ARG0:.*]]: !cir.eh_token
// CIR:           %[[CT0:.*]], %{{.*}} = cir.begin_catch %[[ARG0]] type <@"??_R0H@8"> flags 0 : !cir.eh_token -> (!cir.catch_token, !cir.ptr<!void>)
// CIR:           cir.cleanup.scope {
// CIR:             cir.scope {
// CIR:               cir.try {
// CIR:                 cir.call @might_throw()
// CIR:                 cir.yield
// CIR:               } catch [type #cir.global_view<@"??_R0H@8"> : !cir.ptr<!void>] (%[[ARG1:.*]]: !cir.eh_token
// CIR:                 %[[CT1:.*]], %{{.*}} = cir.begin_catch %[[ARG1]] type <@"??_R0H@8"> flags 0 : !cir.eh_token -> (!cir.catch_token, !cir.ptr<!void>)
// CIR:                 cir.cleanup.scope {
// CIR:                   cir.call @might_throw()
// CIR:                   cir.yield
// CIR:                 } cleanup normal {
// CIR:                   cir.end_catch %[[CT1]] : !cir.catch_token
// CIR:                   cir.yield
// CIR:                 }
// CIR:                 cir.yield
// CIR:               }
// CIR:             }
// CIR:             cir.yield
// CIR:           } cleanup normal {
// CIR:             cir.end_catch %[[CT0]] : !cir.catch_token
// CIR:             cir.yield
// CIR:           }
// CIR:           cir.yield
// CIR:         }

// LLVM-LABEL: define dso_local void @catch_nested() #{{[0-9]+}} personality ptr @__CxxFrameHandler3
// LLVM:         invoke void @might_throw()
// LLVM:                 to label %{{.*}} unwind label %[[OUTER_CS_BB:.*]]
// LLVM:       [[OUTER_CS_BB]]:
// LLVM-NEXT:    %[[OUTER_CS:.*]] = catchswitch within none [label %[[OUTER_CP_BB:.*]]] unwind to caller
// LLVM:       [[OUTER_CP_BB]]:
// LLVM-NEXT:    %[[OUTER_CP:.*]] = catchpad within %[[OUTER_CS]] [ptr @"??_R0H@8", i32 0, ptr null]
// LLVM:         invoke void @might_throw() [ "funclet"(token %[[OUTER_CP]]) ]
// LLVM:                 to label %{{.*}} unwind label %[[INNER_CS_BB:.*]]
// LLVM:       [[INNER_CS_BB]]:
// LLVM-NEXT:    %[[INNER_CS:.*]] = catchswitch within %[[OUTER_CP]] [label %[[INNER_CP_BB:.*]]] unwind to caller
// LLVM:       [[INNER_CP_BB]]:
// LLVM-NEXT:    %[[INNER_CP:.*]] = catchpad within %[[INNER_CS]] [ptr @"??_R0H@8", i32 0, ptr null]
// LLVM:         call void @might_throw() [ "funclet"(token %[[INNER_CP]]) ]
// LLVM:         catchret from %[[INNER_CP]] to label %{{.*}}
// LLVM:         catchret from %[[OUTER_CP]] to label %{{.*}}
extern "C" void catch_nested() {
  try {
    might_throw();
  } catch (int) {
    try {
      might_throw();
    } catch (int) {
      might_throw();
    }
  }
}
