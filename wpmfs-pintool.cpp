#include <assert.h>
#include <fstream>
#include <iostream>

#include "pin.H"

/* ===================================================================== */
/* Class Declarations */
/* ===================================================================== */
 
class PinMemAgent;
class IOController;

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

const size_t LenPage_k = 4 << 10;
const int ThresSync_k = 194;
const int BufSize_k = 128;

std::ofstream TraceFile;
PinMemAgent *PinMemAgent_g = NULL;
IOController *IOController_g = NULL;

/* ===================================================================== */
/* Class Definitions */
/* ===================================================================== */

// io controller
class IOController {
  NATIVE_FD nfd;
#pragma pack(1)  // 统一不加 padding
  struct {
    int fd;
    uint32_t opcode;
    uint64_t pageoff;
    uint64_t cnt;
  } message;
#pragma pack()

  enum opcpde { WPMFS_INC_CNT = 0xBCD00020, WPMFS_GET_CNT = 0xBCD00021 };

 public:
  IOController(int fd) {
    OS_RETURN_CODE ret;
    message.fd = fd;
    // 以读写模式打开 proc 文件，该文件必须已存在
    ret = OS_OpenFD("/proc/wpmfs_proc",
                    OS_FILE_OPEN_TYPE_READ | OS_FILE_OPEN_TYPE_WRITE |
                        OS_FILE_OPEN_TYPE_TRUNCATE,
                    0, &nfd);
    assert(ret.generic_err == OS_RETURN_CODE_NO_ERROR);
  }

  ~IOController() { OS_CloseFD(nfd); }

  void incCnt(uint64_t pageoff, uint64_t cnt) {
    OS_RETURN_CODE ret;
    USIZE len = sizeof(message);

    /* 封装请求 */
    message.opcode = WPMFS_INC_CNT;
    message.pageoff = pageoff;
    message.cnt = cnt;

    /* 发送请求 */
    ret = OS_WriteFD(nfd, &message, &len);
    assert(ret.generic_err == OS_RETURN_CODE_NO_ERROR);

    getCnt(pageoff);
  }

  size_t getCnt(uint64_t pageoff) {
    OS_RETURN_CODE ret;
    USIZE len = sizeof(message) - sizeof(message.cnt);
    message.opcode = WPMFS_GET_CNT;
    message.pageoff = pageoff;

    /* 发送请求 */
    ret = OS_WriteFD(nfd, &message, &len);
    assert(ret.generic_err == OS_RETURN_CODE_NO_ERROR);

    /* 读取响应 */
    len = sizeof(message.cnt);
    ret = OS_ReadFD(nfd, &len, &message.cnt);
    assert(ret.generic_err == OS_RETURN_CODE_NO_ERROR);

    return message.cnt;
  }
};

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
  MemPool<uint64_t> *PinCntPool_g;  // 托管的 pin 维护的计数器的内存池

 public:
  PinMemAgent(ADDRINT *addr, ADDRINT len, ADDRINT offset) {
    AppMemPool_g = new MemPool<uint8_t>((uint8_t *)(addr), len, offset);
    len = len / LenPage_k;
    PinCntPool_g = new MemPool<uint64_t>(new uint64_t[len](), len, 0);
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
    uint64_t pageOff = AppMemPool_g->addrToPageNum((uint8_t *)addr);
    uint64_t &writeCnt =
        (*PinCntPool_g)[AppMemPool_g->addrToPageNum((uint8_t *)addr)];
    IOController_g->incCnt(pageOff, writeCnt);
    writeCnt = 0;
  }
};

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o",
                            "pmWtireTrace.out", "specify trace file name");

/* ===================================================================== */

/* ===================================================================== */
/* Analysis routines                                                     */
/* ===================================================================== */

VOID mmapBefore(ADDRINT *addr, ADDRINT length, ADDRINT fd, ADDRINT offset) {
  if (PinMemAgent_g == NULL && addr != NULL && (int)fd != -1) {
    PinMemAgent_g = new PinMemAgent(addr, length, offset);
    IOController_g = new IOController(fd);
  }
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
                   IARG_FUNCARG_ENTRYPOINT_VALUE, 4,  // 传递 fd
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

VOID Fini(INT32 code, VOID *v) {
  // TODO-MKL: 将所有计数器都同步到内核。
  TraceFile.close();
  delete IOController_g;
  delete PinMemAgent_g;
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32
Usage() {
  cerr << "This tool traces stores to wpmfs." << endl;
  cerr << "Be sure that intel pin is available." << endl;
  cerr << "Be sure that you've specified the location of "
          "wpmfs and the corresponding proc file correctly."
       << endl;
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
