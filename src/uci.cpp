/*
 * Chess Engine - UCI Module (Implementation)
 * ------------------------------------------
 * This file implements the Universal Chess Interface (UCI) protocol loop
 * and supporting command handlers.
 *
 * Responsibilities:
 *  - Initialize engine subsystems
 *  - Parse UCI commands from GUI or console
 *  - Dispatch to search, position setup, benchmarking, perft, etc.
 *  - Maintain and update engine options (hash size, threads, MultiPV, etc.)
 *
 * The UCI loop reads commands line-by-line and responds according to
 * the protocol, ensuring compatibility with UCI-compliant GUIs.
 */

#include "uci.h"
#include <iostream>
#include <sstream>
#include <string>
#include "bench.h"
#include "bitboard.h"
#include "evaluate.h"
#include "hash.h"
#include "main.h"
#include "nnue.h"
#include "perft.h"
#include "search.h"
#include "thread.h"
#include "util.h"

// new_game()
// ----------
// Stop any current search, wait for search thread to finish, and reset search state.
// Called when starting a new game or after "ucinewgame" command.

/*
 * new_game()
 * ----------
 * Stop any running search, wait for threads to finish, and reset
 * search state so the next "position" or "go" starts clean.
 */

void new_game(){
  search::signals.stop_analyzing=true;
  thread_pool.main()->wake(false);
  thread_pool.main()->wait_for_search_to_end();
  search::reset();
}

// init_engine()
// --------------
// Initialize all engine subsystems (bitboards, position, search, threads, hash table, NNUE eval).
// Returns non-zero on success.

/*
 * init_engine()
 * -------------
 * Initialize all core subsystems in a safe order:
 *  1) Time origin
 *  2) Bitboards and attack tables
 *  3) Position and search static data
 *  4) Thread pool
 *  5) Hash table size
 *  6) Load NNUE network file
 * Returns nonzero on success if nnue_init succeeds.
 */

int init_engine(){
  thread_pool.start=now();
  bitboard::init();
  position::init();
  search::init();
  thread_pool.init();
  search::reset();
  main_hash.init(64);
  const char* filename=uci_nnue_evalfile.c_str();
  return nnue_init(filename);
}

namespace{
  // handle_uci()
// ------------
// Respond to "uci" command: print engine name, author, and available UCI options.
// Finish with "uciok".

  void handle_uci(){
    acout()<<"id name "<<program<<" "<<version<<" "<<platform<<" "<<bmis<<std::endl;
    acout()<<"id author "<<author<<std::endl;
    acout()<<"option name Hash type spin default 64 min 16 max 1048576"<<std::endl;
    acout()<<"option name Threads type spin default 1 min 1 max 128"<<std::endl;
    acout()<<"option name MultiPV type spin default 1 min 1 max 64"<<std::endl;
    acout()<<"option name Contempt type spin default 0 min -100 max 100"<<std::endl;
    acout()<<"option name MoveOverhead type spin default 50 min 0 max 1000"<<std::endl;
    acout()<<"option name Ponder type check default false"<<std::endl;
    acout()<<"option name UCI_Chess960 type check default false"<<std::endl;
    acout()<<"uciok"<<std::endl;
    fflush(stdout);
  }

  // handle_stop()
// -------------
// Signal search to stop and wake the main search thread.

/*
 * handle_stop()
 * -------------
 * Request the search to stop and wake the main thread.
 * Used for "stop" and "ponderhit" commands.
 */

  void handle_stop(){
    search::signals.stop_analyzing=true;
    thread_pool.main()->wake(false);
  }

  // handle_perft()
// ---------------
// Parse depth and optional FEN, then run perft (performance test) for debugging move generation.

/*
 * handle_perft(is)
 * ----------------
 * Parse optional depth and FEN, then run a perft test.
 * Syntax: "perft [depth] [fen-string]"
 */

  void handle_perft(std::istringstream& is){
    int depth=7;
    std::string fen=startpos;
    is>>depth;
    is>>fen;
    perft(depth,fen);
  }

  // handle_divide()
// ----------------
// Like perft but prints a breakdown per move, useful for debugging move generation.

/*
 * handle_divide(is)
 * -----------------
 * Like perft, but prints a breakdown of nodes per move.
 * Syntax: "divide [depth] [fen-string]"
 */

