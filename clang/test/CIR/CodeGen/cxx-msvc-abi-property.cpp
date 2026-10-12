// RUN: %clang_cc1 -std=c++17 -triple x86_64-pc-windows-msvc -fms-extensions -fms-compatibility -fclangir -emit-cir %s -o %t64.cir
// RUN: FileCheck --input-file=%t64.cir --check-prefix=CIR %s

// Test __declspec(property) getter and setter syntax lowering to ClangIR.

struct Simple {
  int val_;

  __declspec(property(get = get_x, put = put_x)) int x;
  int get_x() const { return val_; }
  void put_x(int v) { const_cast<Simple *>(this)->val_ = v; }
};

struct S {
  __declspec(property(get = GetX, put = PutX)) int x[];
  int GetX(int i, int j) { return i + j; }
  void PutX(int i, int j, int k) { (void)(i + j + k); }

  __declspec(property(get = GetY, put = PutY)) int y[][];
  int GetY(int i, int j) { return i + j; }
  void PutY(int i, int j, int k) { (void)(i + j + k); }

  __declspec(property(get = GetZ, put = PutZ)) int z[][][];
  int GetZ(int i, int j, int k) { return i + j + k; }
  void PutZ(int i, int j, int k, int v) { (void)(i + j + k + v); }
};

struct Agg {
  int a;
  int b;
};

struct WithAgg {
  Agg agg_;

  __declspec(property(get = get_agg, put = put_agg)) Agg my_agg;
  Agg get_agg() const { return agg_; }
  void put_agg(Agg a) { const_cast<WithAgg *>(this)->agg_ = a; }
};

class Test1 {
  int x_;
public:
  Test1(int x) : x_(x) {}
  __declspec(property(get = get_x)) int X;
  int get_x() const { return x_; }
  static Test1 *GetTest1() { return nullptr; }
};

int idx() { return 7; }

// CIR-LABEL: cir.func no_inline dso_local @"?test_get_set@@YAXPEAUSimple@@@Z"
void test_get_set(Simple *s) {
  // CIR: %[[S:.*]] = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_Simple>>, !cir.ptr<!rec_Simple>
  // CIR: %[[GET:.*]] = cir.call @"?get_x@Simple@@QEBAHXZ"(%[[S]]) : (!cir.ptr<!rec_Simple>{{.*}}) -> (!s32i{{.*}})
  // CIR: cir.store align(4) %[[GET]], %[[VAL_ADDR:.*]] : !s32i, !cir.ptr<!s32i>
  int val = s->x;

  // CIR: %[[S2:.*]] = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_Simple>>, !cir.ptr<!rec_Simple>
  // CIR: %[[VAL:.*]] = cir.load align(4) %[[VAL_ADDR]] : !cir.ptr<!s32i>, !s32i
  // CIR: cir.call @"?put_x@Simple@@QEAAXH@Z"(%[[S2]], %[[VAL]]) : (!cir.ptr<!rec_Simple>{{.*}}, !s32i{{.*}}) -> ()
  s->x = val;
}

// CIR-LABEL: cir.func no_inline dso_local @"?test_compound@@YAXPEAUSimple@@@Z"
void test_compound(Simple *s) {
  // CIR: %[[S1:.*]] = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_Simple>>, !cir.ptr<!rec_Simple>
  // CIR: %[[CUR:.*]] = cir.call @"?get_x@Simple@@QEBAHXZ"(%[[S1]]) : (!cir.ptr<!rec_Simple>{{.*}}) -> (!s32i{{.*}})
  // CIR: %[[ADD:.*]] = cir.add %[[CUR]], %{{.*}} : !s32i
  // CIR: cir.call @"?put_x@Simple@@QEAAXH@Z"(%[[S1]], %[[ADD]]) : (!cir.ptr<!rec_Simple>{{.*}}, !s32i{{.*}}) -> ()
  s->x += 5;

  // CIR: %[[S2:.*]] = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_Simple>>, !cir.ptr<!rec_Simple>
  // CIR: %[[CUR2:.*]] = cir.call @"?get_x@Simple@@QEBAHXZ"(%[[S2]]) : (!cir.ptr<!rec_Simple>{{.*}}) -> (!s32i{{.*}})
  // CIR: %[[INC:.*]] = cir.add %[[CUR2]], %{{.*}} : !s32i
  // CIR: cir.call @"?put_x@Simple@@QEAAXH@Z"(%[[S2]], %[[INC]]) : (!cir.ptr<!rec_Simple>{{.*}}, !s32i{{.*}}) -> ()
  ++s->x;
}

