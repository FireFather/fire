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
#include <cstdint>

#include "fire.h"
#include "position.h"

namespace zobrist
{
	inline uint64_t psq[num_pieces][num_squares];
	inline uint64_t enpassant[num_files];
	inline uint64_t castle[castle_possible_n];
	inline uint64_t on_move;
	inline uint64_t hash_50_move[32];
}
