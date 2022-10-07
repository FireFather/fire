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

#include "pawn.h"

#include "bitboard.h"
#include "fire.h"
#include "position.h"
#include "thread.h"
#include "macro/file.h"
#include "macro/rank.h"
#include "macro/score.h"

// routines for pawn structure evaluation
namespace pawn
{
	template <side me>
	int eval_pawns(const position& pos, pawn_hash_entry* e)
	{
		static constexpr auto center_bind = 4259831;
		static constexpr auto multiple_passed_pawns = 3408076;
		static constexpr auto second_row_fixed = 1114131;

		const auto you = me == white ? black : white;
		const auto second_row = me == white ? rank_2_bb : rank_7_bb;
		const auto center_bind_mask = (file_d_bb | file_e_bb) &
			(me == white ? rank_5_bb | rank_6_bb | rank_7_bb : rank_4_bb | rank_3_bb | rank_2_bb);

		square sq;
		auto score = 0;
		const auto* p_square = pos.piece_list(me, pt_pawn);

		const auto my_pawns = pos.pieces(me, pt_pawn);
		auto your_pawns = pos.pieces(you, pt_pawn);

		e->passed_p[me] = 0;
		e->king_square[me] = no_square;
		e->pawns_sq_color[me][black] = static_cast<uint8_t>(popcnt(my_pawns & dark_squares));
		e->pawns_sq_color[me][white] = static_cast<uint8_t>(pos.number(me, pt_pawn) - e->pawns_sq_color[me][black]);

		while ((sq = *p_square++) != no_square)
		{
			const auto f = file_of(sq);

			const auto neighbor_pawns = my_pawns & get_adjacent_files(f);
			const auto double_pawns = my_pawns & forward_bb(me, sq);
			const bool closed_file = your_pawns & forward_bb(me, sq);
			const auto stoppers = your_pawns & passedpawn_mask(me, sq);
			const auto attackers = your_pawns & pawnattack[me][sq];
			const auto attackers_push = your_pawns & pawnattack[me][square_in_front(me, sq)];
			const auto phalanx = neighbor_pawns & get_rank(sq);
			const auto supported = neighbor_pawns & get_rank(square_behind(me, sq));
			const bool chain = supported | phalanx;
			const auto isolated = !neighbor_pawns;

			bool remaining;
			if (isolated | chain
				|| attackers
				|| my_pawns & pawn_attack_range(you, sq)
				|| relative_rank(me, sq) >= rank_5)
				remaining = false;
			else
			{
				auto b = pawn_attack_range(me, sq) & (my_pawns | your_pawns);
				b = pawn_attack_range(me, sq) & get_rank(rear_square(me, b));
				remaining = (b | shift_up<me>(b)) & your_pawns;
			}

			if (!stoppers && !double_pawns)
			{
				e->passed_p[me] |= sq;
				if (chain)
					score += passed_pawn_values[relative_rank(me, sq)];
			}
			else if (!(stoppers ^ attackers ^ attackers_push)
				&& !double_pawns
				&& popcnt(supported) >= popcnt(attackers)
				&& popcnt(phalanx) >= popcnt(attackers_push))
			{
				score += passed_pawn_values_2[relative_rank(me, sq)];
			}

			if (chain)
				score += chain_score[closed_file][phalanx != 0][popcnt(supported)][relative_rank(me, sq)];

			else if (isolated)
				score -= isolated_pawn[closed_file][f];

			else if (remaining)
				score -= remaining_score[closed_file];

			else
				score -= un_supported_pawn[closed_file];

			if (double_pawns)
				score -= doubled_pawn_distance[f][rank_distance(sq, front_square(me, double_pawns))];

			if (attackers)
				score += pawn_attacker_score[relative_rank(me, sq)];
		}

		uint64_t b = e->half_open_lines[me] ^ 0xFF;
		e->pawn_span[me] = b ? static_cast<uint8_t>(msb(b) - lsb(b)) : 0;

		b = shift_up_left<me>(my_pawns) & shift_up_right<me>(my_pawns) & center_bind_mask;
		score += center_bind * popcnt(b);

		if (more_than_one(e->passed_p[me]))
			score += multiple_passed_pawns;

		const uint64_t begin_blocked_bb = e->p_attack[you] & shift_up<me>(my_pawns & second_row);
		score -= second_row_fixed * popcnt(begin_blocked_bb);

		return score;
	}

