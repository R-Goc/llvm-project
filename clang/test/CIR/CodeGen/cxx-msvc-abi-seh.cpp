// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fms-extensions -fclangir -emit-cir %s -o - | FileCheck %s --check-prefix=CIR
// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fms-extensions -fclangir -emit-llvm %s -o - | FileCheck %s --check-prefix=LLVM

extern "C" void might_throw();
extern "C" void recover();
extern "C" void take_code(int);
extern "C" void cleanup();
extern "C" int filter_func(void *);
extern "C" int filter_with_var(int, int);

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
// CIR:           cir.call @take_code({{.*}})

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

// CIR-LABEL: cir.func internal private @"?fin$0@0@test_seh_finally_simple@@"(%arg0: !u8i {{.*}}, %arg1: !cir.ptr<!void> {{.*}})
// CIR:         cir.call @cleanup()
// CIR:         cir.return

// CIR-LABEL: cir.func no_inline dso_local @test_seh_finally_simple() personality(@__C_specific_handler)
// CIR:         %[[ABNORMAL:.*]] = cir.alloca "abnormal_termination.slot"
// CIR:         %[[ONE:.*]] = cir.const #cir.int<1> : !u8i
// CIR:         cir.store align(1) %[[ONE]], %[[ABNORMAL]]
// CIR:         cir.cleanup.scope {
// CIR:           cir.call @might_throw()
// CIR:           %[[ZERO:.*]] = cir.const #cir.int<0> : !u8i
// CIR:           cir.store align(1) %[[ZERO]], %[[ABNORMAL]]
// CIR:           cir.yield
// CIR:         } cleanup all {
// CIR:           %[[VAL:.*]] = cir.load align(1) %[[ABNORMAL]]
// CIR:           %[[FP:.*]] = cir.call_llvm_intrinsic "localaddress"
// CIR:           cir.call @"?fin$0@0@test_seh_finally_simple@@"(%[[VAL]], %[[FP]])
// CIR:           cir.yield
// CIR:         }

// LLVM-LABEL: define internal void @"?fin$0@0@test_seh_finally_simple@@"(i8 %0, ptr %1)
// LLVM:         call void @cleanup()
// LLVM:         ret void

// LLVM-LABEL: define dso_local void @test_seh_finally_simple() #{{[0-9]+}} personality ptr @__C_specific_handler
// LLVM:         invoke void @might_throw()
// LLVM:                 to label %{{.*}} unwind label %[[CLEANUP_BB:.*]]
// LLVM:       [[CLEANUP_BB]]:
// LLVM-NEXT:    %[[CP:.*]] = cleanuppad within none []
// LLVM:         %[[VAL:.*]] = load i8, ptr %{{.*}}
// LLVM:         %[[FP:.*]] = call ptr @llvm.localaddress()
// LLVM:         call void @"?fin$0@0@test_seh_finally_simple@@"(i8 %[[VAL]], ptr %[[FP]]) #{{[0-9]+}} [ "funclet"(token %[[CP]]) ]
// LLVM:         cleanupret from %[[CP]] unwind to caller
extern "C" void test_seh_finally_simple() {
  __try {
    might_throw();
  } __finally {
    cleanup();
  }
}

// CIR-LABEL: cir.func internal private @"?fin$0@0@test_seh_finally_abnormal@@"(%arg0: !u8i {{.*}}, %arg1: !cir.ptr<!void> {{.*}})
// CIR:         %[[CAST:.*]] = cir.cast integral %arg0 : !u8i -> !s32i
// CIR:         cir.call @take_code(%[[CAST]])
// CIR:         cir.return

// LLVM-LABEL: define internal void @"?fin$0@0@test_seh_finally_abnormal@@"(i8 %0, ptr %1)
// LLVM:         %[[EXT:.*]] = zext i8 %0 to i32
// LLVM:         call void @take_code(i32 noundef %[[EXT]])
// LLVM:         ret void
extern "C" void test_seh_finally_abnormal() {
  __try {
    might_throw();
  } __finally {
    take_code(_abnormal_termination());
  }
}

