#include "uci.h"
#include "util.h"

int main(const int argc,char* argv[]){
  engine_info();
  build_info();
  init_engine();
  uci_loop(argc,argv);
  return 0;
}
