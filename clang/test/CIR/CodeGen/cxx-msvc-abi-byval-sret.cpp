// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fclangir -emit-cir %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir %s -check-prefix=X64
// RUN: %clang_cc1 -std=c++17 -triple i686-pc-windows-msvc -fclangir -emit-cir %s -o %t32.cir
// RUN: FileCheck --input-file=%t32.cir %s -check-prefix=X86

struct A {
  int x;
  A();
  A(const A &);
  ~A();
};

struct B {
  int y;
  B();
  B(const B &);
  ~B();
};

// ----------------------------------------------------------------------------
// a) Single callee-destructed struct parameter
// ----------------------------------------------------------------------------

// X64-LABEL: cir.func no_inline dso_local @"?foo@@YAXUA@@@Z"(%arg0: !rec_A
// X64:         %[[A_ADDR:.*]] = cir.alloca "a" align(4) init : !cir.ptr<!rec_A>
// X64:         cir.store %arg0, %[[A_ADDR]] : !rec_A, !cir.ptr<!rec_A>
// X64:         cir.cleanup.scope {
// X64:           cir.yield
// X64:         } cleanup normal {
// X64:           cir.call @"??1A@@QEAA@XZ"(%[[A_ADDR]]) nothrow : (!cir.ptr<!rec_A>
// X64:           cir.yield
// X64:         }
// X64:         cir.return

// X86-LABEL: cir.func no_inline dso_local @"?foo@@YAXUA@@@Z"(%arg0: !rec_A
// X86:         %[[A_ADDR:.*]] = cir.alloca "a" align(4) init : !cir.ptr<!rec_A>
// X86:         cir.store %arg0, %[[A_ADDR]] : !rec_A, !cir.ptr<!rec_A>
// X86:         cir.cleanup.scope {
// X86:           cir.yield
// X86:         } cleanup normal {
// X86:           cir.call @"??1A@@QAE@XZ"(%[[A_ADDR]]) nothrow cc(x86_thiscall) : (!cir.ptr<!rec_A>
// X86:           cir.yield
// X86:         }
// X86:         cir.return
void foo(A a) {}

// Verify that caller copies argument into temporary, passes to callee, and does NOT
// destroy the argument temporary (callee destroys it). Only local variable 'a' is destroyed.
// X64-LABEL: cir.func no_inline dso_local @"?test_caller_foo@@YAXXZ"()
// X64:         %[[LOCAL_A:.*]] = cir.alloca "a" align(4) init : !cir.ptr<!rec_A>
// X64:         %[[TMP_A:.*]] = cir.alloca "agg.tmp0" align(4) : !cir.ptr<!rec_A>
// X64:         cir.call @"??0A@@QEAA@XZ"(%[[LOCAL_A]])
// X64:         cir.cleanup.scope {
// X64:           cir.call @"??0A@@QEAA@AEBU0@@Z"(%[[TMP_A]], %[[LOCAL_A]])
// X64:           %[[VAL_A:.*]] = cir.load align(4) %[[TMP_A]] : !cir.ptr<!rec_A>, !rec_A
// X64:           cir.call @"?foo@@YAXUA@@@Z"(%[[VAL_A]]) : (!rec_A) -> ()
// X64:           cir.yield
// X64:         } cleanup normal {
// X64-NOT:       cir.call @"??1A@@QEAA@XZ"(%[[TMP_A]])
// X64:           cir.call @"??1A@@QEAA@XZ"(%[[LOCAL_A]])
// X64:           cir.yield
// X64:         }

