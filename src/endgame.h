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
inline constexpr sfactor draw_factor = static_cast<sfactor>(0);
inline constexpr sfactor one_pawn_factor = static_cast<sfactor>(75);
inline constexpr sfactor normal_factor = static_cast<sfactor>(100);
inline constexpr sfactor max_factor = static_cast<sfactor>(200);
inline constexpr sfactor no_factor = static_cast<sfactor>(255);
using endgame_value = int(*)(const position& pos);
using endgame_scale_factor = sfactor(*)(const position& pos);
class endgames
{
	using function_index_map = std::map<uint64_t, int>;
	function_index_map map_value_;
	function_index_map map_scale_factor_;
public:
	endgame_value value_functions[16] = {};
	endgame_scale_factor factor_functions[32] = {};
};
inline constexpr int value_tempo = 24;
inline constexpr uint64_t black_modifier = 0xa4489c56;
inline constexpr uint64_t averbakh_rule = 0xfff7e3c180000000;
namespace endgame {

	inline constexpr int push_to_side[num_squares] =
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
	inline constexpr int push_to_corner[num_squares] =
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
	inline constexpr int draw_closer[8] = { 0, 0, 80, 64, 48, 32, 16, 8 };
	inline constexpr int krppkrp_scale_factors[num_ranks] = { 0, 14, 16, 22, 33, 69, 0, 0 };
}
