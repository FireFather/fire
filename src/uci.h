/*
 * Chess Engine - UCI Module (Interface)
 * -------------------------------------
 * Declares the functions and global settings used by the UCI front-end.
 *
 * Globals:
 *  - startpos FEN
 *  - UCI options (hash, threads, MultiPV, contempt, ponder, chess960, etc.)
 *
 * Functions:
 *  - init_engine(): initialize all engine subsystems
 *  - new_game(): reset search state for a fresh game
 *  - uci_loop(): main input loop for parsing and handling commands
 *  - set_position(): parse "position" command and update board
 *  - set_option(): parse "setoption" command to update settings
 *  - go(): parse "go" command and start a search
 *  - trim(), sq(): string utilities for command parsing and square printing
 */

#pragma once
#include "position.h"

// Default starting position in FEN format

inline std::string startpos=
  "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Default NNUE network file name

inline std::string uci_nnue_evalfile="fire-10.nnue";

// Hash size in MB (can be changed via UCI option)

inline int uci_hash=64;

// Number of search threads (UCI Threads)

inline int uci_threads=1;

// Number of principal variations to report (MultiPV)

inline int uci_multipv=1;

// Search contempt value in centipawns

inline int uci_contempt=0;

// Whether the engine thinks on the opponent's time (Ponder)

inline bool uci_ponder=false;

// Support for Chess960 rules in UCI

inline bool uci_chess960=false;

// True while the built-in bench is running

inline bool bench_active=false;

/* UCI entry points */
int init_engine();
void new_game();
int uci_loop(int argc,char* argv[]);
void set_position(position& pos,std::istringstream& is);
int set_option(std::istringstream& is);
void go(position& pos,std::istringstream& is);
std::string trim(const std::string& str,const std::string& whitespace=" 	");
std::string sq(square sq);
std::string print_pv(const position& pos,int alpha,int beta,int active_pv,
  int active_move);
