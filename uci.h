/*
  Fire is a freeware UCI chess playing engine authored by Norman Schmidt.

  Fire utilizes many state-of-the-art chess programming ideas and techniques
  which have been documented in detail at https://www.chessprogramming.org/
  and demonstrated via the very strong open-source chess engine Stockfish...
  https://github.com/official-stockfish/Stockfish.

  Fire is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or any later version.

  You should have received a copy of the GNU General Public License with
  this program: copying.txt.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include "position.h"

// UCI option default values
inline std::string startpos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
inline int uci_hash = 64;
inline int uci_threads = 1;
inline int uci_multipv = 1;
inline int uci_contempt = 0;
inline bool uci_mcts = false;
inline bool uci_ponder = false;
inline bool uci_chess960 = false;

inline bool uci_syzygy_50_move_rule = false;
inline int uci_syzygy_probe_depth = 1;
inline int uci_syzygy_probe_limit = 6;

inline std::string uci_syzygy_path;
inline std::string uci_nnue_evalfile = "10072022-021546.bin";

inline std::string engine_mode = "nnue";
inline bool bench_active = false;

// function declarations
void init(int hash_size);
void new_game();
void uci_loop(int argc, char* argv[]);
void set_position(position& pos, std::istringstream& is);
void set_option(std::istringstream& is);
void go(position& pos, std::istringstream& is);
void bench(int depth);
std::string trim(const std::string& str, const std::string& whitespace = " \t");
std::string sq(square sq);
std::string print_pv(const position& pos, int alpha, int beta, int active_pv, int active_move);