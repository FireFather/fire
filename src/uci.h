#pragma once
#include "position.h"

inline std::string startpos =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
inline std::string uci_nnue_evalfile = "fire-9.3.nnue";
inline int uci_hash = 64;
inline int uci_threads = 1;
inline int uci_multipv = 1;
inline int uci_contempt = 0;
inline bool uci_ponder = false;
inline bool uci_chess960 = false;
inline bool bench_active = false;
int init_engine();
void new_game();
int uci_loop(int argc, char* argv[]);
void set_position(position& pos, std::istringstream& is);
int set_option(std::istringstream& is);
void go(position& pos, std::istringstream& is);
int bench(int depth);
std::string trim(const std::string& str, const std::string& whitespace = " \t");
std::string sq(square sq);
std::string print_pv(const position& pos, int alpha, int beta, int active_pv,
                     int active_move);