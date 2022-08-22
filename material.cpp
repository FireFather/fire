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

#include "material.h"

#include "bitboard.h"
#include "macro/side.h"
#include "fire.h"
#include "pragma.h"
#include "thread.h"

namespace material
{
	// adapted, consolidated, and optimized from robbolito material routines
	// https://github.com/FireFather/robbolito/blob/main/values.c

	int mat_imbalance(const int wp, const int wn, const int wb, const int wbl, const int wbd, const int wr, const int wq,
		const int bp, const int bn, const int bb, const int bbl, const int bbd, const int br, const int bq)
	{
		// pawn factors
		constexpr auto p_base_score = 950;
		constexpr auto p_q_factor = 90;
		constexpr auto p_r_factor = 28;
		constexpr auto p_b_factor = 17;
		constexpr auto p_n_factor = 16;

		// knight factors
		constexpr auto n_base_score = 3510;
		constexpr auto n_q_factor = 50;
		constexpr auto n_r_factor = 35;
		constexpr auto n_b_factor = 7;
		constexpr auto n_n_factor = 8;
		constexpr auto n_p_factor = 22;

		// bishop factors
		constexpr auto b_base_score = 3775;
		constexpr auto b_q_factor = 55;
		constexpr auto b_r_factor = 27;
		constexpr auto b_b_factor = 9;
		constexpr auto b_n_factor = 6;
		constexpr auto b_p_factor = 6;

		// rook factors
		constexpr auto r_base_score = 6295;
		constexpr auto r_q_factor = 261;
		constexpr auto r_r_factor = 217;
		constexpr auto r_b_factor = 34;
		constexpr auto r_n_factor = 40;
		constexpr auto r_p_factor = 10;

		// queen factors
		constexpr auto q_base_score = 11715;
		constexpr auto q_q_factor = 338;
		constexpr auto q_r_factor = 169;
		constexpr auto q_b_factor = 37;
		constexpr auto q_n_factor = 25;
		constexpr auto q_p_factor = 6;

		// bishop pair factors
		constexpr auto bp_base_score = 515;
		constexpr auto bp_q_factor = 41;
		constexpr auto bp_r_factor = 21;
		constexpr auto bp_b_factor = 7;
		constexpr auto bp_n_factor = 7;

		// material imbalance
		constexpr auto up_two_pieces_bonus = 200;
		constexpr auto more_bishops_bonus = 17;
		constexpr auto more_knights_bonus = 17;

		// phase factors
		constexpr auto max_phase = 32;
		constexpr auto r_phase_factor = 3;
		constexpr auto q_phase_factor = 6;

		auto score = 0;

		// bishop pair
		const auto wbp = wbl & wbd;
		const auto bbp = bbl & bbd;

		score += (wp - bp) * (p_base_score - p_q_factor * (wq + bq) - p_r_factor * (wr + br) - p_b_factor * (wb + bb) + p_n_factor * (wn + bn));
		score += (wn - bn) * (n_base_score - n_q_factor * (wq + bq) - n_r_factor * (wr + br) - n_b_factor * (wb + bb) - n_n_factor * (wn + bn) + n_p_factor * (wp + bp));
		score += (wb - bb) * (b_base_score - b_q_factor * (wq + bq) - b_r_factor * (wr + br) - b_b_factor * (wb + bb) - b_n_factor * (wn + bn) + b_p_factor * (wp + bp));
		score += (wr - br) * (r_base_score - r_q_factor * (wq + bq) - r_r_factor * (wr + br) - r_b_factor * (wb + bb) - r_n_factor * (wn + bn) + r_p_factor * (wp + bp));
		score += (wq - bq) * (q_base_score - q_q_factor * (wq + bq) - q_r_factor * (wr + br) - q_b_factor * (wb + bb) - q_n_factor * (wn + bn) + q_p_factor * (wp + bp));
		score += (wbp - bbp) * (bp_base_score - bp_q_factor * (wq + bq) - bp_r_factor * (wr + br) - bp_b_factor * (wb + bb) - bp_n_factor * (wn + bn));

		const auto w_minor_pieces = wb + wn;

		if (const auto b_minor_pieces = bb + bn; w_minor_pieces > b_minor_pieces)
		{
			// white minor piece bonus
			if (w_minor_pieces > b_minor_pieces + 2)
				score += up_two_pieces_bonus;
			// white bishop bonus 
			if (wb > bb)
				score += more_bishops_bonus;
			// white knight bonus 				
			if (wn > bn)
				score -= more_knights_bonus;
		}
		else if (w_minor_pieces < b_minor_pieces)
		{
			// black minor piece bonus			
			if (b_minor_pieces > w_minor_pieces + 2)
				score -= up_two_pieces_bonus;
			// black bishop bonus 
			if (wb < bb)
				score -= more_bishops_bonus;
			// black knight bonus 				
			if (wn < bn)
				score += more_knights_bonus;
		}

		auto phase = wn + bn + (wb + bb) + r_phase_factor * (wr + br) + q_phase_factor * (wq + bq);
		phase = std::min(phase, max_phase);

		return static_cast<int>(score * score_factor);
	}

