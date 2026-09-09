// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fdeclspec -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s

// Check module attributes at top: global ctors list contains vague linkage initializers and module init.
// CHECK: module {{.*}} cir.global_ctors = [
// CHECK-DAG: #cir.global_ctor<"??__Eselectany1@@YAXXZ", 65535>
// CHECK-DAG: #cir.global_ctor<"??__Eselectany2@@YAXXZ", 65535>
// CHECK-DAG: #cir.global_ctor<"??__E?s@?$ExportedTemplate@H@@2US@@A@@YAXXZ", 65535>
// CHECK-DAG: #cir.global_ctor<"_GLOBAL__sub_I_cxx_msvc_abi_static_initializers.cpp", 65535>

// CHECK-NOT: @__cxa_atexit
// CHECK-NOT: @__dso_handle

struct S {
  S();
  ~S();
};

struct SNoDtor {
  SNoDtor();
};

S s;
// CHECK: cir.global external dso_local @"?s@@3US@@A" = #cir.zero : !rec_S
// CHECK-LABEL: cir.func internal private @"??__Fs@@YAXXZ"()
// CHECK:   %[[S_ADDR2:.*]] = cir.get_global @"?s@@3US@@A" : !cir.ptr<!rec_S>
// CHECK:   cir.call @"??1S@@QEAA@XZ"(%[[S_ADDR2]])
// CHECK:   cir.return

// CHECK-LABEL: cir.func internal private @"??__Es@@YAXXZ"()
// CHECK:   %[[S_ADDR:.*]] = cir.get_global @"?s@@3US@@A" : !cir.ptr<!rec_S>
// CHECK:   cir.call @"??0S@@QEAA@XZ"(%[[S_ADDR]])
// CHECK:   %[[S_DTOR:.*]] = cir.get_global @"??__Fs@@YAXXZ" : !cir.ptr<!cir.func<()>>
// CHECK:   cir.call @atexit(%[[S_DTOR]])
// CHECK:   cir.return

SNoDtor s_nodtor;
// CHECK: cir.global external dso_local @"?s_nodtor@@3USNoDtor@@A" = #cir.zero : !rec_SNoDtor
// CHECK-LABEL: cir.func internal private @"??__Es_nodtor@@YAXXZ"()
// CHECK:   %[[SND_ADDR:.*]] = cir.get_global @"?s_nodtor@@3USNoDtor@@A" : !cir.ptr<!rec_SNoDtor>
// CHECK:   cir.call @"??0SNoDtor@@QEAA@XZ"(%[[SND_ADDR]])
// CHECK-NOT: atexit
// CHECK:   cir.return

namespace N {
S s_nested;
}
// CHECK: cir.global external dso_local @"?s_nested@N@@3US@@A" = #cir.zero : !rec_S
// CHECK-LABEL: cir.func internal private @"??__Fs_nested@N@@YAXXZ"()
// CHECK:   %[[SN_ADDR2:.*]] = cir.get_global @"?s_nested@N@@3US@@A" : !cir.ptr<!rec_S>
// CHECK:   cir.call @"??1S@@QEAA@XZ"(%[[SN_ADDR2]])
// CHECK:   cir.return

// CHECK-LABEL: cir.func internal private @"??__Es_nested@N@@YAXXZ"()
// CHECK:   %[[SN_ADDR:.*]] = cir.get_global @"?s_nested@N@@3US@@A" : !cir.ptr<!rec_S>
// CHECK:   cir.call @"??0S@@QEAA@XZ"(%[[SN_ADDR]])
// CHECK:   %[[SN_DTOR:.*]] = cir.get_global @"??__Fs_nested@N@@YAXXZ" : !cir.ptr<!cir.func<()>>
// CHECK:   cir.call @atexit(%[[SN_DTOR]])
// CHECK:   cir.return

__declspec(selectany) S selectany1;
__declspec(selectany) S selectany2;
// CHECK: cir.global weak_odr comdat dso_local @"?selectany1@@3US@@A" = #cir.zero : !rec_S
// CHECK-LABEL: cir.func internal private @"??__Fselectany1@@YAXXZ"()
// CHECK:   cir.call @"??1S@@QEAA@XZ"
// CHECK:   cir.return

// CHECK-LABEL: cir.func comdat linkonce_odr private @"??__Eselectany1@@YAXXZ"()
// CHECK:   cir.call @"??0S@@QEAA@XZ"
// CHECK:   cir.call @atexit
// CHECK:   cir.return

// CHECK: cir.global weak_odr comdat dso_local @"?selectany2@@3US@@A" = #cir.zero : !rec_S
// CHECK-LABEL: cir.func internal private @"??__Fselectany2@@YAXXZ"()
// CHECK:   cir.call @"??1S@@QEAA@XZ"
// CHECK:   cir.return

// CHECK-LABEL: cir.func comdat linkonce_odr private @"??__Eselectany2@@YAXXZ"()
// CHECK:   cir.call @"??0S@@QEAA@XZ"
// CHECK:   cir.call @atexit
// CHECK:   cir.return

template <typename T>
struct ExportedTemplate {
  static S s;
};
template <typename T>
S ExportedTemplate<T>::s;

void useExportedTemplate(ExportedTemplate<int> x) {
  (void)x.s;
}
// CHECK: cir.global linkonce_odr comdat dso_local @"?s@?$ExportedTemplate@H@@2US@@A" = #cir.zero : !rec_S
// CHECK-LABEL: cir.func internal private @"??__F?s@?$ExportedTemplate@H@@2US@@A@@YAXXZ"()
// CHECK:   cir.call @"??1S@@QEAA@XZ"
// CHECK:   cir.return

// CHECK-LABEL: cir.func comdat linkonce_odr private @"??__E?s@?$ExportedTemplate@H@@2US@@A@@YAXXZ"()
// CHECK:   cir.call @"??0S@@QEAA@XZ"
// CHECK:   %[[ET_DTOR:.*]] = cir.get_global @"??__F?s@?$ExportedTemplate@H@@2US@@A@@YAXXZ"
// CHECK:   cir.call @atexit(%[[ET_DTOR]])
// CHECK:   cir.return

// CHECK-LABEL: cir.func internal private @_GLOBAL__sub_I_cxx_msvc_abi_static_initializers.cpp()
// CHECK:   cir.call @"??__Es@@YAXXZ"()
// CHECK:   cir.call @"??__Es_nodtor@@YAXXZ"()
// CHECK:   cir.call @"??__Es_nested@N@@YAXXZ"()
// CHECK:   cir.return
