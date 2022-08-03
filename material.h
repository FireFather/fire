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
#include "endgame.h"
#include "fire.h"
#include "position.h"

namespace material
{
	// material hash data structure
	struct mat_hash_entry
	{
		[[nodiscard]] int get_game_phase() const
		{
			return game_phase;
		}

		[[nodiscard]] bool has_value_function() const
		{
			return value_function_index >= 0;
		}
		[[nodiscard]] int value_from_function(const position& pos) const;
		[[nodiscard]] sfactor scale_factor_from_function(const position& pos, side color) const;

		uint64_t key64;
		int value_function_index;
		int scale_function_index[num_sides];
		int value;
		sfactor conversion;
		uint8_t factor[num_sides];
		uint8_t game_phase;
		bool conversion_is_estimated;
	};

	static_assert(sizeof(mat_hash_entry) == 32, "Material Entry size incorrect");

	template <class Entry, int Size>
	struct material_hash_table
	{
		Entry* operator[](const uint64_t key)
		{
			static_assert(sizeof(Entry) == 32 || sizeof(Entry) == 128, "Wrong size");
			return reinterpret_cast<Entry*>(reinterpret_cast<char*>(mat_hash_mem_) + (static_cast<uint32_t>(key) & (Size - 1) * sizeof(Entry)));
		}

	private:
		CACHE_ALIGN Entry mat_hash_mem_[Size];
	};
	// pawn factors
	inline auto p_base_score = 950;
	inline auto p_q_factor = 90;
	inline auto p_r_factor = 28;
	inline auto p_b_factor = 17;
	inline auto p_n_factor = 16;

	// knight factors
	inline auto n_base_score = 3510;
	inline auto n_q_factor = 50;
	inline auto n_r_factor = 35;
	inline auto n_b_factor = 7;
	inline auto n_n_factor = 8;
	inline auto n_p_factor = 22;

	// bishop factors
	inline auto b_base_score = 3775;
	inline auto b_q_factor = 55;
	inline auto b_r_factor = 27;
	inline auto b_b_factor = 9;
	inline auto b_n_factor = 6;
	inline auto b_p_factor = 6;

	// rook factors
	inline auto r_base_score = 6295;
	inline auto r_q_factor = 261;
	inline auto r_r_factor = 217;
	inline auto r_b_factor = 34;
	inline auto r_n_factor = 40;
	inline auto r_p_factor = 10;

	// queen factors
	inline auto q_base_score = 11715;
	inline auto q_q_factor = 338;
	inline auto q_r_factor = 169;
	inline auto q_b_factor = 37;
	inline auto q_n_factor = 25;
	inline auto q_p_factor = 6;

	// bishop pair factors
	inline auto bp_base_score = 515;
	inline auto bp_q_factor = 41;
	inline auto bp_r_factor = 21;
	inline auto bp_b_factor = 7;
	inline auto bp_n_factor = 7;

	// material imbalance
	inline auto up_two_pieces_bonus = 200;
	inline auto more_bishops_bonus = 17;
	inline auto more_knights_bonus = 17;

	// phase factors
	inline auto max_phase = 32;
	inline auto r_phase_factor = 3;
	inline auto q_phase_factor = 6;

	// default material hash size = 16 MB;
	constexpr int material_hash_size = 16384;

	typedef material_hash_table<mat_hash_entry, material_hash_size> material_hash;
	mat_hash_entry* probe(const position& pos);
}