// CIR-LABEL: cir.func internal private @"?fin$0@0@test_seh_finally_captured@@"(%arg0: !u8i {{.*}}, %arg1: !cir.ptr<!void> {{.*}})
// CIR:         %[[PARENT:.*]] = cir.const #cir.global_view<@test_seh_finally_captured> : !cir.ptr<!void>
// CIR:         %[[IDX:.*]] = cir.const #cir.int<0> : !s32i
// CIR:         %[[RECOV:.*]] = cir.call_llvm_intrinsic "localrecover" %[[PARENT]], %arg1, %[[IDX]] : (!cir.ptr<!void>, !cir.ptr<!void>, !s32i) -> !cir.ptr<!void>
// CIR:         %[[TYPED:.*]] = cir.cast bitcast %[[RECOV]] : !cir.ptr<!void> -> !cir.ptr<!s32i>
// CIR:         %[[LOAD:.*]] = cir.load align(4) %[[TYPED]] : !cir.ptr<!s32i>, !s32i
// CIR:         cir.call @take_code(%[[LOAD]])
// CIR:         cir.return

// CIR-LABEL: cir.func no_inline dso_local @test_seh_finally_captured() personality(@__C_specific_handler)
// CIR:         %[[X:.*]] = cir.alloca "x"
// CIR:         cir.call_llvm_intrinsic "localescape" %[[X]] : (!cir.ptr<!s32i>) -> ()

// LLVM-LABEL: define internal void @"?fin$0@0@test_seh_finally_captured@@"(i8 %0, ptr %1)
// LLVM:         %[[REC:.*]] = call ptr @llvm.localrecover(ptr @test_seh_finally_captured, ptr %1, i32 0)
// LLVM:         %[[VAL:.*]] = load i32, ptr %[[REC]], align 4
// LLVM:         call void @take_code(i32 noundef %[[VAL]])
// LLVM:         ret void

// LLVM-LABEL: define dso_local void @test_seh_finally_captured() #{{[0-9]+}} personality ptr @__C_specific_handler
// LLVM:         %[[X_SLOT:.*]] = alloca i32, align 4
// LLVM:         call void (...) @llvm.localescape(ptr %[[X_SLOT]])
extern "C" void test_seh_finally_captured() {
  int x = 42;
  __try {
    might_throw();
  } __finally {
    take_code(x);
  }
}

// CIR-LABEL: cir.func no_inline dso_local @test_seh_finally_leave() personality(@__C_specific_handler)
// CIR:         %[[ABN_LEAVE:.*]] = cir.alloca "abnormal_termination.slot"
// CIR:         cir.cleanup.scope {
// CIR:           cir.call @might_throw()
// CIR:           %[[Z:.*]] = cir.const #cir.int<0> : !u8i
// CIR:           cir.store align(1) %[[Z]], %[[ABN_LEAVE]]
// CIR:           cir.br ^[[EXIT_BB:bb[0-9]+]]
// CIR:         ^[[EXIT_BB]]:
// CIR:           cir.yield
// CIR:         } cleanup all
extern "C" void test_seh_finally_leave() {
  __try {
    might_throw();
    __leave;
    might_throw();
  } __finally {
    take_code(_abnormal_termination());
  }
}

// CIR-LABEL: cir.func internal private @"?filt$0@0@test_seh_filter_dynamic@@"(%arg0: !cir.ptr<!void> {{.*}}, %arg1: !cir.ptr<!void> {{.*}}) -> !s32i
// CIR:         %[[SLOT:.*]] = cir.alloca "__exception_code"
// CIR:         %[[BC_EXN:.*]] = cir.cast bitcast %arg0 : !cir.ptr<!void> -> !cir.ptr<!cir.ptr<!void>>
// CIR:         %[[REC:.*]] = cir.load %[[BC_EXN]] : !cir.ptr<!cir.ptr<!void>>, !cir.ptr<!void>
// CIR:         %[[BC_REC:.*]] = cir.cast bitcast %[[REC]] : !cir.ptr<!void> -> !cir.ptr<!s32i>
// CIR:         %[[CODE:.*]] = cir.load %[[BC_REC]] : !cir.ptr<!s32i>, !s32i
// CIR:         cir.store align(4) %[[CODE]], %[[SLOT]] : !s32i, !cir.ptr<!s32i>
// CIR:         %[[EVAL_CODE:.*]] = cir.load %[[SLOT]] : !cir.ptr<!s32i>, !s32i
// CIR:         %[[CAST_U:.*]] = cir.cast integral %[[EVAL_CODE]] : !s32i -> !u32i
// CIR:         %[[CONST:.*]] = cir.const #cir.int<123> : !u32i
// CIR:         %[[CMP:.*]] = cir.cmp eq %[[CAST_U]], %[[CONST]] : !u32i
// CIR:         %[[RET:.*]] = cir.cast bool_to_int %[[CMP]] : !cir.bool -> !s32i
// CIR:         cir.return %[[RET]] : !s32i

