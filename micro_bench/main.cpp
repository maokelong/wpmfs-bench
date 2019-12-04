#include "include/Benches.hpp"

int main() {
  BenchPool benchPool(1);
  if (benchPool.run()) {
    cout << "Bench pool failed." << endl;
    exit(-1);
  }

  BenchFile benchFile(1);
  if (benchFile.run()) {
    cout << "Bench file failed." << endl;
    exit(-1);
  }

  return 0;
}
