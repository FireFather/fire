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

#include "zobrist.h"

#include "position.h"

uint64_t position::exception_key(const uint32_t move)
{
	return zobrist::psq[w_king][from_square(move)] ^ zobrist::psq[b_king][to_square(move)];
}

uint64_t position::draw50_key() const
{
	return zobrist::hash_50_move[pos_info_->draw50_moves >> 2];
}