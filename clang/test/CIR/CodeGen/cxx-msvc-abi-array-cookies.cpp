// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++17 -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s

// Adapted from Clang CodeGenCXX/microsoft-abi-array-cookies.cpp.

//===----------------------------------------------------------------------===//
// Section 1: Types without destructor (no array cookie)
//===----------------------------------------------------------------------===//

struct ClassWithoutDtor {
  char x;
};

void check_array_no_cookies() {
  ClassWithoutDtor *array = new ClassWithoutDtor[42];
  delete [] array;
}

// In MSVC ABI, types without a destructor do not require array cookies.
// 42 elements * 1 byte = 42 bytes allocated directly without cookie overhead.
// CIR-LABEL: cir.func {{.*}} @"?check_array_no_cookies@@YAXXZ"()
// CIR:   %[[SIZE:.*]] = cir.const #cir.int<42> : !u64i
// CIR:   %[[ALLOC:.*]] = cir.call @"??_U@YAPEAX_K@Z"(%[[SIZE]])
// CIR:   cir.call @"??_V@YAXPEAX@Z"(%{{.*}})

//===----------------------------------------------------------------------===//
// Section 2: Types with non-virtual destructor (standard array cookie)
//===----------------------------------------------------------------------===//

struct ClassWithDtor {
  char x;
  ~ClassWithDtor() {}
};

void check_array_cookies_simple() {
  ClassWithDtor *array = new ClassWithDtor[42];
  delete [] array;
}

// In MSVC ABI (x64), types with a destructor require an array cookie.
// Cookie size is max(sizeof(size_t), alignof(T)) = max(8, 1) = 8 bytes.
// Total allocation = 42 elements * 1 byte + 8 cookie bytes = 50 bytes.
// CIR-LABEL: cir.func {{.*}} @"?check_array_cookies_simple@@YAXXZ"()
// CIR:   %[[COUNT:.*]] = cir.const #cir.int<42> : !u64i
// CIR:   %[[SIZE:.*]] = cir.const #cir.int<50> : !u64i
// CIR:   %[[ALLOC:.*]] = cir.call @"??_U@YAPEAX_K@Z"(%[[SIZE]])
// CIR:   cir.store {{.*}}%[[COUNT]], %[[COOKIE_PTR:.*]] : !u64i, !cir.ptr<!u64i>
// CIR:   %[[OFFSET:.*]] = cir.const #cir.int<8> : !s32i
// CIR:   %[[ARRAY:.*]] = cir.ptr_stride %{{.*}}, %[[OFFSET]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>
// CIR:   %[[NEG_OFFSET:.*]] = cir.const #cir.int<-8> : !s64i
// CIR:   %[[BASE_ALLOC:.*]] = cir.ptr_stride %{{.*}}, %[[NEG_OFFSET]] : (!cir.ptr<!u8i>, !s64i) -> !cir.ptr<!u8i>
// CIR:   cir.call @"??1ClassWithDtor@@QEAA@XZ"
// CIR:   cir.call @"??_V@YAXPEAX_K@Z"

//===----------------------------------------------------------------------===//
// Section 3: Types with alignment requirements (padded array cookie)
//===----------------------------------------------------------------------===//

struct alignas(16) ClassWithAlignment {
  int *x;
  ~ClassWithAlignment() {}
};

void check_array_cookies_aligned() {
  ClassWithAlignment *array = new ClassWithAlignment[42];
  delete [] array;
}

// In MSVC ABI (x64), alignment requirements pad the array cookie size.
// Cookie size is max(sizeof(size_t), alignof(T)) = max(8, 16) = 16 bytes.
// Total allocation = 42 elements * 16 bytes + 16 cookie bytes = 688 bytes.
// CIR-LABEL: cir.func {{.*}} @"?check_array_cookies_aligned@@YAXXZ"()
// CIR:   %[[COUNT:.*]] = cir.const #cir.int<42> : !u64i
// CIR:   %[[SIZE:.*]] = cir.const #cir.int<688> : !u64i
// CIR:   %[[ALLOC:.*]] = cir.call @"??_U@YAPEAX_K@Z"(%[[SIZE]])
// CIR:   cir.store {{.*}}%[[COUNT]], %[[COOKIE_PTR:.*]] : !u64i, !cir.ptr<!u64i>
// CIR:   %[[OFFSET:.*]] = cir.const #cir.int<16> : !s32i
// CIR:   %[[ARRAY:.*]] = cir.ptr_stride %{{.*}}, %[[OFFSET]] : (!cir.ptr<!u8i>, !s32i) -> !cir.ptr<!u8i>
// CIR:   cir.call @"??1ClassWithAlignment@@QEAA@XZ"
// CIR:   cir.call @"??_V@YAXPEAX_K@Z"
