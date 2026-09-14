// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fms-extensions -Wno-final-dtor-non-final-class -fclangir -emit-cir -mmlir -mlir-print-ir-before=cir-cxxabi-lowering %s -o %t64.cir 2> %t64-before.cir
// RUN: FileCheck --input-file=%t64-before.cir --check-prefix=BEFORE-X64 %s
// RUN: FileCheck --input-file=%t64.cir --check-prefix=CIR-X64 %s
// RUN: %clang_cc1 -std=c++17 -triple i386-pc-windows-msvc -fms-extensions -Wno-final-dtor-non-final-class -fclangir -emit-cir -mmlir -mlir-print-ir-before=cir-cxxabi-lowering %s -o %t32.cir 2> %t32-before.cir
// RUN: FileCheck --input-file=%t32-before.cir --check-prefix=BEFORE-X86 %s
// RUN: FileCheck --input-file=%t32.cir --check-prefix=CIR-X86 %s

// Adapted from Clang CodeGenCXX test: ms-vdtors-devirtualization.cpp

struct Base {
  virtual ~Base();
};

struct A final : Base {
  virtual ~A();
};

struct B : Base {
  virtual ~B() final;
};

struct D {
  virtual ~D() final = 0;
};

// Case 1: Deleting array of class marked final devirtualizes destructor call.
// No vtable lookup is emitted; cir.delete_array emits loop calling ??1A and calls ??_V.
void test_final_class(A *a) {
  delete[] a;
}

// BEFORE-X64-LABEL: cir.func no_inline dso_local @"?test_final_class@@YAXPEAUA@@@Z"(%arg0: !cir.ptr<!rec_A>
// BEFORE-X64: cir.delete_array %{{.*}} : !cir.ptr<!rec_A> {delete_fn = @"??_V@YAXPEAX_K@Z", delete_params = #cir.usual_delete_params<size = true>, element_dtor = @"??1A@@UEAA@XZ"}

// BEFORE-X86-LABEL: cir.func no_inline dso_local @"?test_final_class@@YAXPAUA@@@Z"(%arg0: !cir.ptr<!rec_A>
// BEFORE-X86: cir.delete_array %{{.*}} : !cir.ptr<!rec_A> {delete_fn = @"??_V@YAXPAXI@Z", delete_params = #cir.usual_delete_params<size = true>, element_dtor = @"??1A@@UAE@XZ"}

// CIR-X64-LABEL: cir.func no_inline dso_local @"?test_final_class@@YAXPEAUA@@@Z"(%arg0: !cir.ptr<!rec_A>
// CIR-X64-NOT: cir.vtable.get_vptr
// CIR-X64: cir.call @"??1A@@UEAA@XZ"(
// CIR-X64: cir.call @"??_V@YAXPEAX_K@Z"(

// CIR-X86-LABEL: cir.func no_inline dso_local @"?test_final_class@@YAXPAUA@@@Z"(%arg0: !cir.ptr<!rec_A>
// CIR-X86-NOT: cir.vtable.get_vptr
// CIR-X86: cir.call @"??1A@@UAE@XZ"(%{{.*}}) nothrow cc(x86_thiscall)
// CIR-X86: cir.call @"??_V@YAXPAXI@Z"(

// Case 2: Deleting array of class with final destructor devirtualizes destructor call.
// No vtable lookup is emitted; cir.delete_array emits loop calling ??1B and calls ??_V.
void test_final_dtor(B *b) {
  delete[] b;
}

// BEFORE-X64-LABEL: cir.func no_inline dso_local @"?test_final_dtor@@YAXPEAUB@@@Z"(%arg0: !cir.ptr<!rec_B>
// BEFORE-X64: cir.delete_array %{{.*}} : !cir.ptr<!rec_B> {delete_fn = @"??_V@YAXPEAX_K@Z", delete_params = #cir.usual_delete_params<size = true>, element_dtor = @"??1B@@UEAA@XZ"}

// BEFORE-X86-LABEL: cir.func no_inline dso_local @"?test_final_dtor@@YAXPAUB@@@Z"(%arg0: !cir.ptr<!rec_B>
// BEFORE-X86: cir.delete_array %{{.*}} : !cir.ptr<!rec_B> {delete_fn = @"??_V@YAXPAXI@Z", delete_params = #cir.usual_delete_params<size = true>, element_dtor = @"??1B@@UAE@XZ"}

// CIR-X64-LABEL: cir.func no_inline dso_local @"?test_final_dtor@@YAXPEAUB@@@Z"(%arg0: !cir.ptr<!rec_B>
// CIR-X64-NOT: cir.vtable.get_vptr
// CIR-X64: cir.call @"??1B@@UEAA@XZ"(
// CIR-X64: cir.call @"??_V@YAXPEAX_K@Z"(

// CIR-X86-LABEL: cir.func no_inline dso_local @"?test_final_dtor@@YAXPAUB@@@Z"(%arg0: !cir.ptr<!rec_B>
// CIR-X86-NOT: cir.vtable.get_vptr
// CIR-X86: cir.call @"??1B@@UAE@XZ"(%{{.*}}) nothrow cc(x86_thiscall)
// CIR-X86: cir.call @"??_V@YAXPAXI@Z"(

// Case 3: Pure virtual destructor does not devirtualize; retains virtual dispatch.
void test_pure_virtual(D *d) {
  delete[] d;
}

// CIR-X64-LABEL: cir.func no_inline dso_local @"?test_pure_virtual@@YAXPEAUD@@@Z"(%arg0: !cir.ptr<!rec_D>
// CIR-X64: %[[VPTR:.*]] = cir.vtable.get_vptr %{{.*}} : !cir.ptr<!rec_D> -> !cir.ptr<!cir.vptr>
// CIR-X64: %[[VTABLE:.*]] = cir.load align(8) %[[VPTR]]
// CIR-X64: %[[SLOT:.*]] = cir.vtable.get_virtual_fn_addr %[[VTABLE]][0]
// CIR-X64: %[[VFN:.*]] = cir.load align(8) %[[SLOT]]
// CIR-X64: cir.call %[[VFN]](%{{.*}}, %{{.*}})

// CIR-X86-LABEL: cir.func no_inline dso_local @"?test_pure_virtual@@YAXPAUD@@@Z"(%arg0: !cir.ptr<!rec_D>
// CIR-X86: %[[VPTR:.*]] = cir.vtable.get_vptr %{{.*}} : !cir.ptr<!rec_D> -> !cir.ptr<!cir.vptr>
// CIR-X86: %[[VTABLE:.*]] = cir.load align(4) %[[VPTR]]
// CIR-X86: %[[SLOT:.*]] = cir.vtable.get_virtual_fn_addr %[[VTABLE]][0]
// CIR-X86: %[[VFN:.*]] = cir.load align(4) %[[SLOT]]
// CIR-X86: cir.call %[[VFN]](%{{.*}}, %{{.*}})

// CIR-X64: cir.func no_inline comdat linkonce_odr dso_local @"??_ED@@UEAAPEAXI@Z"(%arg0: !cir.ptr<!rec_D> {{.*}}, %arg1: !s32i {{.*}}) -> (!cir.ptr<!void>
// CIR-X64: cir.trap

// CIR-X86: cir.func no_inline comdat linkonce_odr dso_local @"??_ED@@UAEPAXI@Z"(%arg0: !cir.ptr<!rec_D> {{.*}}, %arg1: !s32i {{.*}}) -> (!cir.ptr<!void>
// CIR-X86: cir.trap
