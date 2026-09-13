// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fdeclspec -fclangir -emit-cir -fexceptions -fcxx-exceptions %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir --check-prefix=CIR %s
// RUN: %clang_cc1 -std=c++17 -triple i686-pc-windows-msvc -fdeclspec -fclangir -emit-cir -fexceptions -fcxx-exceptions %s -o %t32.cir
// RUN: FileCheck --input-file=%t32.cir --check-prefix=X86 %s

struct S {
  S();
  ~S();
};

// -----------------------------------------------------------------------------
// Module-level globals and declarations (x86_64)
// -----------------------------------------------------------------------------

// CIR-DAG: cir.global "private" external tls_model = tls_dyn @_Init_thread_epoch : !s32i {alignment = 4 : i64}
// CIR-DAG: cir.global "private" linkonce_odr comdat dso_local @"?$TSS0@?1??g@@YAAEAUS@@XZ@4HA" = #cir.int<0> : !s32i {alignment = 4 : i64}
// CIR-DAG: cir.global "private" linkonce_odr comdat tls_model = tls_dyn dso_local @"??__J?1??f@@YAAEAUS@@XZ@51" = #cir.int<0> : !s32i {alignment = 4 : i64}
// CIR-DAG: cir.global "private" internal dso_local @"?$TSS0@?1??g1@@YAHXZ@4HA" = #cir.int<0> : !s32i {alignment = 4 : i64}
// CIR-DAG: cir.global "private" linkonce_odr comdat dso_local @"?$TSS0@?1??h@@YAAEAUS@@_N@Z@4HA" = #cir.int<0> : !s32i {alignment = 4 : i64}
// CIR-DAG: cir.global "private" linkonce_odr comdat tls_model = tls_dyn dso_local @"??__J?1??h@@YAAEAUS@@_N@Z@51" = #cir.int<0> : !s32i {alignment = 4 : i64}
// CIR-DAG: cir.func private @_Init_thread_header(!cir.ptr<!s32i>)
// CIR-DAG: cir.func private @_Init_thread_footer(!cir.ptr<!s32i>)
// CIR-DAG: cir.func private @_Init_thread_abort(!cir.ptr<!s32i>)
// CIR-DAG: cir.func private @atexit(!cir.ptr<!cir.func<()>>) -> !s32i
// CIR-DAG: cir.func private @__tlregdtor(!cir.ptr<!cir.func<()>>) -> !s32i

// -----------------------------------------------------------------------------
// Module-level globals and declarations (i686)
// -----------------------------------------------------------------------------

// X86-DAG: cir.global "private" external tls_model = tls_dyn @_Init_thread_epoch : !s32i {alignment = 4 : i64}
// X86-DAG: cir.global "private" linkonce_odr comdat dso_local @"?$TSS0@?1??g@@YAAAUS@@XZ@4HA" = #cir.int<0> : !s32i {alignment = 4 : i64}
// X86-DAG: cir.global "private" linkonce_odr comdat tls_model = tls_dyn dso_local @"??__J?1??f@@YAAAUS@@XZ@51" = #cir.int<0> : !s32i {alignment = 4 : i64}
// X86-DAG: cir.global "private" internal dso_local @"?$TSS0@?1??g1@@YAHXZ@4HA" = #cir.int<0> : !s32i {alignment = 4 : i64}
// X86-DAG: cir.global "private" linkonce_odr comdat dso_local @"?$TSS0@?1??h@@YAAAUS@@_N@Z@4HA" = #cir.int<0> : !s32i {alignment = 4 : i64}
// X86-DAG: cir.global "private" linkonce_odr comdat tls_model = tls_dyn dso_local @"??__J?1??h@@YAAAUS@@_N@Z@51" = #cir.int<0> : !s32i {alignment = 4 : i64}
// X86-DAG: cir.func private @_Init_thread_header(!cir.ptr<!s32i>)
// X86-DAG: cir.func private @_Init_thread_footer(!cir.ptr<!s32i>)
// X86-DAG: cir.func private @_Init_thread_abort(!cir.ptr<!s32i>)
// X86-DAG: cir.func private @atexit(!cir.ptr<!cir.func<()>>) -> !s32i
// X86-DAG: cir.func private @__tlregdtor(!cir.ptr<!cir.func<()>>) -> !s32i

