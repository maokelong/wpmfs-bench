#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include "MicroBench.hpp"

const int PAGE_SIZE_K = 4096;
const uint64_t FILE_LEN_K = (uint64_t)1 * 1024 * 1024 * 1024;

class BenchPool : public MicroBench {
  uint8_t *PoolAddr_k;
  const uint64_t PoolSize_k;
  const int dumpRounds_k;

  void dumpToMemory() {
    for (uint64_t i = 0; i < PoolSize_k; i += 64) {
      for (uint64_t j = 0; j < 64; j += sizeof(uint64_t))
        *((uint64_t *)(PoolAddr_k + i + j)) = i + j;
      _mm_clflush(&PoolAddr_k[i]);
    }
  }

  bool checkMemory() {
    for (uint64_t i = 0; i < PoolSize_k; i += 64)
      for (uint64_t j = 0; j < 64; j += sizeof(uint64_t)) {
        uint64_t valueRead = *((uint64_t *)(PoolAddr_k + i + j));
        if (valueRead != i + j) {
          cout << "Fatal! Memory may be corrupted." << endl;
          cout << "Expect " << i + j << " but reads " << valueRead << endl;
          return false;
        }
      }
    cout << "BenchPool: Memory check passed!" << endl;
  }

 protected:
  virtual bool init(void) {
    // 在文件内预留空洞
    if (posix_fallocate(fd_k, 0, PoolSize_k) != 0) {
      cout << "Failed to punch a hole within given memory mapped file." << endl;
      return false;
    }

    // 将内存映射文件映射到内存，作为池
    // #define MAP_SHARED_VALIDATE 0x03
    // #define MAP_SYNC	0x80000
    PoolAddr_k = (uint8_t *)mmap((void *)0x600000000000, PoolSize_k,
                                 PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED,
                                 fd_k, 0);
  }

  virtual bool operate(void) {
    cout << "BenchPool: Begin to dump to memory. Rounds = " << dumpRounds_k
         << "." << endl;
    for (int i = 0; i < dumpRounds_k; ++i) dumpToMemory();
    cout << "BenchPool: Memory dumping successed!" << endl;
    return checkMemory();
  }

  virtual bool exit(void) { return true; }

 public:
  BenchPool(int dumpRounds)
      : PoolAddr_k(NULL),
        // round down`
        PoolSize_k((FILE_LEN_K / 4096) * 4096),
        dumpRounds_k(dumpRounds) {}
};

class BenchFile : public MicroBench {
  const int dumpRounds_k;
  const uint64_t filePages_k;

  bool dumpToFile() {
    char buffer[PAGE_SIZE_K];

    lseek(fd_k, 0, SEEK_SET);
    for (int i = 0; i < filePages_k; ++i) {
      memset(buffer, (i + 'a') % 26, PAGE_SIZE_K);
      if (write(fd_k, buffer, PAGE_SIZE_K) != PAGE_SIZE_K) {
        cout << "Failed to write to a file." << endl;
        return false;
      }
    }
    return true;
  }

  bool checkFile() {
    char buffer[PAGE_SIZE_K];

    lseek(fd_k, 0, SEEK_SET);
    for (int i = 0; i < filePages_k; ++i) {
      if (read(fd_k, buffer, PAGE_SIZE_K) != PAGE_SIZE_K) {
        cout << "Unmatched file size." << endl;
        return false;
      }

      for (int j = 0; j < PAGE_SIZE_K; ++j) {
        if (buffer[j] != (i + 'a') % 26) {
          cout << "Fatal! File may be corrupted." << endl;
          return false;
        }
      }
    }

    cout << "BenchFile: File check passed!" << endl;
    return true;
  }

 protected:
  virtual bool init(void) { return true; };

  virtual bool operate(void) {
    cout << "BenchFile: Begin to dump to file. Rounds = " << dumpRounds_k << "."
         << endl;

    for (int i = 0; i < dumpRounds_k; ++i)
      if (!dumpToFile()) return false;
    cout << "BenchFile: File dumping successed!" << endl;

    return checkFile();
  };

  virtual bool exit(void) { return true; };

 public:
  BenchFile(int dumpRounds)
      : dumpRounds_k(dumpRounds), filePages_k(FILE_LEN_K / 4096) {}
};
