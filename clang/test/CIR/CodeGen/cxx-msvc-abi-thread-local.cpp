// RUN: %clang_cc1 -std=c++20 -triple x86_64-pc-windows-msvc -fdeclspec -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir --check-prefix=CIR %s
// RUN: %clang_cc1 -std=c++20 -triple i686-pc-windows-msvc -fdeclspec -fclangir -emit-cir %s -o %t32.cir
// RUN: FileCheck --input-file=%t32.cir --check-prefix=X86 %s

struct A {
  A();
  ~A();
};

template <typename T>
thread_local A a = A();

thread_local A b;

thread_local A &c = b;
thread_local A &d = c;

thread_local int e = 2;
thread_local constinit int ci = 42;

A f() {
  (void)a<void>;
  (void)b;
  return c;
}

int g() {
  return e + ci;
}

// -----------------------------------------------------------------------------
// Module-level globals and .CRT$XDU function pointer initializers (x86_64)
// -----------------------------------------------------------------------------

// CIR-DAG: cir.global constant internal @__tls_init$initializer$ = #cir.global_view<@__tls_init> : !cir.ptr<!cir.func<()>> {section = ".CRT$XDU"}
// CIR-DAG: cir.global constant internal comdat @"??__E?$a@X@@YAXXZ$initializer$" = #cir.global_view<@"??__E?$a@X@@YAXXZ"> : !cir.ptr<!cir.func<()>> {section = ".CRT$XDU"}
// CIR-DAG: cir.func private @__tlregdtor(!cir.ptr<!cir.func<()>>) -> !s32i

// -----------------------------------------------------------------------------
// Functions and dynamic initialization / destruction (x86_64)
// -----------------------------------------------------------------------------

// CIR: cir.global external tls_model = tls_dyn dso_local @"?b@@3UA@@A" = #cir.zero : !rec_A
// CIR: cir.func internal private @"??__Fb@@YAXXZ"() {
// CIR:   %[[B_ADDR:.*]] = cir.get_global thread_local @"?b@@3UA@@A" : !cir.ptr<!rec_A>
// CIR:   cir.call @"??1A@@QEAA@XZ"(%[[B_ADDR]]) : (!cir.ptr<!rec_A>) -> ()
// CIR:   cir.return
// CIR: }

// CIR: cir.func internal private @"??__Eb@@YAXXZ"() {
// CIR:   %[[B_ADDR:.*]] = cir.get_global thread_local @"?b@@3UA@@A" : !cir.ptr<!rec_A>
// CIR:   cir.call @"??0A@@QEAA@XZ"(%[[B_ADDR]])
// CIR:   %[[DTOR_STUB:.*]] = cir.get_global @"??__Fb@@YAXXZ" : !cir.ptr<!cir.func<()>>
// CIR:   cir.call @__tlregdtor(%[[DTOR_STUB]]) nothrow : (!cir.ptr<!cir.func<()>>) -> !s32i
// CIR:   cir.return
// CIR: }

// CIR: cir.global external tls_model = tls_dyn dso_local @"?c@@3AEAUA@@EA" = #cir.ptr<null> : !cir.ptr<!rec_A>
// CIR: cir.func internal private @"??__Ec@@YAXXZ"() {
// CIR:   %[[C_ADDR:.*]] = cir.get_global thread_local @"?c@@3AEAUA@@EA" : !cir.ptr<!cir.ptr<!rec_A>>
// CIR:   %[[B_ADDR:.*]] = cir.get_global thread_local @"?b@@3UA@@A" : !cir.ptr<!rec_A>
// CIR:   cir.store align(8) %[[B_ADDR]], %[[C_ADDR]] : !cir.ptr<!rec_A>, !cir.ptr<!cir.ptr<!rec_A>>
// CIR:   cir.return
// CIR: }

// CIR: cir.global external tls_model = tls_dyn dso_local @"?d@@3AEAUA@@EA" = #cir.ptr<null> : !cir.ptr<!rec_A>
// CIR: cir.func internal private @"??__Ed@@YAXXZ"() {
// CIR:   %[[D_ADDR:.*]] = cir.get_global thread_local @"?d@@3AEAUA@@EA" : !cir.ptr<!cir.ptr<!rec_A>>
// CIR:   %[[C_ADDR:.*]] = cir.get_global thread_local @"?c@@3AEAUA@@EA" : !cir.ptr<!cir.ptr<!rec_A>>
// CIR:   %[[C_VAL:.*]] = cir.load %[[C_ADDR]] : !cir.ptr<!cir.ptr<!rec_A>>, !cir.ptr<!rec_A>
// CIR:   cir.store align(8) %[[C_VAL]], %[[D_ADDR]] : !cir.ptr<!rec_A>, !cir.ptr<!cir.ptr<!rec_A>>
// CIR:   cir.return
// CIR: }

