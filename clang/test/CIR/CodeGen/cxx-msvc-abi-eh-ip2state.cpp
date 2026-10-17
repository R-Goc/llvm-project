// REQUIRES: x86-registered-target
// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fexceptions -fcxx-exceptions -fclangir -emit-cir %s -o - | FileCheck %s --check-prefix=CIR
// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fexceptions -fcxx-exceptions -fclangir -emit-llvm %s -o - | FileCheck %s --check-prefix=LLVM
// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fexceptions -fcxx-exceptions -fclangir -S %s -o - | FileCheck %s --check-prefix=ASM

// CIR-DAG: cir.global "private" constant external @"??_R0?AUBadError@@@8" : !cir.ptr<!void>

// LLVM-DAG: @"??_R0?AUBadError@@@8" = external constant ptr

void might_throw();
void other_func(int x);

class HasDtor {
    int x;
public:
    explicit HasDtor(int x);
    ~HasDtor();
};

struct BadError {
    int errorCode;
};

// CIR-LABEL: cir.func no_inline dso_local @"?case_try_catch@@YAHXZ"() -> (!s32i {{.*}}) personality(@__CxxFrameHandler3)
// CIR:         cir.try {
// CIR:           cir.call @"??0HasDtor@@QEAA@H@Z"
// CIR:           cir.cleanup.scope {
// CIR:             cir.call @"?other_func@@YAXH@Z"
// CIR:             cir.yield
// CIR:           } cleanup all {
// CIR:             cir.call @"??1HasDtor@@QEAA@XZ"
// CIR:             cir.yield
// CIR:           }
// CIR:           cir.yield
// CIR:         } catch [type #cir.global_view<@"??_R0?AUBadError@@@8"> : !cir.ptr<!void>]

// LLVM-LABEL: define dso_local noundef i32 @"?case_try_catch@@YAHXZ"() {{.*}} personality ptr @__CxxFrameHandler3 {
// LLVM:         invoke void @"?other_func@@YAXH@Z"(
// LLVM:                 to label %{{.*}} unwind label %[[CLEANUP_BB:.*]]
// LLVM:       [[CLEANUP_BB]]:
// LLVM-NEXT:    %[[CLEANUP_PAD:.*]] = cleanuppad within none []
// LLVM-NEXT:    call void @"??1HasDtor@@QEAA@XZ"({{.*}}) {{.*}} [ "funclet"(token %[[CLEANUP_PAD]]) ]
// LLVM-NEXT:    cleanupret from %[[CLEANUP_PAD]] unwind {{.*}}label %[[CS_BB:.*]]
// LLVM:       [[CS_BB]]:
// LLVM-NEXT:    %[[CS:.*]] = catchswitch within none [label %[[CP_BB:.*]]] unwind to caller
// LLVM:       [[CP_BB]]:
// LLVM-NEXT:    %[[CP:.*]] = catchpad within %[[CS]] [ptr @"??_R0?AUBadError@@@8", i32 8, ptr %{{.*}}]
// LLVM:         catchret from %[[CP]] to label %{{.*}}

// ASM-LABEL: .def "?case_try_catch@@YAHXZ"
// ASM:       "$cppxdata$?case_try_catch@@YAHXZ":
// ASM:       "$stateUnwindMap$?case_try_catch@@YAHXZ":
// ASM:       "$ip2state$?case_try_catch@@YAHXZ":
int case_try_catch() {
    int result = 0;
    try {
        HasDtor hd{20};
        other_func(10);
    } catch (BadError &e) {
        result = 1;
    }
    return result;
}

// CIR-LABEL: cir.func no_inline dso_local @"?normal_has_regions@@YAXXZ"()
// CIR:         cir.scope {
// CIR:           cir.call @"??0HasDtor@@QEAA@H@Z"
// CIR:           cir.cleanup.scope {
// CIR:             cir.call @"?might_throw@@YAXXZ"
// CIR:             cir.yield
// CIR:           } cleanup all {
// CIR:             cir.call @"??1HasDtor@@QEAA@XZ"
// CIR:             cir.yield
// CIR:           }
// CIR:         }
// CIR:         cir.call @"?other_func@@YAXH@Z"
// CIR:         cir.return

// LLVM-LABEL: define dso_local void @"?normal_has_regions@@YAXXZ"() {{.*}} personality ptr @__CxxFrameHandler3 {
// LLVM:         invoke void @"?might_throw@@YAXXZ"()
// LLVM:                 to label %{{.*}} unwind label %[[CLEANUP_BB:.*]]
// LLVM:       [[CLEANUP_BB]]:
// LLVM-NEXT:    %[[CLEANUP_PAD:.*]] = cleanuppad within none []
// LLVM-NEXT:    call void @"??1HasDtor@@QEAA@XZ"({{.*}}) {{.*}} [ "funclet"(token %[[CLEANUP_PAD]]) ]
// LLVM-NEXT:    cleanupret from %[[CLEANUP_PAD]] unwind to caller

// ASM-LABEL: .def "?normal_has_regions@@YAXXZ"
// ASM:       "$ip2state$?normal_has_regions@@YAXXZ":
void normal_has_regions() {
    {
        HasDtor hd{42};
        might_throw();
    }
    other_func(10);
}