// CIR-LABEL: cir.func no_inline dso_local @"?test_indexed@@YAXPEAUS@@@Z"
void test_indexed(S *s) {
  // CIR: %[[S:.*]] = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_S>>, !cir.ptr<!rec_S>
  // CIR: %[[GET_X:.*]] = cir.call @"?GetX@S@@QEAAHHH@Z"(%[[S]], %{{.*}}, %{{.*}}) : (!cir.ptr<!rec_S>{{.*}}, !s32i{{.*}}, !s32i{{.*}}) -> (!s32i{{.*}})
  // CIR: cir.store align(4) %[[GET_X]], %[[J_ADDR:.*]] : !s32i, !cir.ptr<!s32i>
  int j = s->x[223][11];

  // CIR: %[[S2:.*]] = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_S>>, !cir.ptr<!rec_S>
  // CIR: %[[J_VAL:.*]] = cir.load align(4) %[[J_ADDR]] : !cir.ptr<!s32i>, !s32i
  // CIR: cir.call @"?PutX@S@@QEAAXHHH@Z"(%[[S2]], %{{.*}}, %{{.*}}, %[[J_VAL]]) : (!cir.ptr<!rec_S>{{.*}}, !s32i{{.*}}, !s32i{{.*}}, !s32i{{.*}}) -> ()
  s->x[23][1] = j;

  // CIR: %[[GET_Y:.*]] = cir.call @"?GetY@S@@QEAAHHH@Z"(%{{.*}}, %{{.*}}, %{{.*}}) : (!cir.ptr<!rec_S>{{.*}}, !s32i{{.*}}, !s32i{{.*}}) -> (!s32i{{.*}})
  // CIR: cir.store align(4) %[[GET_Y]], %[[K_ADDR:.*]] : !s32i, !cir.ptr<!s32i>
  int k = s->y[123][22];

  // CIR: %[[K_VAL:.*]] = cir.load align(4) %[[K_ADDR]] : !cir.ptr<!s32i>, !s32i
  // CIR: cir.call @"?PutY@S@@QEAAXHHH@Z"(%{{.*}}, %{{.*}}, %{{.*}}, %[[K_VAL]])
  s->y[16][2] = k;

  // CIR: %[[GET_Z:.*]] = cir.call @"?GetZ@S@@QEAAHHHH@Z"(%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}) : (!cir.ptr<!rec_S>{{.*}}, !s32i{{.*}}, !s32i{{.*}}, !s32i{{.*}}) -> (!s32i{{.*}})
  // CIR: cir.store align(4) %[[GET_Z]], %[[K_ADDR]] : !s32i, !cir.ptr<!s32i>
  k = s->z[123][22][44];

  // CIR: %[[K_VAL2:.*]] = cir.load align(4) %[[K_ADDR]] : !cir.ptr<!s32i>, !s32i
  // CIR: cir.call @"?PutZ@S@@QEAAXHHHH@Z"(%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}, %[[K_VAL2]])
  s->z[16][2][32] = k;
}

// CIR-LABEL: cir.func no_inline dso_local @"?test_indexed_compound@@YAXPEAUS@@@Z"
void test_indexed_compound(S *s) {
  // Ensure idx() is called once, then GetX, add 1, then PutX with the result
  // CIR: %[[S:.*]] = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_S>>, !cir.ptr<!rec_S>
  // CIR: %[[IDX:.*]] = cir.call @"?idx@@YAHXZ"() : () -> (!s32i{{.*}})
  // CIR: %[[CUR:.*]] = cir.call @"?GetX@S@@QEAAHHH@Z"(%[[S]], %[[IDX]], %{{.*}}) : (!cir.ptr<!rec_S>{{.*}}, !s32i{{.*}}, !s32i{{.*}}) -> (!s32i{{.*}})
  // CIR: %[[INC:.*]] = cir.add %[[CUR]], %{{.*}} : !s32i
  // CIR: cir.call @"?PutX@S@@QEAAXHHH@Z"(%[[S]], %[[IDX]], %{{.*}}, %[[INC]]) : (!cir.ptr<!rec_S>{{.*}}, !s32i{{.*}}, !s32i{{.*}}, !s32i{{.*}}) -> ()
  ++s->x[idx()][1];
}