// X86-LABEL: cir.func no_inline dso_local @"?test_caller_foo@@YAXXZ"()
// X86:         %[[LOCAL_A:.*]] = cir.alloca "a" align(4) init : !cir.ptr<!rec_A>
// X86:         %[[TMP_A:.*]] = cir.alloca "agg.tmp0" align(4) : !cir.ptr<!rec_A>
// X86:         cir.call @"??0A@@QAE@XZ"(%[[LOCAL_A]]) cc(x86_thiscall)
// X86:         cir.cleanup.scope {
// X86:           cir.call @"??0A@@QAE@ABU0@@Z"(%[[TMP_A]], %[[LOCAL_A]]) cc(x86_thiscall)
// X86:           %[[VAL_A:.*]] = cir.load align(4) %[[TMP_A]] : !cir.ptr<!rec_A>, !rec_A
// X86:           cir.call @"?foo@@YAXUA@@@Z"(%[[VAL_A]]) : (!rec_A) -> ()
// X86:           cir.yield
// X86:         } cleanup normal {
// X86-NOT:       cir.call @"??1A@@QAE@XZ"(%[[TMP_A]])
// X86:           cir.call @"??1A@@QAE@XZ"(%[[LOCAL_A]])
// X86:           cir.yield
// X86:         }
void test_caller_foo() {
  A a;
  foo(a);
}

// ----------------------------------------------------------------------------
// b) Multiple callee-destructed struct parameters: left-to-right cleanup order
// ----------------------------------------------------------------------------

// Callee must destroy 'a' before 'b' (left-to-right order).
// In LIFO cleanups, inner scope is 'a' and outer scope is 'b'.
// X64-LABEL: cir.func no_inline dso_local @"?bar@@YAXUA@@UB@@@Z"(%arg0: !rec_A {{.*}}, %arg1: !rec_B {{.*}})
// X64:         %[[A_ADDR:.*]] = cir.alloca "a" align(4) init : !cir.ptr<!rec_A>
// X64:         %[[B_ADDR:.*]] = cir.alloca "b" align(4) init : !cir.ptr<!rec_B>
// X64:         cir.store %arg0, %[[A_ADDR]]
// X64:         cir.store %arg1, %[[B_ADDR]]
// X64:         cir.cleanup.scope {
// X64:           cir.cleanup.scope {
// X64:             cir.yield
// X64:           } cleanup normal {
// X64:             cir.call @"??1A@@QEAA@XZ"(%[[A_ADDR]]) nothrow
// X64:             cir.yield
// X64:           }
// X64:           cir.yield
// X64:         } cleanup normal {
// X64:           cir.call @"??1B@@QEAA@XZ"(%[[B_ADDR]]) nothrow
// X64:           cir.yield
// X64:         }
// X64:         cir.return

// X86-LABEL: cir.func no_inline dso_local @"?bar@@YAXUA@@UB@@@Z"(%arg0: !rec_A {{.*}}, %arg1: !rec_B {{.*}})
// X86:         %[[A_ADDR:.*]] = cir.alloca "a" align(4) init : !cir.ptr<!rec_A>
// X86:         %[[B_ADDR:.*]] = cir.alloca "b" align(4) init : !cir.ptr<!rec_B>
// X86:         cir.store %arg0, %[[A_ADDR]]
// X86:         cir.store %arg1, %[[B_ADDR]]
// X86:         cir.cleanup.scope {
// X86:           cir.cleanup.scope {
// X86:             cir.yield
// X86:           } cleanup normal {
// X86:             cir.call @"??1A@@QAE@XZ"(%[[A_ADDR]]) nothrow cc(x86_thiscall)
// X86:             cir.yield
// X86:           }
// X86:           cir.yield
// X86:         } cleanup normal {
// X86:           cir.call @"??1B@@QAE@XZ"(%[[B_ADDR]]) nothrow cc(x86_thiscall)
// X86:           cir.yield
// X86:         }
// X86:         cir.return
void bar(A a, B b) {}

// Caller evaluates arguments right-to-left: 'b' is constructed before 'a'.
// X64-LABEL: cir.func no_inline dso_local @"?test_caller_bar@@YAXXZ"()
// X64:         %[[TMP_B:.*]] = cir.alloca "agg.tmp0" align(4) : !cir.ptr<!rec_B>
// X64:         %[[TMP_A:.*]] = cir.alloca "agg.tmp1" align(4) : !cir.ptr<!rec_A>
// X64:         cir.cleanup.scope {
// X64:           cir.cleanup.scope {
// X64:             cir.call @"??0B@@QEAA@AEBU0@@Z"(%[[TMP_B]],
// X64:             cir.call @"??0A@@QEAA@AEBU0@@Z"(%[[TMP_A]],
// X64:             %[[VAL_A:.*]] = cir.load align(4) %[[TMP_A]]
// X64:             %[[VAL_B:.*]] = cir.load align(4) %[[TMP_B]]
// X64:             cir.call @"?bar@@YAXUA@@UB@@@Z"(%[[VAL_A]], %[[VAL_B]])
// X64:             cir.yield
// X64:           } cleanup normal {
// X64:             cir.call @"??1B@@QEAA@XZ"
// X64:             cir.yield
// X64:         }
// X64:         cir.return