// -----------------------------------------------------------------------------
// Functions (x86_64)
// -----------------------------------------------------------------------------

// CIR-LABEL: cir.func no_inline comdat weak_odr dso_local @"?f@@YAAEAUS@@XZ"()
// CIR:   %[[GUARD_PTR:.*]] = cir.get_global thread_local @"??__J?1??f@@YAAEAUS@@XZ@51" : !cir.ptr<!s32i>
// CIR:   %[[GUARD:.*]] = cir.load align(4) %[[GUARD_PTR]] : !cir.ptr<!s32i>, !s32i
// CIR:   %[[ONE:.*]] = cir.const #cir.int<1> : !s32i
// CIR:   %[[ZERO:.*]] = cir.const #cir.int<0> : !s32i
// CIR:   %[[MASK:.*]] = cir.and %[[GUARD]], %[[ONE]] : !s32i
// CIR:   %[[CMP:.*]] = cir.cmp eq %[[MASK]], %[[ZERO]] : !s32i
// CIR:   cir.if %[[CMP]] {
// CIR:     %[[OR:.*]] = cir.or %[[GUARD]], %[[ONE]] : !s32i
// CIR:     cir.store %[[OR]], %[[GUARD_PTR]] : !s32i, !cir.ptr<!s32i>
// CIR:     cir.cleanup.scope {
// CIR:       cir.call @"??0S@@QEAA@XZ"
// CIR:       %[[DTOR:.*]] = cir.get_global @"??__Fs@?1??f@@YAAEAUS@@XZ@YAXXZ" : !cir.ptr<!cir.func<()>>
// CIR:       cir.call @__tlregdtor(%[[DTOR]]) nothrow : (!cir.ptr<!cir.func<()>>) -> !s32i
// CIR:       cir.yield
// CIR:     } cleanup eh {
// CIR:       %[[CUR_GUARD:.*]] = cir.load align(4) %[[GUARD_PTR]] : !cir.ptr<!s32i>, !s32i
// CIR:       %[[NOT_ONE:.*]] = cir.const #cir.int<-2> : !s32i
// CIR:       %[[RESET:.*]] = cir.and %[[CUR_GUARD]], %[[NOT_ONE]] : !s32i
// CIR:       cir.store %[[RESET]], %[[GUARD_PTR]] : !s32i, !cir.ptr<!s32i>
// CIR:       cir.yield
// CIR:     }
// CIR:   }
extern inline S &f() {
  static thread_local S s;
  return s;
}

// CIR-LABEL: cir.func no_inline comdat weak_odr dso_local @"?g@@YAAEAUS@@XZ"()
// CIR:   %[[GUARD_PTR:.*]] = cir.get_global @"?$TSS0@?1??g@@YAAEAUS@@XZ@4HA" : !cir.ptr<!s32i>
// CIR:   %[[EPOCH_PTR:.*]] = cir.get_global thread_local @_Init_thread_epoch : !cir.ptr<!s32i>
// CIR:   %[[GUARD1:.*]] = cir.load align(4) atomic(relaxed) %[[GUARD_PTR]] : !cir.ptr<!s32i>, !s32i
// CIR:   %[[EPOCH:.*]] = cir.load align(4) %[[EPOCH_PTR]] : !cir.ptr<!s32i>, !s32i
// CIR:   %[[UNINIT:.*]] = cir.cmp gt %[[GUARD1]], %[[EPOCH]] : !s32i
// CIR:   cir.if %[[UNINIT]] {
// CIR:     cir.call @_Init_thread_header(%[[GUARD_PTR]]) nothrow : (!cir.ptr<!s32i>) -> ()
// CIR:     %[[GUARD2:.*]] = cir.load align(4) atomic(relaxed) %[[GUARD_PTR]] : !cir.ptr<!s32i>, !s32i
// CIR:     %[[NEG_ONE:.*]] = cir.const #cir.int<-1> : !s32i
// CIR:     %[[SHOULD_INIT:.*]] = cir.cmp eq %[[GUARD2]], %[[NEG_ONE]] : !s32i
// CIR:     cir.if %[[SHOULD_INIT]] {
// CIR:       cir.cleanup.scope {
// CIR:         cir.call @"??0S@@QEAA@XZ"
// CIR:         %[[DTOR:.*]] = cir.get_global @"??__Fs@?1??g@@YAAEAUS@@XZ@YAXXZ" : !cir.ptr<!cir.func<()>>
// CIR:         cir.call @atexit(%[[DTOR]]) nothrow : (!cir.ptr<!cir.func<()>>) -> !s32i
// CIR:         cir.yield
// CIR:       } cleanup eh {
// CIR:         cir.call @_Init_thread_abort(%[[GUARD_PTR]]) nothrow : (!cir.ptr<!s32i>) -> ()
// CIR:         cir.yield
// CIR:       }
// CIR:       cir.call @_Init_thread_footer(%[[GUARD_PTR]]) nothrow : (!cir.ptr<!s32i>) -> ()
// CIR:     }
// CIR:   }
extern inline S &g() {
  static S s;
  return s;
}

