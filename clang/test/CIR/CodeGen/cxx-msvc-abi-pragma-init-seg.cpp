// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fms-extensions -fdeclspec -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s

// Check module attributes at top: global ctors list contains prioritized initializers and module init.
// CHECK: module {{.*}} cir.global_ctors = [
// CHECK-DAG: #cir.global_ctor<"??__Ex@simple_init@@YAXXZ", 200>
// CHECK-DAG: #cir.global_ctor<"??__Ey@simple_init@@YAXXZ", 400>
// CHECK-DAG: #cir.global_ctor<"_GLOBAL__sub_I_cxx_msvc_abi_pragma_init_seg.cpp", 65535>

// Module-level custom section function pointer tables.
// CHECK-DAG: cir.global "private" constant cir_private @__cxx_init_fn_ptr = #cir.global_view<@"??__Ex@?A0x{{[^@]*}}@internal_init@@YAXXZ"> : !cir.ptr<!cir.func<()>> {section = ".asdf"}
// CHECK-DAG: cir.global "private" constant cir_private comdat @__cxx_init_fn_ptr.1 = #cir.global_view<@"??__Ex@selectany_init@@YAXXZ"> : !cir.ptr<!cir.func<()>> {section = ".asdf"}
// CHECK-DAG: cir.global "private" constant cir_private comdat @__cxx_init_fn_ptr.2 = #cir.global_view<@"??__E?x@?$A@H@explicit_template_instantiation@@2HB@@YAXXZ"> : !cir.ptr<!cir.func<()>> {section = ".asdf"}
// CHECK-DAG: cir.global "private" constant cir_private comdat @__cxx_init_fn_ptr.3 = #cir.global_view<@"??__E?x@?$B@H@implicit_template_instantiation@@2HB@@YAXXZ"> : !cir.ptr<!cir.func<()>> {section = ".asdf"}

int f();

namespace simple_init {
#pragma init_seg(compiler)
int x = f();
// CHECK: cir.global external dso_local @"?x@simple_init@@3HA" = #cir.int<0> : !s32i {{.*}}init_priority = 200
// CHECK: cir.func internal private @"??__Ex@simple_init@@YAXXZ"()
// CHECK:   cir.call @"?f@@YAHXZ"()

#pragma init_seg(lib)
int y = f();
// CHECK: cir.global external dso_local @"?y@simple_init@@3HA" = #cir.int<0> : !s32i {{.*}}init_priority = 400
// CHECK: cir.func internal private @"??__Ey@simple_init@@YAXXZ"()
// CHECK:   cir.call @"?f@@YAHXZ"()

#pragma init_seg(user)
int z = f();
// CHECK: cir.global external dso_local @"?z@simple_init@@3HA" = #cir.int<0> : !s32i
// CHECK: cir.func internal private @"??__Ez@simple_init@@YAXXZ"()
// CHECK:   cir.call @"?f@@YAHXZ"()
}

#pragma init_seg(".asdf")

namespace internal_init {
namespace {
int x = f();
// CHECK: cir.global "private" internal dso_local @"?x@?A0x{{[^@]*}}@internal_init@@3HA" = #cir.int<0> : !s32i
// CHECK: cir.func internal private @"??__Ex@?A0x{{[^@]*}}@internal_init@@YAXXZ"()
// CHECK:   cir.call @"?f@@YAHXZ"()
}
}

namespace selectany_init {
int __declspec(selectany) x = f();
// CHECK: cir.global weak_odr comdat dso_local @"?x@selectany_init@@3HA" = #cir.int<0> : !s32i
// CHECK: cir.func comdat linkonce_odr private @"??__Ex@selectany_init@@YAXXZ"()
// CHECK:   cir.call @"?f@@YAHXZ"()
}

namespace explicit_template_instantiation {
template <typename T> struct A { static const int x; };
template <typename T> const int A<T>::x = f();
template struct A<int>;
// CHECK: cir.global weak_odr comdat dso_local @"?x@?$A@H@explicit_template_instantiation@@2HB" = #cir.int<0> : !s32i
// CHECK: cir.func comdat linkonce_odr private @"??__E?x@?$A@H@explicit_template_instantiation@@2HB@@YAXXZ"()
// CHECK:   cir.call @"?f@@YAHXZ"()
}

namespace implicit_template_instantiation {
template <typename T> struct B { static const int x; };
template <typename T> const int B<T>::x = f();
int g() { return B<int>::x; }
// CHECK: cir.global linkonce_odr comdat dso_local @"?x@?$B@H@implicit_template_instantiation@@2HB" = #cir.int<0> : !s32i
// CHECK: cir.func comdat linkonce_odr private @"??__E?x@?$B@H@implicit_template_instantiation@@2HB@@YAXXZ"()
// CHECK:   cir.call @"?f@@YAHXZ"()
}

// CHECK: cir.global appending @llvm.used = #cir.const_array<[#cir.global_view<@__cxx_init_fn_ptr> : !cir.ptr<!void>, #cir.global_view<@__cxx_init_fn_ptr.1> : !cir.ptr<!void>, #cir.global_view<@__cxx_init_fn_ptr.2> : !cir.ptr<!void>, #cir.global_view<@__cxx_init_fn_ptr.3> : !cir.ptr<!void>]> : !cir.array<!cir.ptr<!void> x 4> {section = "llvm.metadata"}

// User level module initializer calls user-priority inits.
// CHECK-LABEL: cir.func internal private @_GLOBAL__sub_I_cxx_msvc_abi_pragma_init_seg.cpp()
// CHECK:   cir.call @"??__Ez@simple_init@@YAXXZ"()
// CHECK-NOT: ??__Ex@simple_init
// CHECK-NOT: ??__Ey@simple_init
// CHECK-NOT: internal_init
// CHECK-NOT: selectany_init
// CHECK-NOT: explicit_template_instantiation
// CHECK-NOT: implicit_template_instantiation

