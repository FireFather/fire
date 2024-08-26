#pragma once
#include "position.h"

inline bool bench_active = false;
inline bool uci_chess960 = false;
inline bool uci_ponder = false;

inline int uci_contempt = 0;
inline int uci_hash = 64;
inline int uci_multipv = 1;
inline int uci_threads = 1;

inline std::string startpos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
inline std::string uci_nnue_evalfile = "fire-9.3.nnue";

int bench(int depth);
int init_engine();
int set_option(std::istringstream& is);
int uci_loop(int argc, char* argv[]);

std::string print_pv(const position& pos, int alpha, int beta, int active_pv, int active_move);
std::string sq(square sq);
std::string trim(const std::string& str, const std::string& whitespace = " \t");

void go(position& pos, std::istringstream& is);
void new_game();
void set_position(position& pos, std::istringstream& is);
