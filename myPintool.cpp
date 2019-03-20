#include <fstream>
#include <iostream>
#include "pin.H"

class PinMemAgent;

/* ===================================================================== */
/* Class Definitions */
/* ===================================================================== */

const size_t LenPage_k = 4 << 10;
const int ThresSync_k = 194;

// 通用内存池
template <typename T>
struct MemPool {
  friend PinMemAgent;

 private:
  T *addr;
  size_t len;
  int offset;

 public:
  MemPool(T *addr, size_t len, int offset)
      : addr(static_cast<T *>(addr)), len(len), offset(offset) {}

  // 给定地址是否在内存池地址范围内，自动内联
  bool addrInPool(T *addr) {
    return addr >= this->addr && addr < this->addr + this->len;
  }

  // 给定地址的逻辑页号（不考虑偏移），自动内联
  size_t addrToPageNum(T *addr) {
    return (addr - this->addr) * sizeof(T) / LenPage_k;
  }

  // 给定地址的逻辑页号（考虑偏移），自动内联
  size_t addrToPageNumReal(T *addr) {
    return (offset + addr - this->addr) * sizeof(T) / LenPage_k;
  }

  // 通过 [] 访问内存池
  T &operator[](int index) { return addr[index]; }
};

// 内存池代理
class PinMemAgent {
  MemPool<uint8_t> *AppMemPool_g;  // 托管的应用申请的内存池
  MemPool<uint32_t> *PinCntPool_g;  // 托管的 pin 维护的计数器的内存池

 public:
  PinMemAgent(ADDRINT *addr, ADDRINT len, ADDRINT offset) {
    AppMemPool_g = new MemPool<uint8_t>((uint8_t *)(addr), len, offset);
    len = len / LenPage_k;
    PinCntPool_g = new MemPool<uint32_t>(new uint32_t[len](), len, 0);
  }

  ~PinMemAgent() { delete PinCntPool_g->addr; }

  // 当前地址是否位于 app 地址池中？
  bool addrInAppPool(ADDRINT *addr) {
    return AppMemPool_g->addrInPool((uint8_t *)addr);
  }

  // 当前访问地址的写计数器是否抵达阈值？
  bool cntReachThres(ADDRINT *addr, ADDRINT size) {
    return ((*PinCntPool_g)[AppMemPool_g->addrToPageNum((uint8_t *)addr)] +=
            size) >= ThresSync_k;
  }

  // 将当前访问地址的写计数器同步到内核，并清空 Pin 维护的计数器
  void syncCnt(ADDRINT *addr) {
    // TODO-MKL: 考虑 unaligned mem acc
    // TODO-MKL: 通过 proc 同步
    (*PinCntPool_g)[AppMemPool_g->addrToPageNum((uint8_t *)addr)] = 0;
  }
};

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

std::ofstream TraceFile;

PinMemAgent *PinMemAgent_g = NULL;

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o",
                            "pmWtireTrace.out", "specify trace file name");

/* ===================================================================== */

/* ===================================================================== */
/* Analysis routines                                                     */
/* ===================================================================== */

VOID mmapBefore(ADDRINT *addr, ADDRINT length, ADDRINT offset) {
  if (PinMemAgent_g == NULL && addr != NULL)
    PinMemAgent_g = new PinMemAgent(addr, length, offset);
}

INT32 trigerCntSync(ADDRINT *addr, ADDRINT size) {
  return PinMemAgent_g && PinMemAgent_g->addrInAppPool(addr) &&
         PinMemAgent_g->cntReachThres(addr, size);
}

VOID syncCnt(ADDRINT *addr, ADDRINT size) { PinMemAgent_g->syncCnt(addr); }

/* ===================================================================== */
/* Instrumentation routines                                              */
/* ===================================================================== */

VOID Image(IMG img, VOID *v) {
  RTN mmapRtn = RTN_FindByName(img, "mmap");
  if (RTN_Valid(mmapRtn)) {
    RTN_Open(mmapRtn);

    RTN_InsertCall(mmapRtn, IPOINT_BEFORE, (AFUNPTR)mmapBefore,
                   IARG_FUNCARG_ENTRYPOINT_VALUE, 0,  // 传递 addr
                   IARG_FUNCARG_ENTRYPOINT_VALUE, 1,  // 传递 length
                   IARG_FUNCARG_ENTRYPOINT_VALUE, 5,  // 传递 offset
                   IARG_END                           // 结束参数传递
    );
    RTN_Close(mmapRtn);
  }
}

VOID Instruction(INS ins, VOID *v) {
  // 仅插桩 clflush 指令，
  // 因为 whisper 中所有对 PM 的写都需要通过 clflush 刷回
  OPCODE opcode = INS_Opcode(ins);
  if (opcode == XED_ICLASS_CLFLUSH || opcode == XED_ICLASS_CLFLUSHOPT ||
      opcode == XED_ICLASS_CLWB) {
    // 一个小小的优化：
    // 后者仅在前者返回非零的时候执行，以方便前者内联
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)trigerCntSync,
                     IARG_MEMORYREAD_EA,    // 传递地址
                     IARG_MEMORYREAD_SIZE,  // 传递粒度
                     IARG_END               // 结束参数传递
    );
    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)syncCnt,
                       IARG_MEMORYREAD_EA,    // 传递地址
                       IARG_MEMORYREAD_SIZE,  // 传递粒度
                       IARG_END               // 结束参数传递
    );
  }
  // TODO-MKL: 支持 non-temperal store 指令，如有需要
}

/* ===================================================================== */

VOID Fini(INT32 code, VOID *v) { TraceFile.close(); }

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32
Usage() {
  cerr << "This tool produces a trace of calls to malloc." << endl;
  cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
  return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[]) {
  // Initialize pin & symbol manager
  PIN_InitSymbols();
  if (PIN_Init(argc, argv)) {
    return Usage();
  }

  // Write to a file since cout and cerr maybe closed by the application
  TraceFile.open(KnobOutputFile.Value().c_str());
  TraceFile << hex;
  TraceFile.setf(ios::showbase);

  // Register Instruction/Image to be called to instrument functions.
  INS_AddInstrumentFunction(Instruction, 0);
  IMG_AddInstrumentFunction(Image, 0);
  PIN_AddFiniFunction(Fini, 0);

  // Never returns
  PIN_StartProgram();

  return 0;
}
