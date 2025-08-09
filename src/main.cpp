/*
 * Chess Engine - Program Entry (Implementation)
 * --------------------------------------------
 * Boot sequence:
 *   1) Print engine and build information
 *   2) Initialize subsystems (bitboards, TT, threads, NNUE, etc.)
 *   3) Enter UCI loop
 */

#include "uci.h"
#include "util.h"

/*
 * main(argc, argv)
 * ----------------
 * Print info, initialize engine components, and enter the UCI loop.
 */

int main(const int argc,char* argv[]){
  engine_info();
  build_info();
  init_engine();
  uci_loop(argc,argv);
  return 0;
}
