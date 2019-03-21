#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <x86intrin.h>

uint8_t *PoolAddr = NULL;
const size_t PoolSize_k = 1 * 1024 * 1024 * 1024;
const char *PoolSrc = "/mnt/wpmfs/test";

int main() {
  int fd, errno;
  // 打开内存映射文件
  if ((fd = open(PoolSrc, O_CREAT | O_RDWR, 0666)) < 0) {
    printf("failed to open.\n");
    return -1;
  }

  // 在上述文件中预留空洞
  if ((errno = posix_fallocate(fd, 0, PoolSize_k)) != 0) {
    printf("failed to fallocate.\n");
    return -1;
  }

  // 内存映射
  PoolAddr =
      (uint8_t *)mmap((void *)0x600000000000, PoolSize_k,
                      PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);

  // 测试 demand paging
  size_t i = 0;
  for (int i = 0; i < PoolSize_k; i += 64) {
    for (int j = 0; j < 64; ++j) PoolAddr[j] = 1;
    _mm_clflush(&PoolAddr[i]);
  }

  return 0;
}
