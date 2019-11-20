#include "include/Benches.hpp"

int main() {
  BenchPool benchPool(100);
  if (benchPool.run()) {
    cout << "Bench pool failed." << endl;
    exit(-1);
  }

  // BenchFile benchFile(10);
  // if (benchFile.run()) {
  //   cout << "Bench file failed." << endl;
  //   exit(-1);
  // }

  return 0;
}