// X86-LABEL: cir.func no_inline dso_local @"?test_caller_bar@@YAXXZ"()
// X86:         %[[TMP_B:.*]] = cir.alloca "agg.tmp0" align(4) : !cir.ptr<!rec_B>
// X86:         %[[TMP_A:.*]] = cir.alloca "agg.tmp1" align(4) : !cir.ptr<!rec_A>
// X86:         cir.cleanup.scope {
// X86:           cir.cleanup.scope {
// X86:             cir.call @"??0B@@QAE@ABU0@@Z"(%[[TMP_B]],
// X86:             cir.call @"??0A@@QAE@ABU0@@Z"(%[[TMP_A]],
// X86:             %[[VAL_A:.*]] = cir.load align(4) %[[TMP_A]]
// X86:             %[[VAL_B:.*]] = cir.load align(4) %[[TMP_B]]
// X86:             cir.call @"?bar@@YAXUA@@UB@@@Z"(%[[VAL_A]], %[[VAL_B]])
// X86:             cir.yield
// X86:           } cleanup normal {
// X86:             cir.call @"??1B@@QAE@XZ"
// X86:             cir.yield
// X86:         }
// X86:         cir.return
void test_caller_bar() {
  A a;
  B b;
  bar(a, b);
}

// ----------------------------------------------------------------------------
// c) Pass-by-reference: callee does NOT destroy, caller DOES destroy temporary
// ----------------------------------------------------------------------------

// X64-LABEL: cir.func no_inline dso_local @"?ref@@YAXAEBUA@@@Z"(%arg0: !cir.ptr<!rec_A>
// X64-NOT:     cir.call @"??1A@@QEAA@XZ"
// X64:         cir.return

// X86-LABEL: cir.func no_inline dso_local @"?ref@@YAXABUA@@@Z"(%arg0: !cir.ptr<!rec_A>
// X86-NOT:     cir.call @"??1A@@QAE@XZ"
// X86:         cir.return
void ref(const A &a) {}

// X64-LABEL: cir.func no_inline dso_local @"?test_caller_ref@@YAXXZ"()
// X64:         %[[REF_TMP:.*]] = cir.alloca "ref.tmp0" align(4) : !cir.ptr<!rec_A>
// X64:         cir.call @"??0A@@QEAA@XZ"(%[[REF_TMP]])
// X64:         cir.cleanup.scope {
// X64:           cir.call @"?ref@@YAXAEBUA@@@Z"(%[[REF_TMP]])
// X64:           cir.yield
// X64:         } cleanup normal {
// X64:           cir.call @"??1A@@QEAA@XZ"(%[[REF_TMP]]) nothrow
// X64:           cir.yield
// X64:         }
// X64:         cir.return

// X86-LABEL: cir.func no_inline dso_local @"?test_caller_ref@@YAXXZ"()
// X86:         %[[REF_TMP:.*]] = cir.alloca "ref.tmp0" align(4) : !cir.ptr<!rec_A>
// X86:         cir.call @"??0A@@QAE@XZ"(%[[REF_TMP]]) cc(x86_thiscall)
// X86:         cir.cleanup.scope {
// X86:           cir.call @"?ref@@YAXABUA@@@Z"(%[[REF_TMP]])
// X86:           cir.yield
// X86:         } cleanup normal {
// X86:           cir.call @"??1A@@QAE@XZ"(%[[REF_TMP]]) nothrow cc(x86_thiscall)
// X86:           cir.yield
// X86:         }
// X86:         cir.return
void test_caller_ref() {
  ref(A());
}

