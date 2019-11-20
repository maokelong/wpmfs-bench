#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>

using std::cout;
using std::endl;

class MicroBench {
  const char *FileSrc_k;

  bool openPMFile(void) {
    if ((fd_k = open(FileSrc_k, O_CREAT | O_RDWR, 0666)) < 0) {
      cout << "Failed to open the memory mapped file." << endl;
      return false;
    }
    return true;
  }

  bool closePMFIle(void) {
    if (fd_k != -1) close(fd_k);
  }

 protected:
  virtual bool init(void) {
    cout << "init not implemented." << endl;
    return false;
  };

  virtual bool operate(void) {
    cout << "operate not implemented." << endl;
    return false;
  };

  virtual bool exit(void) {
    cout << "exit not implemented." << endl;
    return false;
  };

 public:
  int fd_k;

  MicroBench(void) : fd_k(-1), FileSrc_k("/mnt/wpmfs/test") {}
  ~MicroBench() {
    if (fd_k != -1) {
      close(fd_k);
      fd_k = -1;
    }
  }

  // this virtual class gives a hint on how to implement a microbench
  bool run() {
    if (openPMFile() && init() && operate() && exit() && closePMFIle())
      return true;
    return false;
  }
};
