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

	template <class entry, int Size>
	struct material_hash_table
	{
		entry* operator[](const uint64_t key)
		{
			static_assert(sizeof(entry) == 32 || sizeof(entry) == 128, "Wrong size");
			return reinterpret_cast<entry*>(reinterpret_cast<char*>(mat_hash_mem_) + (static_cast<uint32_t>(key) & (Size - 1) * sizeof(entry)));
		}

	private:
		CACHE_ALIGN entry mat_hash_mem_[Size];
	};

	// default material hash size = 16 MB;
	constexpr int material_hash_size = 16384;

	typedef material_hash_table<mat_hash_entry, material_hash_size> material_hash;
	mat_hash_entry* probe(const position& pos);
}
