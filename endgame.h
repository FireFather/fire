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
#include <map>

#include "fire.h"
#include "position.h"

enum sfactor : int;
constexpr sfactor draw_factor = static_cast<sfactor>(0);
constexpr sfactor one_pawn_factor = static_cast<sfactor>(75);
constexpr sfactor normal_factor = static_cast<sfactor>(100);
constexpr sfactor max_factor = static_cast<sfactor>(200);
constexpr sfactor no_factor = static_cast<sfactor>(255);

typedef int (*endgame_value)(const position& pos);
typedef sfactor(*endgame_scale_factor)(const position& pos);

class endgames
{
	void add_value(const char* pieces, endgame_value, endgame_value);
	void add_scale_factor(const char* pieces, endgame_scale_factor, endgame_scale_factor);

	typedef std::map<uint64_t, int> function_index_map;

	int value_number_ = 0;
	int factor_number_ = 0;
	function_index_map map_value_;
	function_index_map map_scale_factor_;

public:
	endgames();
	void init_endgames();
	void init_scale_factors();

	int probe_value(uint64_t key);
	int probe_scale_factor(uint64_t key, side& strong_side);

	endgame_value value_functions[16] = {};
	endgame_scale_factor factor_functions[32] = {};
};

constexpr int value_tempo = 24;
constexpr uint64_t black_modifier = 0xa4489c56;
constexpr uint64_t averbakh_rule = 0xfff7e3c180000000;

namespace endgame
{
	square normalize_pawn_side(const position& pos, side strong_side, square sq);
	uint64_t attack_king_inc(square s);


	constexpr int push_to_side[num_squares] =
	{
		 80, 72, 64, 56, 56, 64, 72, 80,
		 72, 56, 48, 40, 40, 48, 56, 72,
		 64, 48, 32, 24, 24, 32, 48, 64,
		 56, 40, 24, 16, 16, 24, 40, 56,
		 56, 40, 24, 16, 16, 24, 40, 56,
		 64, 48, 32, 24, 24, 32, 48, 64,
		 72, 56, 48, 40, 40, 48, 56, 72,
		 80, 72, 64, 56, 56, 64, 72, 80
	};

	constexpr int push_to_corner[num_squares] =
	{
		 80, 72, 64, 56, 48, 40, 32, 24,
		 72, 64, 56, 48, 40, 32, 24, 32,
		 64, 56, 44, 32, 32, 20, 32, 40,
		 56, 48, 32, 16, 8, 32, 40, 48,
		 48, 40, 32, 8, 16, 32, 48, 56,
		 40, 32, 20, 32, 32, 44, 56, 64,
		 32, 24, 32, 40, 48, 56, 64, 72,
		 24, 32, 40, 48, 56, 64, 72, 80
	};

	constexpr int draw_closer[8] =
	{
		0, 0, 80, 64, 48, 32, 16, 8

	};

	constexpr int krppkrp_scale_factors[num_ranks] =
	{
		0, 14, 16, 22, 33, 69, 0, 0
	};
}

template <side strong>
int endgame_kxk(const position& pos);

template <side strong>
sfactor endgame_kbpk(const position& pos);

template <side strong>
sfactor endgame_kqkrp(const position& pos);

template <side strong>
sfactor endgame_kpk(const position& pos);

template <side strong>
sfactor endgame_kpkp(const position& pos);

inline int value_of_material(int val);