// ----------------------------------------------------------------------------
// d) Combination of callee-destructed parameter with indirect struct return (sret)
// ----------------------------------------------------------------------------

// Function has sret pointer (%arg0) and byval parameter (%arg1).
// Callee copies to sret, and cleans up byval parameter 'a'.
// X64-LABEL: cir.func no_inline dso_local @"?pass_and_ret@@YA?AUA@@U1@@Z"(%arg0: !cir.ptr<!rec_A> {{.*}}, %arg1: !rec_A {{.*}})
// X64:         %[[A_ADDR:.*]] = cir.alloca "a" align(4) init : !cir.ptr<!rec_A>
// X64:         cir.store %arg1, %[[A_ADDR]] : !rec_A, !cir.ptr<!rec_A>
// X64:         cir.cleanup.scope {
// X64:           cir.call @"??0A@@QEAA@AEBU0@@Z"(%arg0, %[[A_ADDR]])
// X64:           cir.return
// X64:         } cleanup normal {
// X64:           cir.call @"??1A@@QEAA@XZ"(%[[A_ADDR]]) nothrow
// X64:           cir.yield
// X64:         }

// X86-LABEL: cir.func no_inline dso_local @"?pass_and_ret@@YA?AUA@@U1@@Z"(%arg0: !cir.ptr<!rec_A> {{.*}}, %arg1: !rec_A {{.*}})
// X86:         %[[A_ADDR:.*]] = cir.alloca "a" align(4) init : !cir.ptr<!rec_A>
// X86:         cir.store %arg1, %[[A_ADDR]] : !rec_A, !cir.ptr<!rec_A>
// X86:         cir.cleanup.scope {
// X86:           cir.call @"??0A@@QAE@ABU0@@Z"(%arg0, %[[A_ADDR]]) cc(x86_thiscall)
// X86:           cir.return
// X86:         } cleanup normal {
// X86:           cir.call @"??1A@@QAE@XZ"(%[[A_ADDR]]) nothrow cc(x86_thiscall)
// X86:           cir.yield
// X86:         }
A pass_and_ret(A a) {
  return a;
}

// Caller provides sret buffer and passes byval argument.
// X64-LABEL: cir.func no_inline dso_local @"?test_caller_pass_and_ret@@YAXXZ"()
// X64:         %[[LOCAL_A:.*]] = cir.alloca "a" align(4) init : !cir.ptr<!rec_A>
// X64:         %[[RESULT:.*]] = cir.alloca "result" align(4) init : !cir.ptr<!rec_A>
// X64:         %[[TMP_A:.*]] = cir.alloca "agg.tmp0" align(4) : !cir.ptr<!rec_A>
// X64:         cir.call @"??0A@@QEAA@XZ"(%[[LOCAL_A]])
// X64:         cir.cleanup.scope {
// X64:           cir.call @"??0A@@QEAA@AEBU0@@Z"(%[[TMP_A]], %[[LOCAL_A]])
// X64:           %[[VAL_A:.*]] = cir.load align(4) %[[TMP_A]]
// X64:           cir.call @"?pass_and_ret@@YA?AUA@@U1@@Z"(%[[RESULT]], %[[VAL_A]]) : (!cir.ptr<!rec_A>, !rec_A) -> ()
// X64:           cir.cleanup.scope {
// X64:             cir.yield
// X64:           } cleanup normal {
// X64:             cir.call @"??1A@@QEAA@XZ"(%[[RESULT]])
// X64:             cir.yield
// X64:           }
// X64:           cir.yield
// X64:         } cleanup normal {
// X64:           cir.call @"??1A@@QEAA@XZ"(%[[LOCAL_A]])
// X64:           cir.yield
// X64:         }

