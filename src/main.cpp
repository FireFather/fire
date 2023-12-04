#include "uci.h"

int main(const int argc, char* argv[]) {
  init();
  uci_loop(argc, argv);
  return 0;
}
