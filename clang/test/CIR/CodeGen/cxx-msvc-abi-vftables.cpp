// RUN: %clang_cc1 %s -fclangir -emit-cir -fno-rtti -triple=i386-pc-win32 -fms-extensions -O1 -o - | FileCheck %s --check-prefix=NO-RTTI
// RUN: %clang_cc1 %s -fclangir -emit-cir -triple=i386-pc-win32 -fms-extensions -O1 -o - | FileCheck %s --check-prefix=RTTI

//===----------------------------------------------------------------------===//
// Module-level Globals (emitted at the top of MLIR module)
//===----------------------------------------------------------------------===//

// RTTI-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7S@@6B@" = #cir.vtable<{#cir.const_array<[#cir.global_view<@"??_R0?AUS@@@8"> : !cir.ptr<!void>, #cir.global_view<@"??_ES@@UAEPAXI@Z"> : !cir.ptr<!u8i>]> : !cir.array<!cir.ptr<!u8i> x 2>}>
// RTTI-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_SU@@6B@" = #cir.vtable<{#cir.const_array<[#cir.global_view<@"??_R0?AUU@@@8"> : !cir.ptr<!void>, #cir.global_view<@"??_EU@@UAEPAXI@Z"> : !cir.ptr<!u8i>]> : !cir.array<!cir.ptr<!u8i> x 2>}>
// RTTI-DAG: cir.global "private" constant weak_odr comdat dso_local dllexport @"??_7V@@6B@" = #cir.vtable<{#cir.const_array<[#cir.global_view<@"??_R0?AUV@@@8"> : !cir.ptr<!void>, #cir.global_view<@"??_EV@@UAEPAXI@Z"> : !cir.ptr<!u8i>]> : !cir.array<!cir.ptr<!u8i> x 2>}>
// RTTI-DAG: cir.global "private" constant internal dso_local @"??_7W@?A0x{{[^@]*}}@@6B@" = #cir.vtable<{#cir.const_array<[#cir.global_view<@"??_R0?AUW@?A0x{{[^@]*}}@@@8"> : !cir.ptr<!void>, #cir.global_view<@"??_EW@?A0x{{[^@]*}}@@UAEPAXI@Z"> : !cir.ptr<!u8i>]> : !cir.array<!cir.ptr<!u8i> x 2>}>
// RTTI-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7?$Y@H@@6B@" = #cir.vtable<{#cir.const_array<[#cir.global_view<@"??_R0?AU?$Y@H@@@8"> : !cir.ptr<!void>, #cir.global_view<@"??_E?$Y@H@@UAEPAXI@Z"> : !cir.ptr<!u8i>]> : !cir.array<!cir.ptr<!u8i> x 2>}>

// NO-RTTI-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7S@@6B@" = #cir.vtable<{#cir.const_array<[#cir.global_view<@"??_ES@@UAEPAXI@Z"> : !cir.ptr<!u8i>]> : !cir.array<!cir.ptr<!u8i> x 1>}>
// NO-RTTI-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_SU@@6B@" = #cir.vtable<{#cir.const_array<[#cir.global_view<@"??_EU@@UAEPAXI@Z"> : !cir.ptr<!u8i>]> : !cir.array<!cir.ptr<!u8i> x 1>}>
// NO-RTTI-DAG: cir.global "private" constant weak_odr comdat dso_local dllexport @"??_7V@@6B@" = #cir.vtable<{#cir.const_array<[#cir.global_view<@"??_EV@@UAEPAXI@Z"> : !cir.ptr<!u8i>]> : !cir.array<!cir.ptr<!u8i> x 1>}>
// NO-RTTI-DAG: cir.global "private" constant internal dso_local @"??_7W@?A0x{{[^@]*}}@@6B@" = #cir.vtable<{#cir.const_array<[#cir.global_view<@"??_EW@?A0x{{[^@]*}}@@UAEPAXI@Z"> : !cir.ptr<!u8i>]> : !cir.array<!cir.ptr<!u8i> x 1>}>
// NO-RTTI-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_7?$Y@H@@6B@" = #cir.vtable<{#cir.const_array<[#cir.global_view<@"??_E?$Y@H@@UAEPAXI@Z"> : !cir.ptr<!u8i>]> : !cir.array<!cir.ptr<!u8i> x 1>}>

struct S {
  virtual ~S();
} s;

struct __declspec(dllimport) U {
  virtual ~U();
} u;

struct __declspec(dllexport) V {
  virtual ~V();
} v;

namespace {
struct W {
  virtual ~W() {}
} w;
}

struct X {};
template <class> struct Y : virtual X {
  Y() {}
  virtual ~Y();
};

extern template class Y<int>;
template Y<int>::Y();
