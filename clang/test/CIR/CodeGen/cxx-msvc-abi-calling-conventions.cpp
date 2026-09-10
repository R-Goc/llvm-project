// RUN: %clang_cc1 -std=c++17 -triple i386-pc-windows-msvc -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fclangir -emit-cir %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir %s -check-prefix=CIR64

class C {
public:
  void simple_method() {}
  void __cdecl cdecl_method() {}
  void vararg_method(const char *fmt, ...) {}
  static void static_method() {}
};

// CIR-LABEL: cir.func {{.*}}@"?test_member_methods@@YAXXZ"()
void test_member_methods() {
  C instance;

  // Member method call defaults to __thiscall on 32-bit x86.
  // CIR: cir.call @"?simple_method@C@@QAEXXZ"(%{{.+}}) cc(x86_thiscall)
  instance.simple_method();

  // Explicit __cdecl overrides default to C calling convention.
  // CIR: cir.call @"?cdecl_method@C@@QAAXXZ"(%{{.+}}) : (!cir.ptr<!rec_C>{{.*}}) -> ()
  instance.cdecl_method();

  // Variadic member methods override default thiscall to C calling convention.
  // CIR: cir.call @"?vararg_method@C@@QAAXPBDZZ"(%{{.+}}, %{{.+}}) : (!cir.ptr<!rec_C>{{.*}}, !cir.ptr<!s8i>{{.*}}) -> ()
  instance.vararg_method("hello");

  // Static member methods default to C calling convention.
  // CIR: cir.call @"?static_method@C@@SAXXZ"() : () -> ()
  C::static_method();
}

// CIR-LABEL: cir.func {{.*}}@"?simple_method@C@@QAEXXZ"({{.*}}) cc(x86_thiscall)

// CIR-LABEL: cir.func {{.*}}@"?cdecl_method@C@@QAAXXZ"({{.*}})

// CIR-LABEL: cir.func {{.*}}@"?vararg_method@C@@QAAXPBDZZ"({{.*}})

// CIR-LABEL: cir.func {{.*}}@"?static_method@C@@SAXXZ"()

void __stdcall stdcall_func() {}
void __fastcall fastcall_func() {}
void __vectorcall vectorcall_func() {}

// CIR-LABEL: cir.func {{.*}}@"?stdcall_func@@YGXXZ"() cc(x86_stdcall)

// CIR-LABEL: cir.func {{.*}}@"?fastcall_func@@YIXXZ"() cc(x86_fastcall)

// CIR-LABEL: cir.func {{.*}}@"?vectorcall_func@@YQXXZ"() cc(x86_vectorcall)

// CIR-LABEL: cir.func {{.*}}@"?test_calling_conventions@@YAXXZ"()
void test_calling_conventions() {
  // CIR: cir.call @"?stdcall_func@@YGXXZ"() cc(x86_stdcall)
  stdcall_func();

  // CIR: cir.call @"?fastcall_func@@YIXXZ"() cc(x86_fastcall)
  fastcall_func();

  // CIR: cir.call @"?vectorcall_func@@YQXXZ"() cc(x86_vectorcall)
  vectorcall_func();
}

class Base {
public:
  Base();
  ~Base();
};

class Child : public Base {
public:
  Child();
  ~Child();
};

// CIR-LABEL: cir.func {{.*}}@"??0Child@@QAE@XZ"({{.*}}cc(x86_thiscall)
// CIR: cir.call @"??0Base@@QAE@XZ"(%{{.+}}) cc(x86_thiscall)
Child::Child() {}

// CIR-LABEL: cir.func {{.*}}@"??1Child@@QAE@XZ"({{.*}}cc(x86_thiscall)
// CIR: cir.call @"??1Base@@QAE@XZ"(%{{.+}}){{.*}}cc(x86_thiscall)
Child::~Child() {}

#if defined(_M_X64)
void __vectorcall win64_vectorcall_func() {}

// CIR64-LABEL: cir.func {{.*}}@"?win64_vectorcall_func@@YQXXZ"() cc(x86_vectorcall)
void test_win64_vectorcall() {
  // CIR64: cir.call @"?win64_vectorcall_func@@YQXXZ"() cc(x86_vectorcall)
  win64_vectorcall_func();
}
#endif
