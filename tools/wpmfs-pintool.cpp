#include <assert.h>
#include <ctime>
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
  int mfd;
#pragma pack(1)  // 统一不加 padding
  struct {
    unsigned opcode;

    union {
      struct {
        int fd;
        uint64_t pageoff;  // 被统计页起始位置相对于文件的偏移
        uint64_t page_wcnt;  // 被统计页的写次数（读写取决于命令）
      };

      uint64_t capacity;

      struct {
        uint64_t blocknr;
        uint64_t blk_wcnt;
      };
    };

    uint8_t succ;
  } message;
#pragma pack()

  enum opcpde {
    WPMFS_INC_CNT = 0xBCD00020,
    WPMFS_GET_CNT,
    WPMFS_CMD_GET_FS_CAP,
    WPMFS_CMD_GET_FS_WEAR
  };

  void request() {
    OS_RETURN_CODE ret;
    USIZE len = sizeof(message);

    ret = OS_WriteFD(nfd, &message, &len);
    assert(ret.generic_err == OS_RETURN_CODE_NO_ERROR);
    assert(message.succ);
  }
  void fetch() {
    OS_RETURN_CODE ret;
    USIZE len = sizeof(message);

    ret = OS_ReadFD(nfd, &len, &message);
    assert(ret.generic_err == OS_RETURN_CODE_NO_ERROR);
    assert(message.succ);
  }

  void requestAndFetch() {
    request();
    fetch();
  }

 public:
  IOController(int fd) {
    OS_RETURN_CODE ret;
    // 以读写模式打开 proc 文件，该文件必须已存在
    ret = OS_OpenFD("/proc/wpmfs_proc",
                    OS_FILE_OPEN_TYPE_READ | OS_FILE_OPEN_TYPE_WRITE |
                        OS_FILE_OPEN_TYPE_TRUNCATE,
                    0, &nfd);
    assert(ret.generic_err == OS_RETURN_CODE_NO_ERROR);
    mfd = fd;
  }

  ~IOController() { OS_CloseFD(nfd); }

  bool incCnt(uint64_t pageoff, uint64_t cnt) {
    message.opcode = WPMFS_INC_CNT;
    message.fd = mfd;
    message.pageoff = pageoff;
    message.page_wcnt = cnt;
    request();
    return message.succ == 1;
  }

  bool getCnt(uint64_t pageof, uint64_t &cnt) {
    message.opcode = WPMFS_GET_CNT;
    message.fd = mfd;
    requestAndFetch();
    cnt = message.page_wcnt;
    return message.succ == 1;
  }

  bool getFsCap(uint64_t &capacity) {
    message.opcode = WPMFS_CMD_GET_FS_CAP;
    requestAndFetch();
    capacity = message.capacity;
    return message.succ == 1;
  }

  bool getFsBlkCnt(size_t blocknr, uint64_t &blk_wcnt) {
    message.opcode = WPMFS_CMD_GET_FS_WEAR;
    message.blocknr = blocknr;
    requestAndFetch();
    blk_wcnt = message.blk_wcnt;
    return message.succ == 1;
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
  size_t itemSize;

 public:
  MemPool(T *addr, size_t len, int offset, size_t itemSize)
      : addr(static_cast<T *>(addr)),
        len(len),
        offset(offset),
        itemSize(itemSize) {}

  // 给定地址是否在内存池地址范围内，自动内联
  bool addrInPool(T *addr) {
    return addr >= this->addr && addr < this->addr + this->len;
  }

  // 给定地址的逻辑页号（不考虑偏移），自动内联
  size_t addrToItemIndex(T *addr) {
    return (addr - this->addr) * sizeof(T) / itemSize;
  }

  // 给定地址的逻辑页号（考虑偏移），自动内联
  size_t addrToItemIndexReal(T *addr) {
    return addrToItemIndex(addr) + offset / itemSize;
  }

  // 通过 [] 访问内存池
  T &operator[](int index) { return addr[index]; }

  size_t getNumItems() { return len / itemSize; }
  size_t getFstIndexReal() { return offset / itemSize; }
};

// 内存池代理
class PinMemAgent {
  MemPool<uint8_t> *AppMemPool_g;  // 托管的应用申请的内存池
  MemPool<uint64_t> *PinCntPool_g;  // 托管的 pin 维护的计数器的内存池

