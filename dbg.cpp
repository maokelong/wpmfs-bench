#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <x86intrin.h>
#include <iostream>

using std::cout;
using std::endl;

class PoolAgent {
  int fd;

  uint8_t *PoolAddr_k;
  const uint64_t PoolSize_k;
  const char *PoolSrc_k;

 public:
  PoolAgent()
      : PoolAddr_k(NULL),
        PoolSize_k(((uint64_t)128 * 1024)),
        PoolSrc_k("/mnt/wpmfs/test") {}

  void init() {
    // 打开内存映射文件
    if ((fd = open(PoolSrc_k, O_CREAT | O_RDWR, 0666)) < 0) {
      cout << "Failed to open the memory mapped file." << endl;
      exit(-1);
    }

    // 在文件内预留空洞
    int errnoFallocate;
    if ((errnoFallocate = posix_fallocate(fd, 0, PoolSize_k)) != 0) {
      cout << "Failed to punch a hole within given memory mapped file." << endl;
      exit(-1);
    }

    // 将内存映射文件映射到内存，作为池
    // #define MAP_SHARED_VALIDATE 0x03
    // #define MAP_SYNC	0x80000
    PoolAddr_k = (uint8_t *)mmap((void *)0x600000000000, PoolSize_k,
                               PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED,
                               //  0x03 | 0x80000 | MAP_FIXED,
                               fd, 0);
  }

  void dumpToMemory() {
    for (uint64_t i = 0; i < PoolSize_k; i += 64) {
      for (uint64_t j = 0; j < 64; j += sizeof(uint64_t))
        *((uint64_t *)(PoolAddr_k + i + j)) = i + j;
      _mm_clflush(&PoolAddr_k[i]);
    }
    cout << "Dump to memory." << endl;
  }

  void checkMemory() {
    for (uint64_t i = 0; i < PoolSize_k; i += 64)
      for (uint64_t j = 0; j < 64; j += sizeof(uint64_t)) {
        uint64_t valueRead = *((uint64_t *)(PoolAddr_k + i + j));
        if (valueRead != i + j) {
          cout << "Fatal! Memory may be corrupted." << endl;
          cout << "Expect " << i + j << " but reads " << valueRead << endl;
          exit(-1);
        }
      }
    cout << "Great! Memory check passed!" << endl;
  }
};

int main() {
  PoolAgent poolAgent;
  poolAgent.init();
  poolAgent.dumpToMemory();
  poolAgent.checkMemory();

  return 0;
}