// CIR-LABEL: cir.func no_inline comdat weak_odr dso_local @"?h@@YAAEAUS@@_N@Z"(%arg0: !cir.bool
// CIR:   cir.get_global thread_local @"??__J?1??h@@YAAEAUS@@_N@Z@51"
// CIR:   cir.call @__tlregdtor
// CIR:   cir.get_global @"?$TSS0@?1??h@@YAAEAUS@@_N@Z@4HA"
// CIR:   cir.call @_Init_thread_header
// CIR:   cir.call @atexit
// CIR:   cir.call @_Init_thread_footer
extern inline S &h(bool b) {
  static thread_local S j;
  static S i;
  return b ? j : i;
}

// CIR-LABEL: cir.func no_inline dso_local @"?g1@@YAHXZ"()
// CIR:   %[[GUARD_PTR:.*]] = cir.get_global @"?$TSS0@?1??g1@@YAHXZ@4HA" : !cir.ptr<!s32i>
// CIR:   %[[EPOCH_PTR:.*]] = cir.get_global thread_local @_Init_thread_epoch : !cir.ptr<!s32i>
// CIR:   %[[GUARD1:.*]] = cir.load align(4) atomic(relaxed) %[[GUARD_PTR]] : !cir.ptr<!s32i>, !s32i
// CIR:   %[[EPOCH:.*]] = cir.load align(4) %[[EPOCH_PTR]] : !cir.ptr<!s32i>, !s32i
// CIR:   %[[UNINIT:.*]] = cir.cmp gt %[[GUARD1]], %[[EPOCH]] : !s32i
// CIR:   cir.if %[[UNINIT]] {
// CIR:     cir.call @_Init_thread_header(%[[GUARD_PTR]]) nothrow : (!cir.ptr<!s32i>) -> ()
// CIR:     %[[GUARD2:.*]] = cir.load align(4) atomic(relaxed) %[[GUARD_PTR]] : !cir.ptr<!s32i>, !s32i
// CIR:     %[[NEG_ONE:.*]] = cir.const #cir.int<-1> : !s32i
// CIR:     %[[SHOULD_INIT:.*]] = cir.cmp eq %[[GUARD2]], %[[NEG_ONE]] : !s32i
// CIR:     cir.if %[[SHOULD_INIT]] {
// CIR:       cir.cleanup.scope {
// CIR:         %[[VAL:.*]] = cir.call @"?f1@@YAHXZ"() : () -> (!s32i {llvm.noundef})
// CIR:         cir.store align(4) %[[VAL]], {{.*}} : !s32i, !cir.ptr<!s32i>
// CIR:         cir.yield
// CIR:       } cleanup eh {
// CIR:         cir.call @_Init_thread_abort(%[[GUARD_PTR]]) nothrow : (!cir.ptr<!s32i>) -> ()
// CIR:         cir.yield
// CIR:       }
// CIR:       cir.call @_Init_thread_footer(%[[GUARD_PTR]]) nothrow : (!cir.ptr<!s32i>) -> ()
// CIR:     }
// CIR:   }
int f1();
int g1() {
  static int i = f1();
  return i;
}

// -----------------------------------------------------------------------------
// Functions (i686)
// -----------------------------------------------------------------------------