	// probe material hash table, set game_phase, scale function, etc.
	mat_hash_entry* probe(const position& pos)
	{
		const auto key = pos.material_key() ^ pos.bishop_color_key();
		auto* hash_entry = pos.thread_info()->material_table[key];

		if (hash_entry->key64 == key)
			return hash_entry;

		std::memset(hash_entry, 0, sizeof(mat_hash_entry));
		hash_entry->key64 = key;
		hash_entry->factor[white] = hash_entry->factor[black] = static_cast<uint8_t>(normal_factor);
		hash_entry->game_phase = static_cast<uint8_t>(pos.game_phase());
		hash_entry->conversion = max_factor;
		hash_entry->value_function_index = hash_entry->scale_function_index[white] = hash_entry->scale_function_index[black] = -1;

		hash_entry->value = mat_imbalance(
			pos.number(white, pt_pawn), pos.number(white, pt_knight), pos.number(white, pt_bishop),
			popcnt(pos.pieces(white, pt_bishop) & ~dark_squares), popcnt(pos.pieces(white, pt_bishop) & dark_squares),
			pos.number(white, pt_rook), pos.number(white, pt_queen),
			pos.number(black, pt_pawn), pos.number(black, pt_knight), pos.number(black, pt_bishop),
			popcnt(pos.pieces(black, pt_bishop) & ~dark_squares), popcnt(pos.pieces(black, pt_bishop) & dark_squares),
			pos.number(black, pt_rook), pos.number(black, pt_queen));

		hash_entry->value_function_index = thread_pool.end_games.probe_value(pos.material_key());
		if (hash_entry->value_function_index >= 0)
			return hash_entry;

		for (auto color = white; color <= black; ++color)
			if (!more_than_one(pos.pieces(~color)) && pos.non_pawn_material(color) >= mat_rook)
			{
				hash_entry->value_function_index = color == white ? 0 : 1;
				return hash_entry;
			}

		auto strong_side = num_sides;

		if (const auto scale_factor = thread_pool.end_games.probe_scale_factor(pos.material_key(), strong_side); scale_factor >= 0)
		{
			hash_entry->scale_function_index[strong_side] = scale_factor;
			return hash_entry;
		}

		for (auto color = white; color <= black; ++color)
		{
			if (pos.non_pawn_material(color) == mat_bishop && pos.number(color, pt_bishop) == 1 && pos.number(color, pt_pawn) >= 1)
				hash_entry->scale_function_index[color] = color == white ? 0 : 1;

			else if (!pos.number(color, pt_pawn) && pos.non_pawn_material(color) == mat_queen && pos.number(color, pt_queen) == 1
				&& pos.number(~color, pt_rook) == 1 && pos.number(~color, pt_pawn) >= 1)
				hash_entry->scale_function_index[color] = color == white ? 2 : 3;
		}

		const auto npm_w = pos.non_pawn_material(white);
		const auto npm_b = pos.non_pawn_material(black);

		if (npm_w + npm_b == mat_0 && pos.pieces(pt_pawn))
		{
			if (!pos.number(black, pt_pawn))
			{
				assert(pos.number(white, pt_pawn) >= 2);
				hash_entry->scale_function_index[white] = 4;
			}
			else if (!pos.number(white, pt_pawn))
			{
				assert(pos.number(black, pt_pawn) >= 2);
				hash_entry->scale_function_index[black] = 5;
			}
			else if (pos.number(white, pt_pawn) == 1 && pos.number(black, pt_pawn) == 1)
			{
				hash_entry->scale_function_index[white] = 6;
				hash_entry->scale_function_index[black] = 7;
			}
		}

		if (!pos.number(white, pt_pawn) && npm_w - npm_b <= mat_bishop)
			hash_entry->factor[white] = static_cast<uint8_t>(npm_w < mat_rook ? draw_factor : npm_b <= mat_bishop ? static_cast<sfactor>(6) : static_cast<sfactor>(22));

		if (!pos.number(black, pt_pawn) && npm_b - npm_w <= mat_bishop)
			hash_entry->factor[black] = static_cast<uint8_t>(npm_b < mat_rook ? draw_factor : npm_w <= mat_bishop ? static_cast<sfactor>(6) : static_cast<sfactor>(22));

		if (pos.number(white, pt_pawn) == 1 && npm_w - npm_b <= mat_bishop)
			hash_entry->factor[white] = static_cast<uint8_t>(one_pawn_factor);

		if (pos.number(black, pt_pawn) == 1 && npm_b - npm_w <= mat_bishop)
			hash_entry->factor[black] = static_cast<uint8_t>(one_pawn_factor);

		hash_entry->conversion_is_estimated = true;

		return hash_entry;
	}

	// retrieve endgame value from material hash
	int mat_hash_entry::value_from_function(const position& pos) const
	{
		return (*thread_pool.end_games.value_functions[value_function_index])(pos);
	}

	// retrieve scale factor from material hash
	sfactor mat_hash_entry::scale_factor_from_function(const position& pos, const side color) const
	{
		if (scale_function_index[color] >= 0)
		{
			if (const auto scale_factor = (*thread_pool.end_games.factor_functions[scale_function_index[color]])(pos); scale_factor != no_factor)
				return scale_factor;
		}
		return static_cast<sfactor>(factor[color]);
	}
}