// CIR: cir.global external tls_model = tls_dyn dso_local @"?e@@3HA" = #cir.int<2> : !s32i
// CIR: cir.global external tls_model = tls_dyn dso_local @"?ci@@3HA" = #cir.int<42> : !s32i

// CIR: cir.global linkonce_odr comdat tls_model = tls_dyn dso_local @"??$a@X@@3UA@@A" = #cir.zero : !rec_A
// CIR: cir.func internal private @"??__F?$a@X@@YAXXZ"() {
// CIR:   %[[A_ADDR:.*]] = cir.get_global thread_local @"??$a@X@@3UA@@A" : !cir.ptr<!rec_A>
// CIR:   cir.call @"??1A@@QEAA@XZ"(%[[A_ADDR]]) : (!cir.ptr<!rec_A>) -> ()
// CIR:   cir.return
// CIR: }

// CIR: cir.func comdat linkonce_odr private @"??__E?$a@X@@YAXXZ"() {
// CIR:   %[[A_ADDR:.*]] = cir.get_global thread_local @"??$a@X@@3UA@@A" : !cir.ptr<!rec_A>
// CIR:   cir.call @"??0A@@QEAA@XZ"(%[[A_ADDR]])
// CIR:   %[[DTOR_STUB:.*]] = cir.get_global @"??__F?$a@X@@YAXXZ" : !cir.ptr<!cir.func<()>>
// CIR:   cir.call @__tlregdtor(%[[DTOR_STUB]]) nothrow : (!cir.ptr<!cir.func<()>>) -> !s32i
// CIR:   cir.return
// CIR: }

// CIR: cir.func no_inline dso_local @"?f@@YA?AUA@@XZ"(%{{.*}}: !cir.ptr<!rec_A>{{.*}})
// CIR:   cir.get_global thread_local @"??$a@X@@3UA@@A"
// CIR:   cir.get_global thread_local @"?b@@3UA@@A"
// CIR:   cir.get_global thread_local @"?c@@3AEAUA@@EA"

// CIR: cir.func no_inline dso_local @"?g@@YAHXZ"()
// CIR:   cir.get_global thread_local @"?e@@3HA"
// CIR:   cir.get_global thread_local @"?ci@@3HA"

// CIR: cir.global appending @llvm.used = #cir.const_array<[#cir.global_view<@"??__E?$a@X@@YAXXZ$initializer$"> : !cir.ptr<!void>, #cir.global_view<@__tls_init$initializer$> : !cir.ptr<!void>]> : !cir.array<!cir.ptr<!void> x 2> {section = "llvm.metadata"}

// CIR: cir.func internal private @__tls_init() {
// CIR:   cir.call @"??__Eb@@YAXXZ"() : () -> ()
// CIR:   cir.call @"??__Ec@@YAXXZ"() : () -> ()
// CIR:   cir.call @"??__Ed@@YAXXZ"() : () -> ()
// CIR:   cir.return
// CIR: }

// -----------------------------------------------------------------------------
// Module-level globals and .CRT$XDU function pointer initializers (i686)
// -----------------------------------------------------------------------------

// X86-DAG: cir.global constant internal @__tls_init$initializer$ = #cir.global_view<@__tls_init> : !cir.ptr<!cir.func<()>> {section = ".CRT$XDU"}
// X86-DAG: cir.global constant internal comdat @"??__E?$a@X@@YAXXZ$initializer$" = #cir.global_view<@"??__E?$a@X@@YAXXZ"> : !cir.ptr<!cir.func<()>> {section = ".CRT$XDU"}
// X86-DAG: cir.func private @__tlregdtor(!cir.ptr<!cir.func<()>>) -> !s32i

// -----------------------------------------------------------------------------
// Functions and dynamic initialization / destruction (i686)
// -----------------------------------------------------------------------------

// X86: cir.global external tls_model = tls_dyn dso_local @"?b@@3UA@@A" = #cir.zero : !rec_A
// X86: cir.func internal private @"??__Fb@@YAXXZ"() {
// X86:   %[[B_ADDR:.*]] = cir.get_global thread_local @"?b@@3UA@@A" : !cir.ptr<!rec_A>
// X86:   cir.call @"??1A@@QAE@XZ"(%[[B_ADDR]]) cc(x86_thiscall) : (!cir.ptr<!rec_A>) -> ()
// X86:   cir.return
// X86: }

// X86: cir.func internal private @"??__Eb@@YAXXZ"() {
// X86:   %[[B_ADDR:.*]] = cir.get_global thread_local @"?b@@3UA@@A" : !cir.ptr<!rec_A>
// X86:   cir.call @"??0A@@QAE@XZ"(%[[B_ADDR]]) cc(x86_thiscall)
// X86:   %[[DTOR_STUB:.*]] = cir.get_global @"??__Fb@@YAXXZ" : !cir.ptr<!cir.func<()>>
// X86:   cir.call @__tlregdtor(%[[DTOR_STUB]]) nothrow : (!cir.ptr<!cir.func<()>>) -> !s32i
// X86:   cir.return
// X86: }