// X86-LABEL: cir.func no_inline comdat weak_odr dso_local @"?f@@YAAAUS@@XZ"()
// X86:   %[[GUARD_PTR:.*]] = cir.get_global thread_local @"??__J?1??f@@YAAAUS@@XZ@51" : !cir.ptr<!s32i>
// X86:   %[[GUARD:.*]] = cir.load align(4) %[[GUARD_PTR]] : !cir.ptr<!s32i>, !s32i
// X86:   %[[ONE:.*]] = cir.const #cir.int<1> : !s32i
// X86:   %[[ZERO:.*]] = cir.const #cir.int<0> : !s32i
// X86:   %[[MASK:.*]] = cir.and %[[GUARD]], %[[ONE]] : !s32i
// X86:   %[[CMP:.*]] = cir.cmp eq %[[MASK]], %[[ZERO]] : !s32i
// X86:   cir.if %[[CMP]] {
// X86:     %[[OR:.*]] = cir.or %[[GUARD]], %[[ONE]] : !s32i
// X86:     cir.store %[[OR]], %[[GUARD_PTR]] : !s32i, !cir.ptr<!s32i>
// X86:     cir.cleanup.scope {
// X86:       cir.call @"??0S@@QAE@XZ"
// X86:       %[[DTOR:.*]] = cir.get_global @"??__Fs@?1??f@@YAAAUS@@XZ@YAXXZ" : !cir.ptr<!cir.func<()>>
// X86:       cir.call @__tlregdtor(%[[DTOR]]) nothrow : (!cir.ptr<!cir.func<()>>) -> !s32i
// X86:       cir.yield
// X86:     } cleanup eh {
// X86:       %[[CUR_GUARD:.*]] = cir.load align(4) %[[GUARD_PTR]] : !cir.ptr<!s32i>, !s32i
// X86:       %[[NOT_ONE:.*]] = cir.const #cir.int<-2> : !s32i
// X86:       %[[RESET:.*]] = cir.and %[[CUR_GUARD]], %[[NOT_ONE]] : !s32i
// X86:       cir.store %[[RESET]], %[[GUARD_PTR]] : !s32i, !cir.ptr<!s32i>
// X86:       cir.yield
// X86:     }
// X86:   }

// X86-LABEL: cir.func no_inline comdat weak_odr dso_local @"?g@@YAAAUS@@XZ"()
// X86:   %[[GUARD_PTR:.*]] = cir.get_global @"?$TSS0@?1??g@@YAAAUS@@XZ@4HA" : !cir.ptr<!s32i>
// X86:   %[[EPOCH_PTR:.*]] = cir.get_global thread_local @_Init_thread_epoch : !cir.ptr<!s32i>
// X86:   %[[GUARD1:.*]] = cir.load align(4) atomic(relaxed) %[[GUARD_PTR]] : !cir.ptr<!s32i>, !s32i
// X86:   %[[EPOCH:.*]] = cir.load align(4) %[[EPOCH_PTR]] : !cir.ptr<!s32i>, !s32i
// X86:   %[[UNINIT:.*]] = cir.cmp gt %[[GUARD1]], %[[EPOCH]] : !s32i
// X86:   cir.if %[[UNINIT]] {
// X86:     cir.call @_Init_thread_header(%[[GUARD_PTR]]) nothrow : (!cir.ptr<!s32i>) -> ()
// X86:     %[[GUARD2:.*]] = cir.load align(4) atomic(relaxed) %[[GUARD_PTR]] : !cir.ptr<!s32i>, !s32i
// X86:     %[[NEG_ONE:.*]] = cir.const #cir.int<-1> : !s32i
// X86:     %[[SHOULD_INIT:.*]] = cir.cmp eq %[[GUARD2]], %[[NEG_ONE]] : !s32i
// X86:     cir.if %[[SHOULD_INIT]] {
// X86:       cir.cleanup.scope {
// X86:         cir.call @"??0S@@QAE@XZ"
// X86:         %[[DTOR:.*]] = cir.get_global @"??__Fs@?1??g@@YAAAUS@@XZ@YAXXZ" : !cir.ptr<!cir.func<()>>
// X86:         cir.call @atexit(%[[DTOR]]) nothrow : (!cir.ptr<!cir.func<()>>) -> !s32i
// X86:         cir.yield
// X86:       } cleanup eh {
// X86:         cir.call @_Init_thread_abort(%[[GUARD_PTR]]) nothrow : (!cir.ptr<!s32i>) -> ()
// X86:         cir.yield
// X86:       }
// X86:       cir.call @_Init_thread_footer(%[[GUARD_PTR]]) nothrow : (!cir.ptr<!s32i>) -> ()
// X86:     }
// X86:   }