// X86-LABEL: cir.func no_inline dso_local @"?test_caller_pass_and_ret@@YAXXZ"()
// X86:         %[[LOCAL_A:.*]] = cir.alloca "a" align(4) init : !cir.ptr<!rec_A>
// X86:         %[[RESULT:.*]] = cir.alloca "result" align(4) init : !cir.ptr<!rec_A>
// X86:         %[[TMP_A:.*]] = cir.alloca "agg.tmp0" align(4) : !cir.ptr<!rec_A>
// X86:         cir.call @"??0A@@QAE@XZ"(%[[LOCAL_A]]) cc(x86_thiscall)
// X86:         cir.cleanup.scope {
// X86:           cir.call @"??0A@@QAE@ABU0@@Z"(%[[TMP_A]], %[[LOCAL_A]]) cc(x86_thiscall)
// X86:           %[[VAL_A:.*]] = cir.load align(4) %[[TMP_A]]
// X86:           cir.call @"?pass_and_ret@@YA?AUA@@U1@@Z"(%[[RESULT]], %[[VAL_A]]) : (!cir.ptr<!rec_A>, !rec_A) -> ()
// X86:           cir.cleanup.scope {
// X86:             cir.yield
// X86:           } cleanup normal {
// X86:             cir.call @"??1A@@QAE@XZ"(%[[RESULT]]) nothrow cc(x86_thiscall)
// X86:             cir.yield
// X86:           }
// X86:           cir.yield
// X86:         } cleanup normal {
// X86:           cir.call @"??1A@@QAE@XZ"(%[[LOCAL_A]]) nothrow cc(x86_thiscall)
// X86:           cir.yield
// X86:         }
void test_caller_pass_and_ret() {
  A a;
  A result = pass_and_ret(a);
}

// ----------------------------------------------------------------------------
// e) Combination with instance method and sretAfterThis
// ----------------------------------------------------------------------------

struct S {
  A method(A a);
};

// Method has 'this' at arg0, 'sret' at arg1, and byval 'a' at arg2.
// X64-LABEL: cir.func no_inline dso_local @"?method@S@@QEAA?AUA@@U2@@Z"(%arg0: !cir.ptr<!rec_S> {{.*}}, %arg1: !cir.ptr<!rec_A> {{.*}}, %arg2: !rec_A {{.*}})
// X64:         %[[THIS_ADDR:.*]] = cir.alloca "this" align(8) init : !cir.ptr<!cir.ptr<!rec_S>>
// X64:         %[[A_ADDR:.*]] = cir.alloca "a" align(4) init : !cir.ptr<!rec_A>
// X64:         cir.store %arg0, %[[THIS_ADDR]]
// X64:         cir.store %arg2, %[[A_ADDR]]
// X64:         cir.cleanup.scope {
// X64:           cir.call @"??0A@@QEAA@AEBU0@@Z"(%arg1, %[[A_ADDR]])
// X64:           cir.return
// X64:         } cleanup normal {
// X64:           cir.call @"??1A@@QEAA@XZ"(%[[A_ADDR]]) nothrow
// X64:           cir.yield
// X64:         }

// X86-LABEL: cir.func no_inline dso_local @"?method@S@@QAE?AUA@@U2@@Z"(%arg0: !cir.ptr<!rec_S> {{.*}}, %arg1: !cir.ptr<!rec_A> {{.*}}, %arg2: !rec_A {{.*}}) cc(x86_thiscall)
// X86:         %[[THIS_ADDR:.*]] = cir.alloca "this" align(4) init : !cir.ptr<!cir.ptr<!rec_S>>
// X86:         %[[A_ADDR:.*]] = cir.alloca "a" align(4) init : !cir.ptr<!rec_A>
// X86:         cir.store %arg0, %[[THIS_ADDR]]
// X86:         cir.store %arg2, %[[A_ADDR]]
// X86:         cir.cleanup.scope {
// X86:           cir.call @"??0A@@QAE@ABU0@@Z"(%arg1, %[[A_ADDR]]) cc(x86_thiscall)
// X86:           cir.return
// X86:         } cleanup normal {
// X86:           cir.call @"??1A@@QAE@XZ"(%[[A_ADDR]]) nothrow cc(x86_thiscall)
// X86:           cir.yield
// X86:         }
A S::method(A a) {
  return a;
}

