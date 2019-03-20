#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <x86intrin.h>

// 数组声明
char *HUGE_ARRAY;
size_t HUGE_ARRAY_SIZE = 1024 * 1024 * 1024;

int main() {
  // 初始化数组（使用映射段而非数据段）
  HUGE_ARRAY = (char *)mmap((void *)0x600000000000, HUGE_ARRAY_SIZE,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
  // 打印数组基址
  printf("%p\n", HUGE_ARRAY);

  // 测试 demand paging
  size_t i = 0;
  for (int i = 0; i < HUGE_ARRAY_SIZE; i += 64) {
    for (int j = 0; j < 64; ++j) HUGE_ARRAY[j] = 1;
    _mm_clflush(&HUGE_ARRAY[i]);
  }

  return 0;
}