 public:
  PinMemAgent(ADDRINT *addr, ADDRINT len, ADDRINT offset) {
    AppMemPool_g =
        new MemPool<uint8_t>((uint8_t *)(addr), len, offset, LenPage_k);
    len = len / LenPage_k;
    PinCntPool_g =
        new MemPool<uint64_t>(new uint64_t[len](), len, 0, sizeof(uint64_t));
  }

  ~PinMemAgent() { delete PinCntPool_g->addr; }

  // 当前地址是否位于 app 地址池中？
  bool addrInAppPool(ADDRINT *addr) {
    return AppMemPool_g->addrInPool((uint8_t *)addr);
  }

  // 当前访问地址的写计数器是否抵达阈值？
  bool cntReachThres(ADDRINT *addr, ADDRINT size) {
    return ((*PinCntPool_g)[AppMemPool_g->addrToItemIndex((uint8_t *)addr)] +=
            size) >= ThresSync_k;
  }

  // 将当前访问地址的写计数器同步到内核，并清空 Pin 维护的计数器
  void syncCnt(ADDRINT *addr) {
    // TODO-MKL: 考虑 unaligned mem acc
    uint64_t pageOffReal = AppMemPool_g->addrToItemIndexReal((uint8_t *)addr);
    uint64_t &writeCnt =
        (*PinCntPool_g)[AppMemPool_g->addrToItemIndex((uint8_t *)addr)];
    IOController_g->incCnt(pageOffReal, writeCnt);
    writeCnt = 0;
  }

  void syncCntAll() {
    clock_t start, end;
    size_t numCnt = PinCntPool_g->getNumItems();
    size_t fstIndex = AppMemPool_g->getFstIndexReal();

    cout << "Sync-ing all write tracking counter buffers with WPMFS. "
         << "Please wait." << endl;

    start = clock();
    for (size_t i = 0; i < numCnt; ++i) {
      uint64_t &writeCnt = (*PinCntPool_g)[i];
      if (IOController_g->incCnt(i + fstIndex, writeCnt)) continue;

      std::cout
          << "Some error occurred while sync-ing. Mmaped-file may be closed."
          << std::endl;
      break;
    }
    end = clock();
    std::cout << "Sync-ing has finished. Time elapsed = "
              << (double)(end - start) / CLOCKS_PER_SEC << "s." << std::endl;
  }

  void dumpFsCntAll() {
    clock_t start, end;
    size_t fs_cap;

    cout << "Dumping the write times that each page has suffered. Please wait."
         << endl;

    IOController_g->getFsCap(fs_cap);
    // TraceFile << "Capacity of WPMFS(in bytes): " << fs_cap << std::endl;

    start = clock();
    fs_cap /= LenPage_k;
    for (size_t i = 0; i < fs_cap; ++i) {
      size_t fsBlkCnt;
      if (IOController_g->getFsBlkCnt(i, fsBlkCnt) && fsBlkCnt != 0)
        TraceFile << i << '\t' << fsBlkCnt << endl;
    }
    end = clock();

    std::cout << "Dumping has finished. Time elapsed = "
              << (double)(end - start) / CLOCKS_PER_SEC << "s." << std::endl;
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
  // TODO-MKL: 基于 Simulated Cache，关注 Cache Eviction
}

/* ===================================================================== */

VOID Fini(INT32 code, VOID *v) {
  // TODO: 可能因文件关闭导致无法同步，fixme。
  if (!PinMemAgent_g) PinMemAgent_g = new PinMemAgent(NULL, 0, 0);
  if (!IOController_g) IOController_g = new IOController(-1);

  PinMemAgent_g->syncCntAll();
  PinMemAgent_g->dumpFsCntAll();

  delete PinMemAgent_g;
  delete IOController_g;

  TraceFile.close();
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
  TraceFile.setf(ios::showbase);

  // Register Instruction/Image to be called to instrument functions.
  INS_AddInstrumentFunction(Instruction, 0);
  IMG_AddInstrumentFunction(Image, 0);
  PIN_AddFiniFunction(Fini, 0);

  // Never returns
  PIN_StartProgram();

  return 0;
}
