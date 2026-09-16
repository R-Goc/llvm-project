// RUN: %clang_cc1 -std=c++23 -triple x86_64-pc-windows-msvc -fclangir -emit-cir %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir %s --check-prefix=X64
// RUN: %clang_cc1 -std=c++23 -triple i386-pc-windows-msvc -fclangir -emit-cir %s -o %t32.cir
// RUN: FileCheck --input-file=%t32.cir %s --check-prefix=X86

struct S {
  friend void test_calls();
public:
  void a(this auto) {}
  void b(this auto&) {}
  void c(this S) {}
  void c(this S, int) {}
private:
  void d(this auto) {}
protected:
  void g(this auto) {}
};

struct Small {
  int x;
  int y;
};

struct Large {
  int a;
  int b;
  int c;
};

struct S2 {
  Small ret_small(this const S2&) { return Small{1, 2}; }
  Large ret_large(this const S2&) { return Large{1, 2, 3}; }
};

void test_calls() {
  S s;
  s.a();
  s.b();
  s.c();
  s.c(1);
  s.d();
  s.g();
}

// X64-LABEL: cir.func no_inline dso_local @"?test_calls@@YAXXZ"
// X64:         cir.call @"??$a@US@@@S@@SAX_VU0@@Z"
// X64:         cir.call @"??$b@US@@@S@@SAX_VAEAU0@@Z"
// X64:         cir.call @"?c@S@@SAX_VU1@@Z"
// X64:         cir.call @"?c@S@@SAX_VU1@H@Z"
// X64:         cir.call @"??$d@US@@@S@@CAX_VU0@@Z"
// X64:         cir.call @"??$g@US@@@S@@KAX_VU0@@Z"

// X86-LABEL: cir.func no_inline dso_local @"?test_calls@@YAXXZ"
// X86:         cir.call @"??$a@US@@@S@@SAX_VU0@@Z"
// X86:         cir.call @"??$b@US@@@S@@SAX_VAAU0@@Z"
// X86:         cir.call @"?c@S@@SAX_VU1@@Z"
// X86:         cir.call @"?c@S@@SAX_VU1@H@Z"
// X86:         cir.call @"??$d@US@@@S@@CAX_VU0@@Z"
// X86:         cir.call @"??$g@US@@@S@@KAX_VU0@@Z"

void test_returns() {
  S2 s2;
  s2.ret_small();
  s2.ret_large();
}

// Struct returns: small structs returned directly, large structs returned via indirect sret in first position (Arg 0).
// X64-LABEL: cir.func no_inline dso_local @"?test_returns@@YAXXZ"
// X64:         cir.call @"?ret_small@S2@@SA?AUSmall@@_VAEBU1@@Z"(%{{.+}}) : (!cir.ptr<!rec_S2> {{.*}}) -> !u64i
// X64:         cir.call @"?ret_large@S2@@SA?AULarge@@_VAEBU1@@Z"(%[[SRET_PTR:.+]], %{{.+}}) : (!cir.ptr<!rec_Large>, !cir.ptr<!rec_S2> {{.*}}) -> ()

// X86-LABEL: cir.func no_inline dso_local @"?test_returns@@YAXXZ"
// X86:         cir.call @"?ret_small@S2@@SA?AUSmall@@_VABU1@@Z"(%{{.+}}) : (!cir.ptr<!rec_S2> {{.*}}) -> !rec_Small
// X86:         cir.call @"?ret_large@S2@@SA?AULarge@@_VABU1@@Z"(%[[SRET_PTR:.+]], %{{.+}}) : (!cir.ptr<!rec_Large>, !cir.ptr<!rec_S2> {{.*}}) -> ()

// Large struct return definition: sret is Arg 0, explicit object parameter is Arg 1.
// X64-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?ret_large@S2@@SA?AULarge@@_VAEBU1@@Z"(%arg0: !cir.ptr<!rec_Large> {{.*}}, %arg1: !cir.ptr<!rec_S2> {{.*}})
// X64:         %[[MEMBER:.+]] = cir.get_member %arg0[0] {name = "a"} : !cir.ptr<!rec_Large> -> !cir.ptr<!s32i>
// X64:         cir.return

// X86-LABEL: cir.func no_inline comdat linkonce_odr dso_local @"?ret_large@S2@@SA?AULarge@@_VABU1@@Z"(%arg0: !cir.ptr<!rec_Large> {{.*}}, %arg1: !cir.ptr<!rec_S2> {{.*}})
// X86:         %[[MEMBER:.+]] = cir.get_member %arg0[0] {name = "a"} : !cir.ptr<!rec_Large> -> !cir.ptr<!s32i>
// X86:         cir.return

int test_lambda() {
  int x = 42;
  auto lam = [x](this auto const &self) {
    return x;
  };
  return lam();
}

// Lambda capture accessed via explicit object parameter (self).
// X64-LABEL: cir.func no_inline lambda internal private dso_local @"??$?RV<lambda_0>{{.*}}"
// X64:         %[[SELF_ALLOCA:.+]] = cir.alloca "self"
// X64:         %[[SELF:.+]] = cir.load %[[SELF_ALLOCA]]
// X64:         %[[FIELD:.+]] = cir.get_member %[[SELF]][0] {name = "x"}
// X64:         %[[VAL:.+]] = cir.load align(4) %[[FIELD]]
// X64:         cir.return %{{.+}} : !s32i

// X86-LABEL: cir.func no_inline lambda internal private dso_local @"??$?RV<lambda_0>{{.*}}"
// X86:         %[[SELF_ALLOCA:.+]] = cir.alloca "self"
// X86:         %[[SELF:.+]] = cir.load %[[SELF_ALLOCA]]
// X86:         %[[FIELD:.+]] = cir.get_member %[[SELF]][0] {name = "x"}
// X86:         %[[VAL:.+]] = cir.load align(4) %[[FIELD]]
// X86:         cir.return %{{.+}} : !s32i