// X86: cir.global external tls_model = tls_dyn dso_local @"?c@@3AAUA@@A" = #cir.ptr<null> : !cir.ptr<!rec_A>
// X86: cir.func internal private @"??__Ec@@YAXXZ"() {
// X86:   %[[C_ADDR:.*]] = cir.get_global thread_local @"?c@@3AAUA@@A" : !cir.ptr<!cir.ptr<!rec_A>>
// X86:   %[[B_ADDR:.*]] = cir.get_global thread_local @"?b@@3UA@@A" : !cir.ptr<!rec_A>
// X86:   cir.store align(4) %[[B_ADDR]], %[[C_ADDR]] : !cir.ptr<!rec_A>, !cir.ptr<!cir.ptr<!rec_A>>
// X86:   cir.return
// X86: }

// X86: cir.global external tls_model = tls_dyn dso_local @"?d@@3AAUA@@A" = #cir.ptr<null> : !cir.ptr<!rec_A>
// X86: cir.func internal private @"??__Ed@@YAXXZ"() {
// X86:   %[[D_ADDR:.*]] = cir.get_global thread_local @"?d@@3AAUA@@A" : !cir.ptr<!cir.ptr<!rec_A>>
// X86:   %[[C_ADDR:.*]] = cir.get_global thread_local @"?c@@3AAUA@@A" : !cir.ptr<!cir.ptr<!rec_A>>
// X86:   %[[C_VAL:.*]] = cir.load %[[C_ADDR]] : !cir.ptr<!cir.ptr<!rec_A>>, !cir.ptr<!rec_A>
// X86:   cir.store align(4) %[[C_VAL]], %[[D_ADDR]] : !cir.ptr<!rec_A>, !cir.ptr<!cir.ptr<!rec_A>>
// X86:   cir.return
// X86: }

// X86: cir.global external tls_model = tls_dyn dso_local @"?e@@3HA" = #cir.int<2> : !s32i
// X86: cir.global external tls_model = tls_dyn dso_local @"?ci@@3HA" = #cir.int<42> : !s32i

// X86: cir.global linkonce_odr comdat tls_model = tls_dyn dso_local @"??$a@X@@3UA@@A" = #cir.zero : !rec_A
// X86: cir.func internal private @"??__F?$a@X@@YAXXZ"() {
// X86:   %[[A_ADDR:.*]] = cir.get_global thread_local @"??$a@X@@3UA@@A" : !cir.ptr<!rec_A>
// X86:   cir.call @"??1A@@QAE@XZ"(%[[A_ADDR]]) cc(x86_thiscall) : (!cir.ptr<!rec_A>) -> ()
// X86:   cir.return
// X86: }

// X86: cir.func comdat linkonce_odr private @"??__E?$a@X@@YAXXZ"() {
// X86:   %[[A_ADDR:.*]] = cir.get_global thread_local @"??$a@X@@3UA@@A" : !cir.ptr<!rec_A>
// X86:   cir.call @"??0A@@QAE@XZ"(%[[A_ADDR]]) cc(x86_thiscall)
// X86:   %[[DTOR_STUB:.*]] = cir.get_global @"??__F?$a@X@@YAXXZ" : !cir.ptr<!cir.func<()>>
// X86:   cir.call @__tlregdtor(%[[DTOR_STUB]]) nothrow : (!cir.ptr<!cir.func<()>>) -> !s32i
// X86:   cir.return
// X86: }

// X86: cir.func no_inline dso_local @"?f@@YA?AUA@@XZ"(%{{.*}}: !cir.ptr<!rec_A>{{.*}})
// X86:   cir.get_global thread_local @"??$a@X@@3UA@@A"
// X86:   cir.get_global thread_local @"?b@@3UA@@A"
// X86:   cir.get_global thread_local @"?c@@3AAUA@@A"

// X86: cir.func no_inline dso_local @"?g@@YAHXZ"()
// X86:   cir.get_global thread_local @"?e@@3HA"
// X86:   cir.get_global thread_local @"?ci@@3HA"

// X86: cir.global appending @llvm.used = #cir.const_array<[#cir.global_view<@"??__E?$a@X@@YAXXZ$initializer$"> : !cir.ptr<!void>, #cir.global_view<@__tls_init$initializer$> : !cir.ptr<!void>]> : !cir.array<!cir.ptr<!void> x 2> {section = "llvm.metadata"}

// X86: cir.func internal private @__tls_init() {
// X86:   cir.call @"??__Eb@@YAXXZ"() : () -> ()
// X86:   cir.call @"??__Ec@@YAXXZ"() : () -> ()
// X86:   cir.call @"??__Ed@@YAXXZ"() : () -> ()
// X86:   cir.return
// X86: }
