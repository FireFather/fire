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
#include "define.h"
#include "fire.h"
#include "position.h"
#include "macro/square.h"

namespace pawn
{
	struct pawn_hash_entry
	{
		[[nodiscard]] int pawns_score() const
		{
			return pscore;
		}

		[[nodiscard]] uint64_t pawn_attack(const side color) const
		{
			return p_attack[color];
		}

		[[nodiscard]] uint64_t passed_pawns(const side color) const
		{
			return passed_p[color];
		}

		[[nodiscard]] uint64_t safe_for_pawn(const side color) const
		{
			return safe_pawn[color];
		}

		[[nodiscard]] int pawn_range(const side color) const
		{
			return pawn_span[color];
		}

		[[nodiscard]] int semi_open_side(const side color, const file f, const bool left_side) const
		{
			return half_open_lines[color] & (left_side ? (1 << f) - 1 : ~((1 << (f + 1)) - 1));
		}

		[[nodiscard]] int pawns_on_color(const side color, const square sq) const
		{
			return pawns_sq_color[color][!!(dark_squares & sq)];
		}

		[[nodiscard]] int pawns_not_on_color(const side color, const square sq) const
		{
			return pawns_sq_color[color][!(dark_squares & sq)];
		}

		uint64_t key;
		uint64_t passed_p[num_sides];
		uint64_t p_attack[num_sides];
		uint64_t safe_pawn[num_sides];
		int pscore;
		score my_king_safety[num_sides];
		uint8_t king_square[num_sides];
		uint8_t castle_possibilities[num_sides];
		uint8_t half_open_lines[num_sides];
		uint8_t pawn_span[num_sides];
		int asymmetry;
		uint8_t pawns_sq_color[num_sides][num_sides];
		int average_line;
		int n_pawns;
		bool conversion_difficult;
		int safety[num_sides];
		int file_width;
		char padding[20];
	};

	static_assert(offsetof(pawn_hash_entry, half_open_lines) == 72, "offset wrong");
	static_assert(sizeof(pawn_hash_entry) == 128, "Pawn Hash Entry size incorrect");

	template <class entry, int size>
	struct pawn_hash_table
	{
		entry* operator[](const uint64_t key)
		{
			static_assert(sizeof(entry) == 32 || sizeof(entry) == 128, "Wrong size");
			return reinterpret_cast<entry*>(reinterpret_cast<char*>(pawn_hash_mem_) + (static_cast<uint32_t>(key) & (size - 1) * sizeof(entry)));
		}

	private:
		CACHE_ALIGN entry pawn_hash_mem_[size];
	};

	pawn_hash_entry* probe(const position& pos);
	constexpr int pawn_hash_size = 16384;
	typedef pawn_hash_table<pawn_hash_entry, pawn_hash_size> pawn_hash;
}

inline square square_in_front(const side color, const square sq)
{
	return color == white ? sq + north : sq + south;
}

inline square square_behind(const side color, const square sq)
{
	return color == white ? sq - north : sq - south;
}