	void init()
	{
		for (auto n = 0; n < 17; n++)
			for (auto dist = 0; dist < 8; dist++)
				king_pawn_distance[n][dist] = make_score(0, static_cast<int>(floor(sqrt(static_cast<double>(n)) * 5.0 * dist)));

		for (auto n = 0; n < 8; n++)
		{
			pawn_shield[n] = 8 * pawn_shield_constants[n];
			pawn_storm[n] = 8 * pawn_storm_constants[n];
			storm_half_open_file[n] = 9 * pawn_storm_constants[n];
			attack_on_file[n] = n * pawn_storm[0];
		}

		for (auto closed_file = 0; closed_file <= 1; ++closed_file)
			for (auto phalanx = 0; phalanx <= 1; ++phalanx)
				for (auto point = 0; point <= 2; ++point)
					for (auto rank = rank_2; rank < rank_8; ++rank)
					{
						static constexpr auto chain_div = 2;
						static constexpr auto chain_mult = 3;
						static constexpr auto pawn_unsupported = 5505051;
						auto val = (phalanx ? phalanx_seed[rank] : seed[rank]) >> closed_file;
						val += point == 2 ? 13 : 0;
						chain_score[closed_file][phalanx][point][rank] = ps(chain_mult * val / chain_div, val)
							- (point == 0 ? pawn_unsupported : 0);
					}
	}

	pawn_hash_entry* probe(const position& pos)
	{
		const auto key = pos.pawn_key();
		auto* entry = pos.thread_info()->pawn_table[key];

		if (entry->key == key)
			return entry;

		entry->key = key;

		uint64_t pawn_files_bb[2]{};

		const auto w_pawn = pos.pieces(white, pt_pawn);
		const auto b_pawn = pos.pieces(black, pt_pawn);

		pawn_files_bb[white] = file_front_rear(w_pawn);
		pawn_files_bb[black] = file_front_rear(b_pawn);

		entry->p_attack[white] = pawn_attack<white>(w_pawn);
		entry->p_attack[black] = pawn_attack<black>(b_pawn);

		entry->safe_pawn[white] = ~file_front(entry->p_attack[white]);
		entry->safe_pawn[black] = ~file_behind(entry->p_attack[black]);

		entry->half_open_lines[white] = static_cast<int>(~pawn_files_bb[white]) & 0xFF;
		entry->half_open_lines[black] = static_cast<int>(~pawn_files_bb[black]) & 0xFF;

		entry->asymmetry = popcnt(entry->half_open_lines[white] ^ entry->half_open_lines[black]);

		entry->pscore = eval_pawns<white>(pos, entry);
		entry->pscore -= eval_pawns<black>(pos, entry);

		auto files = static_cast<int>((pawn_files_bb[white] | pawn_files_bb[black]) & 0xFF);
		if (files)
			files = msb(files) - lsb(files);
		entry->file_width = std::max(files - 3, 0);

		entry->conversion_difficult = !((entry->half_open_lines[white] & entry->half_open_lines[black])
			|| entry->half_open_lines[white] & entry->half_open_lines[black] & 0x3c) && !entry->asymmetry;

		auto pawns_bb = pos.pieces(pt_pawn);
		entry->n_pawns = popcnt(pawns_bb);
		if (pawns_bb)
		{
			auto line_sum = 0;
			do
			{
				const auto sq = pop_lsb(&pawns_bb);
				line_sum += sq & 7;
			} while (pawns_bb);
			entry->average_line = line_sum / entry->n_pawns;
		}

		return entry;
	}