  void handle_divide(std::istringstream& is){
    int depth=7;
    std::string fen=startpos;
    is>>depth;
    is>>fen;
    divide(depth,fen);
  }

  // handle_bench()
// ---------------
// Run a benchmark search to measure engine speed and performance.

/*
 * handle_bench(is)
 * ----------------
 * Optional argument: depth. Runs a fixed bench to measure speed.
 * While bench is running, bench_active is set to true.
 */

  void handle_bench(std::istringstream& is){
    std::string token;
    const std::string bench_depth=is>>token?token:"14";
    bench_active=true;
    bench(std::stoi(bench_depth));
    bench_active=false;
  }
}

/*
 * uci_loop(argc, argv)
 * --------------------
 * Main loop for reading and executing UCI commands. Handles both interactive (stdin)
 * and command-line modes. Commands include:
 *   - uci, isready, ucinewgame, setoption, position, go, stop, quit
 *   - perft, divide, bench (debug/utility)
 */

/*
 * uci_loop(argc, argv)
 * --------------------
 * Main UCI read-eval-print loop.
 *  - If argc > 1, process the command from argv and exit.
 *  - Otherwise, read lines from stdin until "quit".
 * Handles: uci, isready, ucinewgame, setoption, position, go, stop,
 *          ponderhit, quit, perft, divide, bench.
 */

int uci_loop(const int argc,char* argv[]){
  position pos{};
  std::string token,cmd;
  int ret=0;
  pos.set(startpos,uci_chess960,thread_pool.main());
  new_game();
  for(auto i=1;i<argc;++i) cmd+=std::string(argv[i])+" ";
  do{
    if(argc==1&&!getline(std::cin,cmd)) cmd="quit";
    cmd=trim(cmd);
    if(cmd.empty()) continue;
    std::istringstream is(cmd);
    token.clear();
    is>>std::skipws>>token;
    if(token=="uci"){
      handle_uci();
    } else if(token=="isready"){
      acout()<<"readyok"<<std::endl;
      ret=fflush(stdout);
    } else if(token=="ucinewgame"){
      new_game();
    } else if(token=="setoption"){
      set_option(is);
    } else if(token=="position"){
      set_position(pos,is);
    } else if(token=="go"){
      go(pos,is);
    } else if(token=="stop"||(token=="ponderhit"
      &&search::signals.stop_if_ponder_hit)){
      handle_stop();
    } else if(token=="quit"){
      break;
    } else if(token=="pos"){
      acout()<<pos;
    } else if(token=="perft"){
      handle_perft(is);
    } else if(token=="divide"){
      handle_divide(is);
    } else if(token=="bench"){
      handle_bench(is);
    } else{}
  } while(token!="quit"&&argc==1);
  thread_pool.exit();
  return ret;
}

// set_option()
// -------------
// Parse "setoption" command and update global engine settings accordingly.
// Recognizes options: Hash, Threads, MultiPV, Contempt, MoveOverhead, Ponder, UCI_Chess960.

/*
 * set_option(is)
 * --------------
 * Parse "setoption name X value Y" and update engine options:
 *  - Hash, Threads, MultiPV, Contempt, MoveOverhead, Ponder, UCI_Chess960
 * Reconfigure subsystems as needed (hash size, thread pool).
 */