// CIR-LABEL: cir.func no_inline dso_local @test_seh_filter_dynamic() personality(@__C_specific_handler)
// CIR:         %[[EXN_SLOT:.*]] = cir.alloca "__exception_code"
// CIR:         cir.try {
// CIR:           cir.call @might_throw()
// CIR:           cir.yield
// CIR:         } catch all (%[[ARG:.*]]: !cir.eh_token
// CIR:           %[[CT:.*]], %{{.*}} = cir.begin_catch %[[ARG]] type <@"?filt$0@0@test_seh_filter_dynamic@@"> flags 0 : !cir.eh_token -> (!cir.catch_token, !cir.ptr<!void>)
// CIR:           cir.cleanup.scope {
// CIR:             %[[CODE:.*]] = cir.call_llvm_intrinsic "eh.exceptioncode" %[[CT]] : (!cir.catch_token) -> !s32i
// CIR:             cir.store align(4) %[[CODE]], %[[EXN_SLOT]]
// CIR:             cir.call @recover()
// CIR:             cir.yield
// CIR:           } cleanup normal {
// CIR:             cir.end_catch %[[CT]] : !cir.catch_token
// CIR:             cir.yield
// CIR:           }
// CIR:           cir.yield
// CIR:         }

// LLVM-LABEL: define internal i32 @"?filt$0@0@test_seh_filter_dynamic@@"(ptr %0, ptr %1)
// LLVM:         %[[REC:.*]] = load ptr, ptr %0
// LLVM:         %[[CODE:.*]] = load i32, ptr %[[REC]]
// LLVM:         %[[CMP:.*]] = icmp eq i32 %{{.*}}, 123
// LLVM:         %[[ZEXT:.*]] = zext i1 %[[CMP]] to i32
// LLVM:         ret i32 %[[ZEXT]]

// LLVM-LABEL: define dso_local void @test_seh_filter_dynamic() #{{[0-9]+}} personality ptr @__C_specific_handler
// LLVM:         invoke void @might_throw()
// LLVM:                 to label %{{.*}} unwind label %[[CS:.*]]
// LLVM:       [[CS]]:
// LLVM-NEXT:    %[[C_SWITCH:.*]] = catchswitch within none [label %[[CP_BB:.*]]] unwind to caller
// LLVM:       [[CP_BB]]:
// LLVM-NEXT:    %[[CP:.*]] = catchpad within %[[C_SWITCH]] [ptr @"?filt$0@0@test_seh_filter_dynamic@@"]
// LLVM:         %[[EXN_CODE:.*]] = call i32 @llvm.eh.exceptioncode(token %[[CP]])
// LLVM:         store i32 %[[EXN_CODE]], ptr %{{.*}}, align 4
// LLVM:         call void @recover(){{.*}}[ "funclet"(token %[[CP]]) ]
// LLVM:         catchret from %[[CP]] to label %{{.*}}
extern "C" void test_seh_filter_dynamic() {
  __try {
    might_throw();
  } __except (_exception_code() == 123) {
    recover();
  }
}

// CIR-LABEL: cir.func internal private @"?filt$0@0@test_seh_filter_info@@"(%arg0: !cir.ptr<!void> {{.*}}, %arg1: !cir.ptr<!void> {{.*}}) -> !s32i
// CIR:         %[[CALL:.*]] = cir.call @filter_func(%arg0) : (!cir.ptr<!void> {{.*}}) -> !s32i
// CIR:         cir.return %[[CALL]] : !s32i