	template <side me>
	int eval_shelter_storm(const position& pos, const square square_k)
	{
		const auto you{ me == white ? black : white };

		static constexpr auto file_factor_mult = 64;
		static constexpr auto ss_danger_factor = 3;
		static constexpr auto ss_safety_factor = 3;
		static constexpr auto max_safety_bonus = ev(258);
		static constexpr auto ss_base = 100;

		enum
		{
			not_my_pawn,
			can_move,
			blocked_by_pawn,
			blocked_by_king
		};

		auto b = ranks_forward_bb(me, rank_of(square_k)) | get_rank(square_k);
		const auto my_pawns = b & pos.pieces(me, pt_pawn);
		const auto your_pawns = b & pos.pieces(you, pt_pawn);
		auto danger = eval_0;
		const auto center = std::max(file_b, std::min(file_g, file_of(square_k)));

		for (auto f = center - static_cast<file>(1); f <= center + static_cast<file>(1); ++f)
		{
			b = my_pawns & get_file(f);
			const auto my_rank = b ? relative_rank(me, rear_square(me, b)) : rank_1;

			b = your_pawns & get_file(f);
			const auto your_rank = b ? relative_rank(me, front_square(you, b)) : rank_1;

			danger += shelter_weakness[std::min(f, file_h - f)][my_rank] * shield_factor[std::abs(f - file_of(square_k))];

			const int t_type =
				f == file_of(square_k) && your_rank == relative_rank(me, square_k) + 1
				? blocked_by_king
				: my_rank == rank_1
				? not_my_pawn
				: your_rank == my_rank + 1
				? blocked_by_pawn
				: can_move;

			danger += storm_danger[t_type][std::min(f, file_h - f)][your_rank] * storm_factor[std::abs(f - file_of(square_k))];
		}
		danger /= file_factor_mult;
		return ss_base + max_safety_bonus * ss_safety_factor - danger * ss_danger_factor;
	}

	template <side me>
	score pawn_hash_entry::calculate_king_safety(const position& pos)
	{
		const auto you = me == white ? black : white;

		static constexpr auto safe_bonus_div = 220;
		static constexpr auto safe_bonus_mult = 8;
		static constexpr auto safe_bonus_mult_r34 = 16;
		static constexpr auto safe_bonus_mult_r5 = 8;
		static constexpr auto king_1st_rank = static_cast<score>(-6553876);
		static constexpr auto king_near_enemy_pawns = static_cast<score>(43);

		const auto square_k = pos.king(me);
		king_square[me] = square_k;
		castle_possibilities[me] = static_cast<uint8_t>(pos.castling_possible(me));

		int safe_bonus = eval_shelter_storm<me>(pos, square_k);

		if (pos.castling_possible(me == white ? white_short : black_short))
			safe_bonus = std::max(safe_bonus, eval_shelter_storm<me>(pos, relative_square(me, g1)));
		if (pos.castling_possible(me == white ? white_long : black_long))
			safe_bonus = std::max(safe_bonus, eval_shelter_storm<me>(pos, relative_square(me, c1)));

		const auto third_fourth_rank = me == white ? rank_3_bb | rank_4_bb : rank_6_bb | rank_5_bb;
		const auto fifth_rank = me == white ? rank_5_bb : rank_4_bb;

		const auto attacking_pawns = attack_files[file_of(square_k)] & pos.pieces(you, pt_pawn);

		safety[me] = safe_bonus / safe_bonus_div * safe_bonus_mult
			- safe_bonus_mult_r34 * popcnt(attacking_pawns & third_fourth_rank)
			- safe_bonus_mult_r5 * popcnt(attacking_pawns & fifth_rank);

		auto result = make_score(safe_bonus, 0);

		const auto king_bb = pos.pieces(me, pt_king);

		if (const auto first_rank = me == white ? rank_1_bb : rank_8_bb; king_bb & first_rank)
		{
			if (const uint64_t bb = shift_up<me>(king_bb) | shift_up_left<me>(king_bb) | shift_up_right<me>(king_bb); bb == (pos.pieces(me, pt_pawn) & bb))
				result += king_1st_rank;
		}

		if (pos.pieces(you, pt_pawn) & shift_down<me>(king_bb))
			result += king_near_enemy_pawns;

		if (n_pawns)
			result -= king_pawn_distance[n_pawns][abs(average_line - file_of(pos.king(me)))];

		return result;
	}

	template score pawn_hash_entry::calculate_king_safety<white>(const position& pos);
	template score pawn_hash_entry::calculate_king_safety<black>(const position& pos);
}
