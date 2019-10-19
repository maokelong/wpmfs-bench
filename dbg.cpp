#include "micro_bench/Benches.hpp"

int main() {
  BenchPool benchPool(10);
  if (benchPool.run()) {
    cout << "Bench pool failed." << endl;
    exit(-1);
  }

  BenchFile benchFile(10);
  if (benchFile.run()) {
    cout << "Bench file failed." << endl;
    exit(-1);
  }

  return 0;
}