// CIR-LABEL: cir.func no_inline dso_local @"?test_aggregate@@YAXPEAUWithAgg@@@Z"
void test_aggregate(WithAgg *w) {
  // CIR: %[[W:.*]] = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_WithAgg>>, !cir.ptr<!rec_WithAgg>
  // CIR: cir.call @"?get_agg@WithAgg@@QEBA?AUAgg@@XZ"(%[[W]], %{{.*}}) : (!cir.ptr<!rec_WithAgg>{{.*}}, !cir.ptr<!rec_Agg>) -> ()
  Agg a = w->my_agg;

  // CIR: %[[W2:.*]] = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_WithAgg>>, !cir.ptr<!rec_WithAgg>
  // CIR: cir.call @"?put_agg@WithAgg@@QEAAXUAgg@@@Z"(%[[W2]], %{{.*}}) : (!cir.ptr<!rec_WithAgg>{{.*}}, !u64i) -> ()
  w->my_agg = a;
}

// CIR-LABEL: cir.func no_inline dso_local @"?test_static_call_property@@YAHXZ"
int test_static_call_property() {
  // CIR: %[[CALL:.*]] = cir.call @"?GetTest1@Test1@@SAPEAV1@XZ"() : () -> (!cir.ptr<!rec_Test1>{{.*}})
  // CIR: %[[GET:.*]] = cir.call @"?get_x@Test1@@QEBAHXZ"(%[[CALL]]) : (!cir.ptr<!rec_Test1>{{.*}}) -> (!s32i{{.*}})
  // CIR: cir.store %[[GET]], %{{.*}} : !s32i, !cir.ptr<!s32i>
  // CIR: cir.return %{{.*}} : !s32i
  return Test1::GetTest1()->X;
}

struct WithReturnSetter {
  int val_;
  __declspec(property(get = get_x, put = put_x)) int x;
  int get_x() const { return val_; }
  int put_x(int v) { const_cast<WithReturnSetter *>(this)->val_ = v; return v; }
};

// CIR-LABEL: cir.func no_inline dso_local @"?test_chained_assignment@@YAXPEAUWithReturnSetter@@@Z"
void test_chained_assignment(WithReturnSetter *s) {
  // CIR: %[[TEN:.*]] = cir.const #cir.int<10> : !s32i
  // CIR: %[[RET:.*]] = cir.call @"?put_x@WithReturnSetter@@QEAAHH@Z"(%{{.*}}, %[[TEN]])
  // CIR: cir.store align(4) %[[RET]], %[[A_ADDR:.*]] : !s32i, !cir.ptr<!s32i>
  int a;
  a = s->x = 10;
}

template <typename T>
class St {
public:
  __declspec(property(get = GetX, put = PutX)) T x[];
  T GetX(T i, T j) { return i + j; }
  void PutX(T i, T j, T k) { (void)(i + j + k); }
};

// CIR-LABEL: cir.func no_inline dso_local @"?test_template@@YAXPEAV?$St@M@@@Z"
void test_template(St<float> *p) {
  // CIR: %[[GET_F:.*]] = cir.call @"?GetX@?$St@M@@QEAAMMM@Z"(%{{.*}}, %{{.*}}, %{{.*}}) : (!cir.ptr<!rec_St3Cfloat3E>{{.*}}, !cir.float{{.*}}, !cir.float{{.*}}) -> (!cir.float{{.*}})
  float val = p->x[223.0f][11.0f];
  // CIR: cir.call @"?PutX@?$St@M@@QEAAXMMM@Z"(%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}})
  p->x[23.0f][1.0f] = val;
}

// CIR-LABEL: cir.func no_inline dso_local @"?test_nested_property@@YAXPEAUSimple@@PEAUS@@@Z"
void test_nested_property(Simple *s, S *p) {
  // CIR: %[[GET_INNER:.*]] = cir.call @"?get_x@Simple@@QEBAHXZ"(%{{.*}}) : (!cir.ptr<!rec_Simple>{{.*}}) -> (!s32i{{.*}})
  // CIR: cir.call @"?PutX@S@@QEAAXHHH@Z"(%{{.*}}, %[[GET_INNER]], %{{.*}}, %{{.*}})
  p->x[s->x][1] = 42;
}