int set_option(std::istringstream& is){
  std::string token;
  is>>token;
  int ret=0;
  if(token=="name"){
    while(is>>token){
      if(token=="Hash"){
        is>>token;
        is>>token;
        uci_hash=stoi(token);
        main_hash.init(uci_hash);
        acout()<<"info string Hash "<<uci_hash<<" MB"<<std::endl;
        break;
      }
      if(token=="Threads"){
        is>>token;
        is>>token;
        uci_threads=stoi(token);
        thread_pool.change_thread_count(uci_threads);
        if(uci_threads==1)
          acout()<<"info string Threads "<<uci_threads<<" thread"
            <<std::endl;
        else
          acout()<<"info string Threads "<<uci_threads<<" threads"
            <<std::endl;
        break;
      }
      if(token=="MultiPV"){
        is>>token;
        is>>token;
        uci_multipv=stoi(token);
        acout()<<"info string MultiPV "<<uci_multipv<<std::endl;
        break;
      }
      if(token=="Contempt"){
        is>>token;
        is>>token;
        uci_contempt=stoi(token);
        acout()<<"info string Contempt "<<uci_contempt<<std::endl;
        break;
      }
      if(token=="MoveOverhead"){
        is>>token;
        is>>token;
        time_control.move_overhead=stoi(token);
        acout()<<"info string MoveOverhead "<<time_control.move_overhead
          <<" ms"<<std::endl;
        break;
      }
      if(token=="Ponder"){
        is>>token;
        is>>token;
        if(token=="true") uci_ponder=true;
        else uci_ponder=false;
        acout()<<"info string Ponder "<<uci_ponder<<std::endl;
        break;
      }
      if(token=="UCI_Chess960"){
        is>>token;
        is>>token;
        if(token=="true") uci_chess960=true;
        else uci_chess960=false;
        acout()<<"info string UCI_Chess960 "<<uci_chess960<<std::endl;
        break;
      }
    }
    ret=fflush(stdout);
  }
  return ret;
}

// go()
// -----
// Parse "go" command parameters (time control, depth, nodes, movetime, infinite)
// and start a search with those constraints.

/*
 * go(pos, is)
 * -----------
 * Parse time controls and search limits, then start a search:
 *   wtime/btime, winc/binc, movestogo, depth, nodes, movetime, infinite.
 * Passes a search_param to the thread pool.
 */

void go(position& pos,std::istringstream& is){
  search_param param;
  std::string token;
  param.infinite=1;
  while(is>>token){
    if(token=="wtime"){
      is>>param.time[white];
      param.infinite=0;
    } else if(token=="btime"){
      is>>param.time[black];
      param.infinite=0;
    } else if(token=="winc"){
      is>>param.inc[white];
      param.infinite=0;
    } else if(token=="binc"){
      is>>param.inc[black];
      param.infinite=0;
    } else if(token=="movestogo"){
      is>>param.moves_to_go;
      param.infinite=0;
    } else if(token=="depth"){
      is>>param.depth;
      param.infinite=0;
    } else if(token=="nodes"){
      is>>param.nodes;
      param.infinite=0;
    } else if(token=="movetime"){
      is>>param.move_time;
      param.infinite=0;
    } else if(token=="infinite") param.infinite=1;
  }
  thread_pool.begin_search(pos,param);
}

// set_position()
// ---------------
// Parse "position" command to set board state from "startpos" or a FEN string.
// Optionally play a sequence of moves afterwards.

/*
 * set_position(pos, is)
 * ---------------------
 * Parse a position specification:
 *  - "position startpos [moves ...]"
 *  - "position fen <FEN> moves ..."
 * Apply moves sequentially, updating ply for repetition tracking.
 */

void set_position(position& pos,std::istringstream& is){
  uint32_t move;
  std::string token,fen;
  is>>token;
  if(token=="startpos"){
    fen=startpos;
    is>>token;
  } else if(token=="fen") while(is>>token&&token!="moves") fen+=token+" ";
  else return;
  pos.set(fen,false,thread_pool.main());
  while(is>>token&&(move=move_from_string(pos,token))!=no_move){
    pos.play_move(move);
    pos.increase_game_ply();
  }
}

// trim()
// -------
// Remove leading and trailing whitespace from a string.

/*
 * trim(str, whitespace)
 * ---------------------
 * Return a copy of 'str' with leading and trailing characters in
 * 'whitespace' removed.
 */

std::string trim(const std::string& str,const std::string& whitespace){
  const auto str_begin=str.find_first_not_of(whitespace);
  if(str_begin==std::string::npos) return "";
  const auto str_end=str.find_last_not_of(whitespace);
  const auto str_range=str_end-str_begin+1;
  return str.substr(str_begin,str_range);
}

// sq()
// -----
// Convert a square index to algebraic notation string, e.g., e4.

/*
 * sq(sq)
 * ------
 * Convert a square enum to a coordinate string like "e4".
 */

std::string sq(const square sq){
  return std::string{
  static_cast<char>('a'+file_of(sq)),
  static_cast<char>('1'+rank_of(sq))
  };
}