// Caller calls method passing 'this', 'sret', and byval argument.
// X64-LABEL: cir.func no_inline dso_local @"?test_caller_method@@YAXAEAUS@@@Z"(%arg0: !cir.ptr<!rec_S>
// X64:         %[[S_PTR:.*]] = cir.alloca "s" align(8) init const : !cir.ptr<!cir.ptr<!rec_S>>
// X64:         %[[LOCAL_A:.*]] = cir.alloca "a" align(4) init : !cir.ptr<!rec_A>
// X64:         %[[RESULT:.*]] = cir.alloca "result" align(4) init : !cir.ptr<!rec_A>
// X64:         %[[TMP_A:.*]] = cir.alloca "agg.tmp0" align(4) : !cir.ptr<!rec_A>
// X64:         cir.cleanup.scope {
// X64:           %[[LOAD_S:.*]] = cir.load %[[S_PTR]] : !cir.ptr<!cir.ptr<!rec_S>>, !cir.ptr<!rec_S>
// X64:           cir.call @"??0A@@QEAA@AEBU0@@Z"(%[[TMP_A]], %[[LOCAL_A]])
// X64:           %[[VAL_A:.*]] = cir.load align(4) %[[TMP_A]]
// X64:           cir.call @"?method@S@@QEAA?AUA@@U2@@Z"(%[[LOAD_S]], %[[RESULT]], %[[VAL_A]]) : (!cir.ptr<!rec_S> {{.*}}, !cir.ptr<!rec_A>, !rec_A) -> ()
// X64:           cir.cleanup.scope {
// X64:             cir.yield
// X64:           } cleanup normal {
// X64:             cir.call @"??1A@@QEAA@XZ"(%[[RESULT]])
// X64:             cir.yield
// X64:           }
// X64:           cir.yield
// X64:         } cleanup normal {
// X64:           cir.call @"??1A@@QEAA@XZ"(%[[LOCAL_A]])
// X64:           cir.yield
// X64:         }

// X86-LABEL: cir.func no_inline dso_local @"?test_caller_method@@YAXAAUS@@@Z"(%arg0: !cir.ptr<!rec_S>
// X86:         %[[S_PTR:.*]] = cir.alloca "s" align(4) init const : !cir.ptr<!cir.ptr<!rec_S>>
// X86:         %[[LOCAL_A:.*]] = cir.alloca "a" align(4) init : !cir.ptr<!rec_A>
// X86:         %[[RESULT:.*]] = cir.alloca "result" align(4) init : !cir.ptr<!rec_A>
// X86:         %[[TMP_A:.*]] = cir.alloca "agg.tmp0" align(4) : !cir.ptr<!rec_A>
// X86:         cir.cleanup.scope {
// X86:           %[[LOAD_S:.*]] = cir.load %[[S_PTR]] : !cir.ptr<!cir.ptr<!rec_S>>, !cir.ptr<!rec_S>
// X86:           cir.call @"??0A@@QAE@ABU0@@Z"(%[[TMP_A]], %[[LOCAL_A]]) cc(x86_thiscall)
// X86:           %[[VAL_A:.*]] = cir.load align(4) %[[TMP_A]]
// X86:           cir.call @"?method@S@@QAE?AUA@@U2@@Z"(%[[LOAD_S]], %[[RESULT]], %[[VAL_A]]) cc(x86_thiscall) : (!cir.ptr<!rec_S> {{.*}}, !cir.ptr<!rec_A>, !rec_A) -> ()
// X86:           cir.cleanup.scope {
// X86:             cir.yield
// X86:           } cleanup normal {
// X86:             cir.call @"??1A@@QAE@XZ"(%[[RESULT]]) nothrow cc(x86_thiscall)
// X86:             cir.yield
// X86:           }
// X86:           cir.yield
// X86:         } cleanup normal {
// X86:           cir.call @"??1A@@QAE@XZ"(%[[LOCAL_A]]) nothrow cc(x86_thiscall)
// X86:           cir.yield
// X86:         }
void test_caller_method(S &s) {
  A a;
  A result = s.method(a);
}