// CIR-LABEL: cir.func no_inline dso_local @test_seh_filter_info() personality(@__C_specific_handler)
// CIR:         cir.try {
// CIR:           cir.call @might_throw()
// CIR:         } catch all (%[[ARG:.*]]: !cir.eh_token
// CIR:           %[[CT:.*]], %{{.*}} = cir.begin_catch %[[ARG]] type <@"?filt$0@0@test_seh_filter_info@@"> flags 0

// LLVM-LABEL: define internal i32 @"?filt$0@0@test_seh_filter_info@@"(ptr %0, ptr %1)
// LLVM:         %[[CALL:.*]] = call i32 @filter_func(ptr noundef %0)
// LLVM:         ret i32 %[[CALL]]

// LLVM-LABEL: define dso_local void @test_seh_filter_info() #{{[0-9]+}} personality ptr @__C_specific_handler
// LLVM:         catchpad within %{{.*}} [ptr @"?filt$0@0@test_seh_filter_info@@"]
extern "C" void test_seh_filter_info() {
  __try {
    might_throw();
  } __except (filter_func(_exception_info())) {
    recover();
  }
}

// CIR-LABEL: cir.func internal private @"?filt$0@0@test_seh_filter_capture@@"(%arg0: !cir.ptr<!void> {{.*}}, %arg1: !cir.ptr<!void> {{.*}}) -> !s32i
// CIR:         %[[PARENT:.*]] = cir.const #cir.global_view<@test_seh_filter_capture> : !cir.ptr<!void>
// CIR:         %[[IDX:.*]] = cir.const #cir.int<0> : !s32i
// CIR:         %[[RECOV:.*]] = cir.call_llvm_intrinsic "localrecover" %[[PARENT]], %arg1, %[[IDX]] : (!cir.ptr<!void>, !cir.ptr<!void>, !s32i) -> !cir.ptr<!void>
// CIR:         %[[TYPED:.*]] = cir.cast bitcast %[[RECOV]] : !cir.ptr<!void> -> !cir.ptr<!s32i>
// CIR:         %[[LOAD_X:.*]] = cir.load align(4) %[[TYPED]] : !cir.ptr<!s32i>, !s32i
// CIR:         cir.call @filter_with_var({{.*}}, %[[LOAD_X]])

// CIR-LABEL: cir.func no_inline dso_local @test_seh_filter_capture() personality(@__C_specific_handler)
// CIR:         %[[X:.*]] = cir.alloca "x"
// CIR:         cir.call_llvm_intrinsic "localescape" %[[X]] : (!cir.ptr<!s32i>) -> ()
// CIR:         cir.try {
// CIR:           cir.call @might_throw()
// CIR:         } catch all (%[[ARG:.*]]: !cir.eh_token
// CIR:           %[[CT:.*]], %{{.*}} = cir.begin_catch %[[ARG]] type <@"?filt$0@0@test_seh_filter_capture@@"> flags 0

// LLVM-LABEL: define internal i32 @"?filt$0@0@test_seh_filter_capture@@"(ptr %0, ptr %1)
// LLVM:         %[[REC:.*]] = call ptr @llvm.localrecover(ptr @test_seh_filter_capture, ptr %1, i32 0)
// LLVM:         %[[X_VAL:.*]] = load i32, ptr %[[REC]], align 4
// LLVM:         call i32 @filter_with_var(i32 noundef %{{.*}}, i32 noundef %[[X_VAL]])

// LLVM-LABEL: define dso_local void @test_seh_filter_capture() #{{[0-9]+}} personality ptr @__C_specific_handler
// LLVM:         %[[X_SLOT:.*]] = alloca i32, align 4
// LLVM:         call void (...) @llvm.localescape(ptr %[[X_SLOT]])
// LLVM:         catchpad within %{{.*}} [ptr @"?filt$0@0@test_seh_filter_capture@@"]
extern "C" void test_seh_filter_capture() {
  int x = 42;
  __try {
    might_throw();
  } __except (filter_with_var(_exception_code(), x)) {
    recover();
  }
}

