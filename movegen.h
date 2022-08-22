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
#include "fire.h"

class position;

struct s_move
{
	uint32_t move;
	int value;

	operator uint32_t() const
	{
		return move;
	}

	void operator=(const uint32_t z)
	{
		move = z;
	}
};

enum move_gen
{
	captures_promotions,
	quiet_moves,
	quiet_checks,
	evade_check,
	all_moves,
	pawn_advances,
	queen_checks,
	castle_moves
};

namespace movegen
{
	template <side me, move_gen mg>
	s_move* moves_for_pawn(const position& pos, s_move* moves, uint64_t target);

	template <side me, uint8_t piece, bool only_check_moves>
	s_move* moves_for_piece(const position& pos, s_move* moves, uint64_t target);

	template < uint8_t castle, bool only_check_moves, bool chess960>
	s_move* get_castle(const position& pos, s_move* moves);
}

inline bool operator<(const s_move& f, const s_move& s)
{
	return f.value < s.value;
}

template <move_gen>
s_move* generate_moves(const position& pos, s_move* moves);
s_move* generate_captures_on_square(const position& pos, s_move* moves, square sq);
s_move* generate_legal_moves(const position& pos, s_move* moves);

struct legal_move_list
{
	explicit legal_move_list(const position& pos)
	{
		last_ = generate_legal_moves(pos, moves_);
	}

	[[nodiscard]] const s_move* begin() const
	{
		return moves_;
	}

	[[nodiscard]] const s_move* end() const
	{
		return last_;
	}
	[[nodiscard]] size_t size() const
	{
		return last_ - moves_;
	}

private:
	s_move* last_;
	s_move moves_[max_moves]{};
};

bool legal_moves_list_contains_move(const position& pos, uint32_t move);
bool at_least_one_legal_move(const position& pos);
bool legal_move_list_contains_castle(const position& pos, uint32_t move);
