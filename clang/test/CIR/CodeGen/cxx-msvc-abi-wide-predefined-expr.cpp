// RUN: %clang_cc1 %s -fms-extensions -triple i686-pc-win32 -fclangir -emit-cir -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s

// Check wide predefined expressions (L__FUNCTION__ and L__FUNCSIG__) in ClangIR,
// lowering to wide string literal globals of type !cir.array<!u16i x N>.

void wprint(const wchar_t*);

#define __STR2WSTR(str) L##str
#define _STR2WSTR(str) __STR2WSTR(str)
#define STR2WSTR(str) _STR2WSTR(str)

// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_C@_19DPFBEKIN@?$AAf?$AAu?$AAn?$AAc?$AA?$AA@" = #cir.const_array<[#cir.int<102> : !u16i, #cir.int<117> : !u16i, #cir.int<110> : !u16i, #cir.int<99> : !u16i], trailing_zeros> : !cir.array<!u16i x 5>
// CIR-DAG: cir.global "private" constant linkonce_odr comdat dso_local @"??_C@_1DA@GPJPIIGA@?$AAv?$AAo?$AAi?$AAd?$AA?5?$AA_?$AA_?$AAc?$AAd?$AAe?$AAc?$AAl?$AA?5?$AAf?$AAu?$AAn?$AAc?$AA?$CI?$AAv?$AAo?$AAi?$AAd?$AA?$CJ?$AA?$AA@" = #cir.const_array<[{{.*}}], trailing_zeros> : !cir.array<!u16i x 24>

void func() {
// CIR-LABEL: cir.func no_inline dso_local @"?func@@YAXXZ"()
// CIR: %[[FN_STR:.*]] = cir.get_global @"??_C@_19DPFBEKIN@?$AAf?$AAu?$AAn?$AAc?$AA?$AA@" : !cir.ptr<!cir.array<!u16i x 5>>
// CIR: %[[FN_DECAY:.*]] = cir.cast array_to_ptrdecay %[[FN_STR]] : !cir.ptr<!cir.array<!u16i x 5>> -> !cir.ptr<!u16i>
// CIR: cir.call @"?wprint@@YAXPB_W@Z"(%[[FN_DECAY]])
// CIR: %[[SIG_STR:.*]] = cir.get_global @"??_C@_1DA@GPJPIIGA@?$AAv?$AAo?$AAi?$AAd?$AA?5?$AA_?$AA_?$AAc?$AAd?$AAe?$AAc?$AAl?$AA?5?$AAf?$AAu?$AAn?$AAc?$AA?$CI?$AAv?$AAo?$AAi?$AAd?$AA?$CJ?$AA?$AA@" : !cir.ptr<!cir.array<!u16i x 24>>
// CIR: %[[SIG_DECAY:.*]] = cir.cast array_to_ptrdecay %[[SIG_STR]] : !cir.ptr<!cir.array<!u16i x 24>> -> !cir.ptr<!u16i>
// CIR: cir.call @"?wprint@@YAXPB_W@Z"(%[[SIG_DECAY]])
// CIR: cir.return
  wprint(STR2WSTR(__FUNCTION__));
  wprint(STR2WSTR(__FUNCSIG__));
}

int main() {
// CIR-LABEL: cir.func no_inline dso_local @main()
// CIR: cir.call @"?func@@YAXXZ"()
// CIR: cir.return
  func();
  return 0;
}
