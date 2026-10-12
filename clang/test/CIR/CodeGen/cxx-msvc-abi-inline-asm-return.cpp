// RUN: %clang_cc1 %s -triple i686-pc-windows-msvc -emit-cir -fasm-blocks -fclangir -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s

// Check that we take EAX or EAX:EDX and return it from these functions for MSVC
// compatibility in ClangIR, verifying constraints (={eax}, =&{eax}, =A, =&A),
// stores into __retval, loads from __retval, and cir.return.

extern "C" {

long long f_i64() {
// CIR-LABEL: cir.func no_inline dso_local @f_i64() -> !s64i
// CIR: %[[RETVAL:.*]] = cir.alloca "__retval" align(8) : !cir.ptr<!s64i>
// CIR: %[[RET_CAST:.*]] = cir.cast bitcast %[[RETVAL]] : !cir.ptr<!s64i> -> !cir.ptr<!u64i>
// CIR: %[[RES:.*]] = cir.asm(x86_intel,
// CIR: {"mov eax, $$1\0A\09mov edx, $$1" "=&A,
// CIR: cir.store align(8) %[[RES]], %[[RET_CAST]] : !u64i, !cir.ptr<!u64i>
// CIR: %[[LOAD:.*]] = cir.load %[[RETVAL]] : !cir.ptr<!s64i>, !s64i
// CIR: cir.return %[[LOAD]] : !s64i
  __asm {
    mov eax, 1
    mov edx, 1
  }
}

long long f_i64_reverse() {
// CIR-LABEL: cir.func no_inline dso_local @f_i64_reverse() -> !s64i
// CIR: %[[RETVAL:.*]] = cir.alloca "__retval" align(8) : !cir.ptr<!s64i>
// CIR: %[[RET_CAST:.*]] = cir.cast bitcast %[[RETVAL]] : !cir.ptr<!s64i> -> !cir.ptr<!u64i>
// CIR: %[[RES:.*]] = cir.asm(x86_intel,
// CIR: {"mov edx, $$1\0A\09mov eax, $$1" "=&A,
// CIR: cir.store align(8) %[[RES]], %[[RET_CAST]] : !u64i, !cir.ptr<!u64i>
// CIR: %[[LOAD:.*]] = cir.load %[[RETVAL]] : !cir.ptr<!s64i>, !s64i
// CIR: cir.return %[[LOAD]] : !s64i
  __asm {
    mov edx, 1
    mov eax, 1
  }
}

int f_i32() {
// CIR-LABEL: cir.func no_inline dso_local @f_i32() -> !s32i
// CIR: %[[RETVAL:.*]] = cir.alloca "__retval" align(4) : !cir.ptr<!s32i>
// CIR: %[[RET_CAST:.*]] = cir.cast bitcast %[[RETVAL]] : !cir.ptr<!s32i> -> !cir.ptr<!u32i>
// CIR: %[[RES:.*]] = cir.asm(x86_intel,
// CIR: {"mov eax, $$1\0A\09mov edx, $$1" "=&{eax},~{edx},
// CIR: cir.store align(4) %[[RES]], %[[RET_CAST]] : !u32i, !cir.ptr<!u32i>
// CIR: %[[LOAD:.*]] = cir.load %[[RETVAL]] : !cir.ptr<!s32i>, !s32i
// CIR: cir.return %[[LOAD]] : !s32i
  __asm {
    mov eax, 1
    mov edx, 1
  }
}

int f_i32_reverse() {
// CIR-LABEL: cir.func no_inline dso_local @f_i32_reverse() -> !s32i
// CIR: %[[RETVAL:.*]] = cir.alloca "__retval" align(4) : !cir.ptr<!s32i>
// CIR: %[[RET_CAST:.*]] = cir.cast bitcast %[[RETVAL]] : !cir.ptr<!s32i> -> !cir.ptr<!u32i>
// CIR: %[[RES:.*]] = cir.asm(x86_intel,
// CIR: {"mov edx, $$1\0A\09mov eax, $$1" "=&{eax},~{edx},
// CIR: cir.store align(4) %[[RES]], %[[RET_CAST]] : !u32i, !cir.ptr<!u32i>
// CIR: %[[LOAD:.*]] = cir.load %[[RETVAL]] : !cir.ptr<!s32i>, !s32i
// CIR: cir.return %[[LOAD]] : !s32i
  __asm {
    mov edx, 1
    mov eax, 1
  }
}

int f_i32_edx() {
// CIR-LABEL: cir.func no_inline dso_local @f_i32_edx() -> !s32i
// CIR: %[[RETVAL:.*]] = cir.alloca "__retval" align(4) : !cir.ptr<!s32i>
// CIR: %[[RET_CAST:.*]] = cir.cast bitcast %[[RETVAL]] : !cir.ptr<!s32i> -> !cir.ptr<!u32i>
// CIR: %[[RES:.*]] = cir.asm(x86_intel,
// CIR: {"mov edx, $$1" "={eax},~{edx},
// CIR: cir.store align(4) %[[RES]], %[[RET_CAST]] : !u32i, !cir.ptr<!u32i>
// CIR: %[[LOAD:.*]] = cir.load %[[RETVAL]] : !cir.ptr<!s32i>, !s32i
// CIR: cir.return %[[LOAD]] : !s32i
  __asm {
    mov edx, 1
  }
}

short f_i16() {
// CIR-LABEL: cir.func no_inline dso_local @f_i16() -> !s16i
// CIR: %[[RETVAL:.*]] = cir.alloca "__retval" align(2) : !cir.ptr<!s16i>
// CIR: %[[RET_CAST:.*]] = cir.cast bitcast %[[RETVAL]] : !cir.ptr<!s16i> -> !cir.ptr<!u16i>
// CIR: %[[RES:.*]] = cir.asm(x86_intel,
// CIR: {"mov eax, $$1\0A\09mov edx, $$1" "=&{eax},~{edx},
// CIR: %[[TRUNC:.*]] = cir.cast integral %[[RES]] : !u32i -> !u16i
// CIR: cir.store align(2) %[[TRUNC]], %[[RET_CAST]] : !u16i, !cir.ptr<!u16i>
// CIR: %[[LOAD:.*]] = cir.load %[[RETVAL]] : !cir.ptr<!s16i>, !s16i
// CIR: cir.return %[[LOAD]] : !s16i
  __asm {
    mov eax, 1
    mov edx, 1
  }
}

char f_i8() {
// CIR-LABEL: cir.func no_inline dso_local @f_i8() -> !s8i
// CIR: %[[RETVAL:.*]] = cir.alloca "__retval" align(1) : !cir.ptr<!s8i>
// CIR: %[[RET_CAST:.*]] = cir.cast bitcast %[[RETVAL]] : !cir.ptr<!s8i> -> !cir.ptr<!u8i>
// CIR: %[[RES:.*]] = cir.asm(x86_intel,
// CIR: {"mov eax, $$1\0A\09mov edx, $$1" "=&{eax},~{edx},
// CIR: %[[TRUNC:.*]] = cir.cast integral %[[RES]] : !u32i -> !u8i
// CIR: cir.store align(1) %[[TRUNC]], %[[RET_CAST]] : !u8i, !cir.ptr<!u8i>
// CIR: %[[LOAD:.*]] = cir.load %[[RETVAL]] : !cir.ptr<!s8i>, !s8i
// CIR: cir.return %[[LOAD]] : !s8i
  __asm {
    mov eax, 1
    mov edx, 1
  }
}

bool f_i1() {
// CIR-LABEL: cir.func no_inline dso_local @f_i1() -> !cir.bool
// CIR: %[[RETVAL:.*]] = cir.alloca "__retval" align(1) : !cir.ptr<!cir.bool>
// CIR: %[[RET_CAST:.*]] = cir.cast bitcast %[[RETVAL]] : !cir.ptr<!cir.bool> -> !cir.ptr<!u8i>
// CIR: %[[RES:.*]] = cir.asm(x86_intel,
// CIR: {"mov eax, $$1\0A\09mov edx, $$1" "=&{eax},~{edx},
// CIR: %[[TRUNC:.*]] = cir.cast integral %[[RES]] : !u32i -> !u8i
// CIR: cir.store align(1) %[[TRUNC]], %[[RET_CAST]] : !u8i, !cir.ptr<!u8i>
// CIR: %[[LOAD:.*]] = cir.load %[[RETVAL]] : !cir.ptr<!cir.bool>, !cir.bool
// CIR: cir.return %[[LOAD]] : !cir.bool
  __asm {
    mov eax, 1L
    mov edx, 1U
  }
}

struct FourChars {
  char a, b, c, d;
};
FourChars f_s4() {
// CIR-LABEL: cir.func no_inline dso_local @f_s4() -> !rec_FourChars
// CIR: %[[RETVAL:.*]] = cir.alloca "__retval" align(1) : !cir.ptr<!rec_FourChars>
// CIR: %[[RET_CAST:.*]] = cir.cast bitcast %[[RETVAL]] : !cir.ptr<!rec_FourChars> -> !cir.ptr<!u32i>
// CIR: %[[RES:.*]] = cir.asm(x86_intel,
// CIR: {"mov eax, $$16843009" "=&{eax},
// CIR: cir.store align(1) %[[RES]], %[[RET_CAST]] : !u32i, !cir.ptr<!u32i>
// CIR: %[[LOAD:.*]] = cir.load %[[RETVAL]] : !cir.ptr<!rec_FourChars>, !rec_FourChars
// CIR: cir.return %[[LOAD]] : !rec_FourChars
  __asm {
    mov eax, 0x01010101
  }
}

struct EightChars {
  char a, b, c, d, e, f, g, h;
};
EightChars f_s8() {
// CIR-LABEL: cir.func no_inline dso_local @f_s8() -> !rec_EightChars
// CIR: %[[RETVAL:.*]] = cir.alloca "__retval" align(1) : !cir.ptr<!rec_EightChars>
// CIR: %[[RET_CAST:.*]] = cir.cast bitcast %[[RETVAL]] : !cir.ptr<!rec_EightChars> -> !cir.ptr<!u64i>
// CIR: %[[RES:.*]] = cir.asm(x86_intel,
// CIR: {"mov eax, $$16843009\0A\09mov edx, $$85" "=&A,
// CIR: cir.store align(1) %[[RES]], %[[RET_CAST]] : !u64i, !cir.ptr<!u64i>
// CIR: %[[LOAD:.*]] = cir.load %[[RETVAL]] : !cir.ptr<!rec_EightChars>, !rec_EightChars
// CIR: cir.return %[[LOAD]] : !rec_EightChars
  __asm {
    mov eax, 01010101h
    mov edx, 01010101b
  }
}

} // extern "C"

int main() {
// CIR-LABEL: cir.func no_inline dso_local @main() -> (!s32i {llvm.noundef})
// CIR: %[[RETVAL:.*]] = cir.alloca "__retval" align(4) : !cir.ptr<!s32i>
// CIR: %[[RET_CAST:.*]] = cir.cast bitcast %[[RETVAL]] : !cir.ptr<!s32i> -> !cir.ptr<!u32i>
// CIR: %[[RES:.*]] = cir.asm(x86_intel,
// CIR: {"xor eax, eax" "=&{eax},
// CIR: cir.store align(4) %[[RES]], %[[RET_CAST]] : !u32i, !cir.ptr<!u32i>
// CIR: %[[LOAD:.*]] = cir.load %[[RETVAL]] : !cir.ptr<!s32i>, !s32i
// CIR: cir.return %[[LOAD]] : !s32i
  __asm xor eax, eax
}
