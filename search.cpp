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

#include "search.h"

#include <sstream>

#include "chrono.h"
#include "evaluate.h"
#include "fire.h"
#include "hash.h"
#include "mcts/mcts.h"
#include "movegen.h"
#include "movepick.h"
#include "pragma.h"
#include "thread.h"
#include "uci.h"
#include "util/util.h"

namespace search
{
	void adjust_time_after_ponder_hit()
	{
		main_hash.new_age();
		if (param.use_time_calculating())
			time_control.adjustment_after_ponder_hit();
	}

	// alpha-beta pruning utilizing minimax algorithm, effectively eliminating 'unpromising' branches of the search tree...
	// search time is consequently limited to a 'more promising' subtree, resulting in deeper searches
	template <nodetype nt>
	int alpha_beta(position& pos, int alpha, int beta, int depth, bool cut_node)
	{
		// search constants (can be easily convert int global variables for tuning w/ CLOP, etc.)
		// these values have only been roughly hand tuned

		constexpr auto null_move_tempo_mult = 2;
		constexpr auto null_move_strong_threat_mult = 8;
		constexpr auto null_move_pos_val_less_than_beta_mult = 12;
		constexpr auto null_move_max_depth = 8;

		constexpr auto no_early_pruning_strong_threat_min_depth = 8;
		constexpr auto no_early_pruning_non_pv_node_depth_min = 8;
		constexpr auto no_early_pruning_position_value_margin = 102;

		constexpr auto only_quiet_check_moves_max_depth = 8;

		constexpr auto late_move_count_max_depth = 16;

		constexpr auto quiet_moves_max_gain_base = -44;
		constexpr auto sort_cmp_sort_value = -200;

		const auto pv_node = nt == PV;
		constexpr auto max_quiet_moves = 64;

		assert(-max_score <= alpha && alpha < beta&& beta <= max_score);
		assert(pv_node || alpha == beta - 1);
		assert(depth >= plies && depth < max_depth);

		uint32_t quiet_moves[max_quiet_moves];
		main_hash_entry* hash_entry = nullptr;

		uint64_t key64 = 0;

		auto hash_move = no_move;
		auto move = no_move;
		auto best_move = no_move;
		auto extension = no_depth;
		auto new_depth = no_depth;
		auto predicted_depth = no_depth;
		auto hash_value = no_score;
		auto eval = no_score;
		auto state_check = false;
		auto gives_check = false;
		auto progress = false;
		auto capture_or_promotion = false;
		auto full_search_needed = false;
		auto moved_piece = no_piece;
		auto move_number = 0;
		auto quiet_move_number = 0;

		auto* pi = pos.info();
		const auto root_node = pv_node && pi->ply == 1;

		auto* my_thread = pos.my_thread();
		state_check = pi->in_check;
		move_number = 0;
		quiet_move_number = 0;
		pi->move_number = 0;

		if (my_thread == thread_pool.main())
		{
			if (auto* main_thread = static_cast<mainthread*>(my_thread); ++main_thread->interrupt_counter >= 4096)
			{
				if (main_thread->quick_move_evaluation_busy)
				{
					if (main_thread->quick_move_evaluation_stopped)
						return alpha;
					if (auto elapsed = time_control.elapsed(); elapsed > 1000 || elapsed > time_control.optimum() / 16)
					{
						main_thread->quick_move_evaluation_stopped = true;
						return alpha;
					}
				}
				send_time_info();
				main_thread->interrupt_counter = 0;
			}
		}

		if (!root_node)
		{
			if (signals.stop_analyzing.load(std::memory_order_relaxed) || pi->move_repetition || pi->ply >= max_ply)
				return pi->ply >= max_ply && !state_check
				? evaluate::eval(pos)
				: draw[pos.on_move()];

			alpha = std::max(gets_mated(pi->ply), alpha);
			beta = std::min(gives_mate(pi->ply + 1), beta);
			if (alpha >= beta)
				return alpha;
		}

		assert(1 <= pi->ply && pi->ply < max_ply);

		best_move = no_move;
		(pi + 2)->killers[0] = (pi + 2)->killers[1] = no_move;
		pi->stats_value = sort_max;
		if (pi->excluded_move)
		{
			hash_entry = nullptr;
			hash_move = no_move;
			hash_value = no_score;
			key64 = 0;
			goto view_all_moves;
		}

		key64 = pi->key;
		key64 ^= pos.draw50_key();
		hash_entry = main_hash.probe(key64);
		hash_value = hash_entry ? value_from_hash(hash_entry->value(), pi->ply) : no_score;
		hash_move = root_node
			? my_thread->root_moves[my_thread->active_pv].pv[0]
			: hash_entry
			? hash_entry->move()
			: no_move;

		if (!pv_node
			&& hash_value != no_score
			&& hash_entry->depth() >= depth
			&& (hash_value >= beta
				? hash_entry->bounds() & south_border
				: hash_entry->bounds() & north_border))
		{
			if (hash_move)
			{
				if (hash_value >= beta)
					update_stats(pos, state_check, hash_move, depth, nullptr, 0);
				else
					update_stats_minus(pos, state_check, hash_move, depth);
			}

			return hash_value;
		}

		if (!root_node && tb_number && depth >= tb_probe_depth && pos.material_or_castle_changed())
		{
			if (auto number_pieces = pos.total_num_pieces(); number_pieces <= tb_number
				&& depth >= tb_probe_depth
				&& !pos.castling_possible(all))
			{
				auto value = no_score;
				if (number_pieces <= egtb::max_pieces_wdl)
					value = egtb::egtb_probe_wdl(pos);
				else if (number_pieces <= egtb::max_pieces_dtm)
					value = egtb::egtb_probe_dtm(pos);
				else
					value = no_score;

				if (value != no_score)
				{
					hash_entry = main_hash.replace(key64);
					hash_entry->save(key64, value_to_hash(value, pi->ply), exact_value,
						std::min(max_depth - plies, depth + 6 * plies),
						no_move, no_score, main_hash.age());

					return value;
				}
			}
		}

		if (state_check)
		{
			pi->position_value = no_score;
			goto view_all_moves;
		}

		if (hash_entry && hash_entry->eval() != no_score)
		{
			eval = pi->position_value = hash_entry->eval();
			pi->strong_threat = hash_entry->threat();

			if (hash_value != no_score)
				if (hash_entry->bounds() & (hash_value > eval ? south_border : north_border))
					eval = hash_value;
		}
		else
		{
			if (pi->previous_move != null_move)
				eval = evaluate::eval(pos);
			else
				eval = evaluate::eval_after_null_move((pi - 1)->position_value);

			pi->position_value = eval;
			if (pi->eval_is_exact && !root_node)
				return eval;

			hash_entry = main_hash.replace(key64);
			hash_entry->save(key64, no_score, no_limit + pi->strong_threat, no_depth, no_move,
				pi->position_value, main_hash.age());
		}

		if (pi->previous_move != null_move
			&& (pi - 1)->position_value != no_score
			&& pi->material_key == (pi - 1)->material_key)
		{
			constexpr auto max_gain_value = 500;
			constexpr auto max_gain = static_cast<int>(max_gain_value);
			auto gain = -pi->position_value - (pi - 1)->position_value + 2 * value_tempo;
			gain = std::min(max_gain, std::max(-max_gain, gain));
			pos.thread_info()->max_gain_table.update(pi->moved_piece, pi->previous_move, gain);
		}

		if (pi->no_early_pruning)
			goto no_early_pruning;

		if (constexpr auto razoring_min_depth = 4; !pv_node
			&& depth < razoring_min_depth * plies
			&& !hash_move
			&& eval + razor_margin <= alpha)
		{
			if (constexpr auto razoring_qs_min_depth = 2; depth < razoring_qs_min_depth * plies)
				return q_search<nonPV, false>(pos, alpha, beta, depth_0);

			auto r_alpha = alpha - razor_margin;
			if (auto val = q_search<nonPV, false>(pos, r_alpha, r_alpha + score_1, depth_0); val <= r_alpha)
				return val;
		}

		if (constexpr auto futility_min_depth = 7; !root_node
			&& depth < futility_min_depth * plies
			&& eval - futility_margin(depth) >= beta
			&& eval < win_score
			&& pi->non_pawn_material[pos.on_move()])
			return eval - futility_margin(depth);

		if (constexpr auto null_move_min_depth = 2; !pv_node
			&& depth >= null_move_min_depth * plies
			&& eval >= beta + null_move_tempo_mult * value_tempo
			&& (!pi->strong_threat || depth >= null_move_strong_threat_mult * plies)
			&& (pi->position_value >= beta || depth >= null_move_pos_val_less_than_beta_mult * plies)
			&& pi->non_pawn_material[pos.on_move()]
			&& (!thread_pool.analysis_mode || depth < null_move_max_depth * plies))
		{
			constexpr auto null_move_depth_greater_than_cut_node_mult = 15;
			constexpr auto null_move_depth_greater_than_sub = 20;
			constexpr auto null_move_depth_greater_than_div = 204;
			constexpr auto null_move_depth_greater_than_mult = 310;
			constexpr auto null_move_tm_mult = 66;
			constexpr auto null_move_tm_base = 540;
			assert(eval - beta >= 0);

			auto R = no_depth;
			R = depth < 4 * plies ? depth : (null_move_tm_base + null_move_tm_mult * (depth / plies)
				+ std::max(std::min(null_move_depth_greater_than_mult * (eval - beta) / static_cast<int>(null_move_depth_greater_than_div)
				- null_move_depth_greater_than_sub - null_move_depth_greater_than_cut_node_mult
				* cut_node - null_move_depth_greater_than_cut_node_mult * (hash_move != no_move), 3 * 256), 0)) / 256 * plies;

			pi->mp_end_list = (pi - 1)->mp_end_list;
			pos.play_null_move();
			(pi + 1)->no_early_pruning = true;
			auto value = depth - R < plies
				? -q_search<nonPV, false>(pos, -beta, -beta + score_1, depth_0)
				: -alpha_beta<nonPV>(pos, -beta, -beta + score_1, depth - R, !cut_node);
			(pi + 1)->no_early_pruning = false;
			pos.take_null_back();

			if (constexpr auto value_less_than_beta_margin = 100; (pi - 1)->lmr_reduction && (value < beta - static_cast<int>(value_less_than_beta_margin) || value < -longest_mate_score))
			{
				if ((pi - 1)->lmr_reduction <= 2 * plies)
					return beta - score_1;
				depth += 2 * plies;
			}

			if (value >= beta)
			{
				if (value >= longest_mate_score)
					value = beta;

				if (constexpr auto value_greater_than_beta_max_depth = 12; depth < value_greater_than_beta_max_depth * plies && abs(beta) < win_score)
					return value;

				pi->no_early_pruning = true;
				auto val = depth - R < plies
					? q_search<nonPV, false>(pos, beta - score_1, beta, depth_0)
					: alpha_beta<nonPV>(pos, beta - score_1, beta, depth - R, false);
				pi->no_early_pruning = false;

				if (val >= beta)
					return value;
			}
		}
		else if (constexpr auto dummy_null_move_threat_min_depth_mult = 6; thread_pool.dummy_null_move_threat && depth >= dummy_null_move_threat_min_depth_mult * plies && eval >= beta && (pi - 1)->lmr_reduction)
		{
			pi->mp_end_list = (pi - 1)->mp_end_list;
			pos.play_null_move();
			(pi + 1)->no_early_pruning = true;
			auto null_beta = beta - 240;
			auto value = -alpha_beta<nonPV>(pos, -null_beta, -null_beta + score_1, depth / 2 - 2 * plies, false);
			(pi + 1)->no_early_pruning = false;
			pos.take_null_back();

			if (value < null_beta)
			{
				if ((pi - 1)->lmr_reduction <= 2 * plies)
					return beta - score_1;
				depth += 2 * plies;
			}
		}

	no_early_pruning:

		if (constexpr auto no_early_pruning_min_depth = 5; !pv_node
			&& depth >= no_early_pruning_min_depth * plies
			&& (depth >= no_early_pruning_strong_threat_min_depth * plies || pi->strong_threat & pos.on_move() + 1)
			&& abs(beta) < longest_mate_score)
		{
			constexpr auto limit_depth_min = 8;
			constexpr auto pc_beta_depth_margin = 4;
			constexpr auto pc_beta_margin = 160;
			auto pc_beta = beta + static_cast<int>(pc_beta_margin);
			auto pc_depth = depth - pc_beta_depth_margin * plies;

			assert(pc_depth >= plies);
			assert(pi->previous_move != no_move);

			auto s_limit = depth >= limit_depth_min * plies
				? position::see_values()[pi->captured_piece]
				: std::max(see_0, (pc_beta - pi->position_value) / 2);

			movepick::init_prob_cut(pos, hash_move, s_limit);

			while ((move = movepick::pick_move(pos)) != no_move)
			{
				if (pos.legal_move(move))
				{
					pos.play_move(move);
					auto value = -alpha_beta<nonPV>(pos, -pc_beta, -pc_beta + score_1, pc_depth, !cut_node);
					pos.take_move_back(move);
					if (value >= pc_beta)
						return value;
				}
			}
		}

		if (constexpr auto no_early_pruning_pv_node_depth_min = 5; depth >= (pv_node ? no_early_pruning_pv_node_depth_min * plies : no_early_pruning_non_pv_node_depth_min * plies)
			&& !hash_move
			&& (pv_node || cut_node || pi->position_value + static_cast<int>(no_early_pruning_position_value_margin) >= beta))
		{
			auto d = depth - 2 * plies - (pv_node ? depth_0 : static_cast<uint32_t>(depth) / plies / 4 * plies);
			pi->no_early_pruning = true;
			alpha_beta<nt>(pos, alpha, beta, d, !pv_node && cut_node);
			pi->no_early_pruning = false;

			hash_entry = main_hash.probe(key64);
			hash_move = hash_entry ? hash_entry->move() : no_move;
		}

	view_all_moves:
		const counter_move_values* cmh = pi->move_counter_values;
		const counter_move_values* fmh = (pi - 1)->move_counter_values;
		const counter_move_values* fmh2 = (pi - 3)->move_counter_values;

		auto only_quiet_check_moves = !root_node && depth < only_quiet_check_moves_max_depth* plies
			&& pi->position_value + futility_margin_ext(depth - plies) <= alpha;

		movepick::init_search(pos, hash_move, depth, only_quiet_check_moves);

		auto best_score = -max_score;

		progress = pi->position_value >= (pi - 2)->position_value
			|| (pi - 2)->position_value == no_score;

		auto late_move_count = depth < late_move_count_max_depth* plies ? late_move_number(depth, progress) : 999;
		if (only_quiet_check_moves)
			late_move_count = 1;
		bool discovered_check_possible = pos.discovered_check_possible();

		while ((move = movepick::pick_move(pos)) != no_move)
		{
			constexpr auto excluded_move_hash_depth_reduction = 3;
			assert(piece_color(pos.moved_piece(move)) == pos.on_move());

			if (move == pi->excluded_move)
				continue;

			if (root_node && my_thread->root_moves.find(move) < my_thread->active_pv)
				continue;

			pi->move_number = ++move_number;

			if (!bench_active && root_node && my_thread == thread_pool.main())
			{
				if (constexpr auto info_currmove_interval = 4000; time_control.elapsed() > info_currmove_interval)
					acout() << "info currmove " << util::move_to_string(move, pos) << " currmovenumber " << move_number + my_thread->active_pv << std::endl;
			}

			if (pv_node)
				(pi + 1)->pv = nullptr;

			capture_or_promotion = pos.capture_or_promotion(move);
			moved_piece = pos.moved_piece(move);

			gives_check = move < static_cast<uint32_t>(castle_move) && !discovered_check_possible
				? pi->check_squares[piece_type(moved_piece)] & to_square(move)
				: pos.give_check(move);

			extension = depth_0;

			if (gives_check
				&& (pi->mp_stage == good_captures || move_number < late_move_count)
				&& (pi->mp_stage == good_captures || pos.see_test(move, see_0)))
				extension = plies;

			if (constexpr auto excluded_move_min_depth = 8; !root_node
				&& move == hash_move
				&& depth >= excluded_move_min_depth * plies
				&& hash_entry->bounds() & south_border
				&& hash_entry->depth() >= depth - excluded_move_hash_depth_reduction * plies
				&& extension < plies
				&& abs(hash_value) < win_score
				&& pos.legal_move(move))
			{
				constexpr auto excluded_move_r_beta_hash_value_margin_div = 5;
				constexpr auto excluded_move_r_beta_hash_value_margin_mult = 8;
				auto cm = pi->mp_counter_move;
				auto r_beta = hash_value - static_cast<int>(static_cast<uint32_t>(depth) / plies * excluded_move_r_beta_hash_value_margin_mult / excluded_move_r_beta_hash_value_margin_div);
				auto r_depth = depth / plies / 2 * plies;
				pi->excluded_move = move;

				if (auto value = alpha_beta<nonPV>(pos, r_beta - score_1, r_beta, r_depth, !pv_node && cut_node); value < r_beta)
					extension = plies;

				pi->excluded_move = no_move;

				movepick::init_search(pos, hash_move, depth, false);
				pi->mp_counter_move = cm;
				++pi->mp_stage;
				pi->move_number = move_number;
			}

			new_depth = depth - plies + extension;
			if (!(root_node || capture_or_promotion || gives_check)
				&& best_score > -longest_mate_score
				&& !pos.advanced_pawn(move)
				&& pi->non_pawn_material[pos.on_move()])
			{
				constexpr auto predicted_depth_see_test_mult = 20;
				constexpr auto predicted_depth_max_depth = 7;
				if (move_number >= late_move_count)
					continue;

				if (constexpr auto quiet_moves_max_depth = 6; depth < quiet_moves_max_depth * plies
					&& pi->mp_stage >= quietmoves)
				{
					auto offset = counter_move_values::calculate_offset(moved_piece, to_square(move));

					if (constexpr auto sort_cmp = static_cast<int>(sort_cmp_sort_value); (!cmh || cmh->value_at_offset(offset) < sort_cmp)
						&& (!fmh || fmh->value_at_offset(offset) < sort_cmp)
						&& (cmh && fmh || !fmh2 || fmh2->value_at_offset(offset) < sort_cmp))
						continue;

					if (constexpr auto quiet_moves_max_gain_mult = 12; pos.thread_info()->max_gain_table.get(moved_piece, move) < static_cast<int>(quiet_moves_max_gain_base)
						- static_cast<int>(quiet_moves_max_gain_mult) * static_cast<int>(static_cast<uint32_t>(depth) / plies))
						continue;
				}

				predicted_depth = std::max(new_depth - lmr_reduction(pv_node, progress, depth, move_number), depth_0);

				if (predicted_depth < predicted_depth_max_depth * plies
					&& pi->position_value + futility_margin_ext(predicted_depth) <= alpha)
					continue;

				if (constexpr auto predicted_depth_see_test_base = 300; predicted_depth < predicted_depth_max_depth * plies
					&& !pos.see_test(move, std::min(see_0, predicted_depth_see_test_base
					- predicted_depth_see_test_mult * predicted_depth * predicted_depth / 64)))
					continue;
			}

			else if (constexpr auto non_root_node_max_depth = 7; !root_node
				&& depth < non_root_node_max_depth * plies
				&& best_score > -longest_mate_score)
			{
				constexpr auto non_root_node_see_test_mult = 20;
				if (constexpr auto non_root_node_see_test_base = 150; pi->mp_stage != good_captures && extension != plies
					&& !pos.see_test(move, std::min(see_knight
					- see_bishop, non_root_node_see_test_base
					- non_root_node_see_test_mult * depth * depth / 64)))
					continue;
			}

			if (!root_node && !pos.legal_move(move))
			{
				pi->move_number = --move_number;
				continue;
			}

			pos.play_move(move, gives_check);

			auto value = score_0;

			if (constexpr auto lmr_min_depth = 3; depth >= lmr_min_depth * plies
				&& move_number > 1
				&& !capture_or_promotion)
			{
				constexpr auto r_stats_value_mult_div = 8;
				constexpr auto r_stats_value_div = 2048;
				constexpr auto stats_value_sort_value_add = 2000;
				auto r = lmr_reduction(pv_node, progress, depth, move_number);

				if (!pv_node && cut_node)
					r += 2 * plies;

				if (piece_type(moved_piece) >= pt_knight
					&& !pos.see_test(make_move(to_square(move), from_square(move)), see_0))
					r -= 2 * plies;

				auto offset = move_value_stats::calculate_offset(moved_piece, to_square(move));
				auto stats_value = static_cast<int>(state_check
					? pos.thread_info()->evasion_history.value_at_offset(offset)
					: pos.thread_info()->history.value_at_offset(offset))
					+ (cmh ? static_cast<int>(cmh->value_at_offset(offset)) : sort_zero)
					+ (fmh ? static_cast<int>(fmh->value_at_offset(offset)) : sort_zero)
					+ (fmh2 ? static_cast<int>(fmh2->value_at_offset(offset)) : sort_zero);
				stats_value += static_cast<int>(stats_value_sort_value_add);

				r -= stats_value / r_stats_value_div * (plies / r_stats_value_mult_div);

				pi->stats_value = stats_value;
				if ((pi - 1)->stats_value != sort_max)
					r -= std::min(plies, std::max(-plies, (stats_value - (pi - 1)->stats_value) / 4096 * (plies / 8)));

				r = std::max(r, depth_0);
				auto d = std::max(new_depth - r, plies);
				pi->lmr_reduction = static_cast<uint8_t>(new_depth - d);

				value = -alpha_beta<nonPV>(pos, -(alpha + score_1), -alpha, d, true);

				if (constexpr auto lmr_reduction_min = 5; value > alpha && pi->lmr_reduction >= lmr_reduction_min * plies)
				{
					pi->lmr_reduction = static_cast<uint8_t>(lmr_reduction_min * plies / 2);
					value = -alpha_beta<nonPV>(pos, -(alpha + score_1), -alpha, new_depth - lmr_reduction_min * plies / 2, true);
				}

				full_search_needed = value > alpha && pi->lmr_reduction != 0;
				pi->lmr_reduction = 0;
			}
			else
			{
				pi->stats_value = sort_max;
				full_search_needed = !pv_node || move_number > 1;
			}

			if (full_search_needed)
			{
				value = new_depth < plies
					? gives_check
					? -q_search<nonPV, true>(pos, -(alpha + score_1), -alpha, depth_0)
					: -q_search<nonPV, false>(pos, -(alpha + score_1), -alpha, depth_0)
					: -alpha_beta<nonPV>(pos, -(alpha + score_1), -alpha, new_depth, pv_node || !cut_node);
			}

			if (pv_node && (move_number == 1 || value > alpha && (root_node || value < beta)))
			{
				uint32_t pv[max_ply + 1];
				(pi + 1)->pv = pv;
				(pi + 1)->pv[0] = no_move;

				if (new_depth < plies && !(pi->ply & 1))
					new_depth = plies;

				value = new_depth < plies
					? gives_check
					? -q_search<PV, true>(pos, -beta, -alpha, depth_0)
					: -q_search<PV, false>(pos, -beta, -alpha, depth_0)
					: -alpha_beta<PV>(pos, -beta, -alpha, new_depth, false);
			}

			pos.take_move_back(move);

			assert(value > -max_score && value < max_score);

			if (signals.stop_analyzing.load(std::memory_order_relaxed))
				return alpha;

			if (my_thread == thread_pool.main() && static_cast<mainthread*>(my_thread)->quick_move_evaluation_stopped)
				return alpha;

			if (root_node)
			{
				auto move_index = my_thread->root_moves.find(move);

				if (auto& root_move = my_thread->root_moves.moves[move_index]; move_number == 1 || value > alpha)
				{
					root_move.score = value;
					root_move.pv.resize(1);
					root_move.depth = depth;

					assert((pi + 1)->pv);

					for (auto* z = (pi + 1)->pv; *z != no_move; ++z)
						root_move.pv.add(*z);

					if (move_number > 1 && my_thread == thread_pool.main())
						static_cast<mainthread*>(my_thread)->best_move_changed += 1024;

					if (!bench_active && my_thread == thread_pool.main())
						acout() << print_pv(pos, alpha, beta, my_thread->active_pv, move_index) << std::endl;
				}
				else
					root_move.score = -max_score;
			}

			if (value > best_score)
			{
				best_score = value;

				if (value > alpha)
				{
					if (pv_node
						&& my_thread == thread_pool.main()
						&& easy_move.expected_move(pi->key)
						&& (move != easy_move.expected_move(pi->key) || move_number > 1))
						easy_move.clear();

					best_move = move;

					if (pv_node && !root_node)
						copy_pv(pi->pv, move, (pi + 1)->pv);

					if (pv_node && value < beta)
						alpha = value;
					else
					{
						assert(value >= beta);
						break;
					}
				}
			}

			if (!capture_or_promotion && move != best_move && quiet_move_number < max_quiet_moves)
				quiet_moves[quiet_move_number++] = move;
		}

		if (best_score == -max_score)
			best_score = pi->excluded_move
			? alpha
			: state_check
			? gets_mated(pi->ply)
			: draw[pos.on_move()];

		else if (best_move)
		{
			update_stats(pos, state_check, best_move, depth, quiet_moves, quiet_move_number);
		}

		else
		{
			constexpr auto fail_low_counter_move_bonus_min_depth_margin = 30;
			if (constexpr auto fail_low_counter_move_bonus_min_depth_mult = 3; depth >= fail_low_counter_move_bonus_min_depth_mult * plies && pi->position_value >= alpha - static_cast<int>(fail_low_counter_move_bonus_min_depth_margin))
				update_stats_quiet(pos, state_check, depth, quiet_moves, quiet_move_number);

			if (constexpr auto fail_low_counter_move_bonus_min_depth = 3; depth >= fail_low_counter_move_bonus_min_depth * plies
				&& !state_check
				&& !pi->captured_piece
				&& pi->move_counter_values)
			{
				if (constexpr auto counter_move_bonus_min_depth = 18; depth < counter_move_bonus_min_depth * plies)
				{
					auto bonus = counter_move_value(depth);
					auto offset = counter_move_values::calculate_offset(pi->moved_piece, to_square(pi->previous_move));

					if ((pi - 1)->move_counter_values)
						(pi - 1)->move_counter_values->update_plus(offset, bonus);

					if ((pi - 2)->move_counter_values)
						(pi - 2)->move_counter_values->update_plus(offset, bonus);

					if ((pi - 4)->move_counter_values)
						(pi - 4)->move_counter_values->update_plus(offset, bonus);
				}
			}
		}

		if (!pi->excluded_move)
		{
			hash_entry = main_hash.replace(key64);
			hash_entry->save(key64, value_to_hash(best_score, pi->ply),
				(best_score >= beta ? south_border : pv_node && best_move ? exact_value : north_border) + pi->strong_threat,
				depth, best_move, pi->position_value, main_hash.age());
		}

		return best_score;
	}

	void calculate_egtb_mate_pv(position& pos, const int expected_score, principal_variation& pv)
	{
		auto score = no_score;

		for (const auto& move : legal_move_list(pos))
		{
			pos.play_move(move);

			if (const auto number_pieces = pos.total_num_pieces(); number_pieces <= egtb::max_pieces_dtm)
				score = egtb::egtb_probe_dtm(pos);

			if (score == expected_score)
			{
				pv.add(move);
				calculate_egtb_mate_pv(pos, -score, pv);
				pos.take_move_back(move);
				return;
			}
			pos.take_move_back(move);
		}
	}

	int calculate_egtb_use(const position& pos)
	{
		const auto wq = pos.number(white, pt_queen);
		const auto bq = pos.number(black, pt_queen);
		const auto wr = pos.number(white, pt_rook);
		const auto br = pos.number(black, pt_rook);
		const auto wb = pos.number(white, pt_bishop);
		const auto bb = pos.number(black, pt_bishop);
		const auto wp = pos.number(white, pt_knight);
		const auto bp = pos.number(black, pt_knight);
		const auto w_pawn = pos.number(white, pt_pawn);
		const auto b_pawn = pos.number(black, pt_pawn);

		if (const auto tot_number = pos.total_num_pieces(); tot_number == 4)
		{
			if (wq == 1 && b_pawn == 1
				|| bq == 1 && w_pawn == 1
				|| wr == 1 && b_pawn == 1
				|| br == 1 && w_pawn == 1
				|| wq == 1 && br == 1
				|| bq == 1 && wr == 1
				|| w_pawn == 1 && b_pawn == 1)
				return egtb_helpful;
		}
		else if (tot_number == 5)
		{
			if (wq == 1 && bq == 1 && w_pawn + b_pawn == 1
				|| wr == 1 && br == 1 && w_pawn + b_pawn == 1
				|| wb + wp == 1 && bb + bp == 1 && w_pawn + b_pawn == 1
				|| wp == 2 && b_pawn == 1
				|| bp == 2 && w_pawn == 1
				|| wq == 1 && br == 1 && b_pawn == 1
				|| bq == 1 && wr == 1 && w_pawn == 1
				|| w_pawn == 2 && b_pawn == 1
				|| w_pawn == 1 && b_pawn == 2
				|| wb + wp + bb + bp == 1 && w_pawn == 1 && b_pawn == 1)
				return egtb_helpful;
		}
		else if (tot_number == 6)
		{
			if (wq == 1 && bq == 1 && w_pawn + b_pawn == 2
				|| wr == 1 && br == 1 && w_pawn + b_pawn == 2
				|| wb + wp == 1 && bb + bp == 1 && w_pawn + b_pawn == 2
				|| (wr == 1 && bb + bp == 1
				|| wb + wp == 1 && br == 1) && w_pawn == 1 && b_pawn == 1
				|| wq == 1 && br == 1 && b_pawn == 2
				|| bq == 1 && wr == 1 && w_pawn == 2
				|| w_pawn == 2 && b_pawn == 2
				|| wq == 1 && bb == 1 && bp == 1 && b_pawn == 1
				|| bq == 1 && wb == 1 && wp == 1 && w_pawn == 1
				)
				return egtb_helpful;
		}
		return egtb_not_helpful;
	}

	void copy_pv(uint32_t* pv, const uint32_t move, uint32_t* pv_lower)
	{
		*pv++ = move;
		if (pv_lower)
			while (*pv_lower != no_move)
				*pv++ = *pv_lower++;
		*pv = no_move;
	}

	void init()
	{
		std::memset(lm_reductions, 0, sizeof lm_reductions);

		for (int d = plies; d < 64 * plies; ++d)
			for (auto n = 2; n < 64; ++n)
			{
				const auto rr = log(static_cast<double>(d) / plies) * log(n) / 2 * static_cast<int>(plies);
				if (rr < 6.4)
					continue;
				const auto r = static_cast<int>(std::lround(rr));

				lm_reductions[false][1][d][n] = static_cast<uint8_t>(r);
				lm_reductions[false][0][d][n] = static_cast<uint8_t>(r + (r < 2 * plies ? 0 * plies : plies));
				lm_reductions[true][1][d][n] = static_cast<uint8_t>(std::max(r - plies, depth_0));
				lm_reductions[true][0][d][n] = lm_reductions[true][1][d][n];
			}

		for (auto d = 1; d < max_ply; ++d)
		{
			constexpr auto counter_move_bonus_value = 24;
			counter_move_bonus[d] = static_cast<int>(std::min(8192, counter_move_bonus_value * (d * d + 2 * d - 2)));
		}
	}

	// start at the leaf nodes of the main search and resolve all tactics/capture moves in "quiet" positions.
	// extend search at all unstable nodes & perform an extension of the evaluation function in order to obtain a static
	// evaluation ... greatly mitigating the effect of the horizon problem.
	template <nodetype nt, bool state_check>
	int q_search(position& pos, int alpha, const int beta, const int depth)
	{
		constexpr auto qs_futility_value_0 = 102;
		constexpr auto qs_futility_value_1 = 0;
		constexpr auto qs_futility_value_2 = 308;
		constexpr auto qs_futility_value_3 = 818;
		constexpr auto qs_futility_value_4 = 827;
		constexpr auto qs_futility_value_5 = 1186;
		constexpr auto qs_futility_value_6 = 2228;
		constexpr auto qs_futility_value_7 = 0;

		constexpr int q_search_futility_value[num_pieces] =
		{
			qs_futility_value_0, qs_futility_value_1, qs_futility_value_2, qs_futility_value_3,
			qs_futility_value_4, qs_futility_value_5, qs_futility_value_6, qs_futility_value_7,
			qs_futility_value_0, qs_futility_value_1, qs_futility_value_2, qs_futility_value_3,
			qs_futility_value_4, qs_futility_value_4, qs_futility_value_6, qs_futility_value_7
		};

		constexpr auto qs_stats_value_sortvalue = -12000;
		
		const auto pv_node = nt == PV;

		assert(alpha >= -max_score && alpha < beta&& beta <= max_score);
		assert(pv_node || alpha == beta - 1);
		assert(depth <= depth_0);

		uint32_t move;
		int best_value;
		int futility_basis;
		int orig_alpha = {};

		auto* pi = pos.info();

		if (pv_node)
		{
			uint32_t pv[max_ply + 1];
			orig_alpha = alpha;
			(pi + 1)->pv = pv;
			pi->pv[0] = no_move;
		}

		auto best_move = no_move;

		if (pi->move_repetition || pi->ply >= max_ply)
			return pi->ply >= max_ply && !state_check
			? evaluate::eval(pos)
			: draw[pos.on_move()];

		assert(0 <= pi->ply && pi->ply < max_ply);

		const auto hash_depth = state_check || depth == depth_0 ? depth_0 : -plies;

		auto key64 = pi->key;
		key64 ^= pos.draw50_key();
		auto* hash_entry = main_hash.probe(key64);
		const auto hash_move = hash_entry ? hash_entry->move() : no_move;
		const auto hash_value = hash_entry ? value_from_hash(hash_entry->value(), pi->ply) : no_score;

		if (!pv_node
			&& hash_value != no_score
			&& hash_entry->depth() >= hash_depth
			&& (hash_value >= beta ? hash_entry->bounds() & south_border : hash_entry->bounds() & north_border))
		{
			return hash_value;
		}

		if (tb_number && tb_probe_depth <= 0 && pos.material_or_castle_changed()
			&& pos.total_num_pieces() <= egtb::max_pieces_wdl
			&& calculate_egtb_use(pos) == egtb_helpful
			&& !pos.castling_possible(all))
		{
			if (const auto value = egtb::egtb_probe_wdl(pos); value != no_score)
				return value;
		}

		if (state_check)
		{
			pi->position_value = no_score;
			best_value = futility_basis = -max_score;
		}
		else
		{
			if (hash_entry && hash_entry->eval() != no_score)
			{
				pi->position_value = best_value = hash_entry->eval();
				pi->strong_threat = hash_entry->threat();

				if (hash_value != no_score)
					if (hash_entry->bounds() & (hash_value > best_value ? south_border : north_border))
						best_value = hash_value;

				if (best_value >= beta)
					return best_value;
			}
			else
			{
				if (pi->previous_move != null_move)
					best_value = evaluate::eval(pos);
				else
					best_value = evaluate::eval_after_null_move((pi - 1)->position_value);

				pi->position_value = best_value;
				if (pi->eval_is_exact)
					return best_value;

				if (best_value >= beta)
				{
					hash_entry = main_hash.replace(key64);
					hash_entry->save(key64, value_to_hash(best_value, pi->ply), south_border + pi->strong_threat,
						no_depth, no_move, pi->position_value, main_hash.age());
					return best_value;
				}
			}

			if (pv_node && best_value > alpha)
				alpha = best_value;

			futility_basis = best_value;
		}

		movepick::init_q_search(pos, hash_move, depth, to_square(pi->previous_move));

		while ((move = movepick::pick_move(pos)) != no_move)
		{
			assert(is_ok(move));

			bool gives_check = move < static_cast<uint32_t>(castle_move) && !pos.discovered_check_possible()
				? pi->check_squares[piece_type(pos.moved_piece(move))] & to_square(move)
				: pos.give_check(move);

			if (!state_check
				&& !gives_check
				&& futility_basis > -win_score
				&& !pos.advanced_pawn(move))
			{
				constexpr auto qs_futility_value_capture_history_add_div = 32;
				assert(move_type(move) != enpassant);

				const auto capture = pos.piece_on_square(to_square(move));
				auto futility_value = futility_basis + q_search_futility_value[capture];
				futility_value += pos.thread_info()->capture_history[capture][to_square(move)] / qs_futility_value_capture_history_add_div;

				if (futility_value <= alpha)
				{
					best_value = std::max(best_value, futility_value);
					continue;
				}

				if (constexpr auto qs_futility_basis_margin = 102; futility_basis + static_cast<int>(qs_futility_basis_margin) <= alpha)
				{
					if (!pos.see_test(move, 1))
					{
						best_value = std::max(best_value, futility_basis + static_cast<int>(qs_futility_basis_margin));
						continue;
					}
					goto skip_see_test;
				}
			}

			if (state_check)
			{
				if (best_value > -longest_mate_score
					&& !pos.is_capture_move(move)
					&& move < static_cast<uint32_t>(promotion_p))
				{
					const auto moved_piece = pos.moved_piece(move);

					if (!gives_check)
					{
						const auto offset = counter_move_values::calculate_offset(moved_piece, to_square(move));

						if (const auto stats_value = static_cast<int>(pos.thread_info()->evasion_history.value_at_offset(offset))
							+ (pi->move_counter_values ? static_cast<int>(pi->move_counter_values->value_at_offset(offset)) : sort_zero)
							+ ((pi - 1)->move_counter_values ? static_cast<int>((pi - 1)->move_counter_values->value_at_offset(offset)) : sort_zero)
							+ ((pi - 3)->move_counter_values ? static_cast<int>((pi - 3)->move_counter_values->value_at_offset(offset)) : sort_zero); stats_value < static_cast<int>(qs_stats_value_sortvalue))
							continue;
					}

					if (piece_type(moved_piece) != pt_king && !pos.see_test(move, see_0))
						continue;
				}
			}
			else
			{
				if (move < static_cast<uint32_t>(promotion_p) && !pos.see_test(move, see_0))
					continue;
			}

		skip_see_test:

			if (!pos.legal_move(move))
				continue;

			pos.play_move(move, gives_check);
			const int value = gives_check ? -q_search<nt, true>(pos, -beta, -alpha, depth - plies)
				: -q_search<nt, false>(pos, -beta, -alpha, depth - plies);
			pos.take_move_back(move);

			if ((pi + 1)->captured_piece)
			{
				constexpr auto qs_skip_see_test_value_less_than_equal_to_alpha_sort_value = 2000;
				constexpr auto qs_skip_see_test_value_greater_than_alpha_sort_value = 1000;
				if (const auto offset = move_value_stats::calculate_offset((pi + 1)->captured_piece, to_square(move)); value > alpha)
					pos.thread_info()->capture_history.update_plus(offset, qs_skip_see_test_value_greater_than_alpha_sort_value);
				else
					pos.thread_info()->capture_history.update_minus(offset, qs_skip_see_test_value_less_than_equal_to_alpha_sort_value);
			}

			if (value > best_value)
			{
				best_value = value;

				if (value > alpha)
				{
					if (pv_node)
						copy_pv(pi->pv, move, (pi + 1)->pv);

					if (pv_node && value < beta)
					{
						alpha = value;
						best_move = move;
					}
					else
					{
						hash_entry = main_hash.replace(key64);
						hash_entry->save(key64, value_to_hash(value, pi->ply), south_border + pi->strong_threat,
							hash_depth, move, pi->position_value, main_hash.age());

						return value;
					}
				}
			}
		}

		if (state_check && best_value == -max_score)
			return gets_mated(pi->ply);

		hash_entry = main_hash.replace(key64);
		hash_entry->save(key64, value_to_hash(best_value, pi->ply),
			(pv_node && best_value > orig_alpha ? exact_value : north_border) + pi->strong_threat,
			hash_depth, best_move, pi->position_value, main_hash.age());

		assert(best_value > -max_score && best_value < max_score);

		return best_value;
	}

	// reset history, evasion history, max gain, counter moves, followup moves, and capture history
	void reset()
	{
		main_hash.clear();
		threadpool::delete_counter_move_history();

		for (auto i = 0; i < thread_pool.thread_count; ++i)
		{
			const auto* th = thread_pool.threads[i];
			th->ti->history.clear();
			th->ti->evasion_history.clear();
			th->ti->max_gain_table.clear();
			th->ti->counter_moves.clear();
			th->ti->counter_followup_moves.clear();
			th->ti->capture_history.clear();
		}

		// set score and depth back to 0
		thread_pool.main()->previous_root_score = max_score;
		thread_pool.main()->previous_root_depth = 999 * plies;
		thread_pool.main()->quick_move_allow = false;
	}

	void send_time_info()
	{
		const auto elapsed = time_control.elapsed();

		// don't send info if running bench or more frequently than once per second
		if (!bench_active && elapsed - previous_info_time >= 1000)
		{
			previous_info_time = (elapsed + 100) / 1000 * 1000;
			const auto nodes = thread_pool.visited_nodes();
			const auto nps = elapsed ? nodes / elapsed * 1000 : 0;
			const auto tb_hits = thread_pool.tb_hits();
			acout() << "info time " << elapsed << " nodes " << nodes << " nps " << nps
				<< " tbhits " << tb_hits << " hashfull " << main_hash.hash_full() << std::endl;
		}

		if (param.ponder)
			return;

		if (param.use_time_calculating() && elapsed > time_control.maximum() - 10
			|| param.move_time && elapsed >= param.move_time
			|| param.nodes && thread_pool.visited_nodes() >= param.nodes)
			signals.stop_analyzing = true;
	}

	// update history, killers, and countermoves
	void update_stats(const position& pos, const bool state_check, const uint32_t move,
		const int depth, const uint32_t* quiet_moves, const int quiet_number)
	{
		auto* pi = pos.info();

		auto* cmh = pi->move_counter_values;
		auto* fmh = (pi - 1)->move_counter_values;
		auto* fmh2 = (pi - 3)->move_counter_values;
		auto* ti = pos.thread_info();
		auto& history = state_check ? ti->evasion_history : ti->history;

		if (!pos.capture_or_promotion(move))
		{
			if (pi->killers[0] != move)
			{
				pi->killers[1] = pi->killers[0];
				pi->killers[0] = move;
			}

			if (cmh)
				ti->counter_moves.update(pi->moved_piece, to_square(pi->previous_move), static_cast<unsigned short>(move));

			if (cmh && fmh)
				ti->counter_followup_moves.update((pi - 1)->moved_piece, to_square((pi - 1)->previous_move),
					pi->moved_piece, to_square(pi->previous_move), move);

			if (constexpr auto update_stats_max_depth = 18; depth < update_stats_max_depth * plies)
			{
				const auto bonus = counter_move_value(depth);
				const auto hist_bonus = history_bonus(depth);
				auto offset = move_value_stats::calculate_offset(pos.moved_piece(move), to_square(move));
				history.update_plus(offset, hist_bonus);
				if (cmh)
					cmh->update_plus(offset, bonus);

				if (fmh)
					fmh->update_plus(offset, bonus);

				if (fmh2)
					fmh2->update_plus(offset, bonus);

				for (auto i = 0; i < quiet_number; ++i)
				{
					offset = move_value_stats::calculate_offset(pos.moved_piece(quiet_moves[i]), to_square(quiet_moves[i]));
					history.update_minus(offset, hist_bonus);
					if (cmh)
						cmh->update_minus(offset, bonus);

					if (fmh)
						fmh->update_minus(offset, bonus);

					if (fmh2)
						fmh2->update_minus(offset, bonus);
				}
			}
		}

		if ((pi - 1)->move_number == 1 && !pi->captured_piece)
		{
			if (constexpr auto update_stats_move_1_max_depth = 18; depth < update_stats_move_1_max_depth * plies)
			{
				const auto bonus = counter_move_value(depth + plies);
				const auto offset = move_value_stats::calculate_offset(pi->moved_piece, to_square(pi->previous_move));

				if ((pi - 1)->move_counter_values)
					(pi - 1)->move_counter_values->update_minus(offset, bonus);

				if ((pi - 2)->move_counter_values)
					(pi - 2)->move_counter_values->update_minus(offset, bonus);

				if ((pi - 4)->move_counter_values)
					(pi - 4)->move_counter_values->update_minus(offset, bonus);
			}
		}
	}

	void update_stats_minus(const position& pos, const bool state_check, const uint32_t move, const int depth)
	{
		const auto* pi = pos.info();
		auto* cmh = pi->move_counter_values;
		auto* fmh = (pi - 1)->move_counter_values;
		auto* fmh2 = (pi - 3)->move_counter_values;
		auto* ti = pos.thread_info();
		auto& history = state_check ? ti->evasion_history : ti->history;

		if (!pos.capture_or_promotion(move))
		{
			if (constexpr auto update_stats_minus_max_depth = 18; depth < update_stats_minus_max_depth * plies)
			{
				const auto bonus = counter_move_value(depth);
				const auto hist_bonus = history_bonus(depth);
				const auto offset = move_value_stats::calculate_offset(pos.moved_piece(move), to_square(move));
				history.update_minus(offset, hist_bonus);
				if (cmh)
					cmh->update_minus(offset, bonus);

				if (fmh)
					fmh->update_minus(offset, bonus);

				if (fmh2)
					fmh2->update_minus(offset, bonus);
			}
		}
	}

	void update_stats_quiet(const position& pos, const bool state_check, const int depth, const uint32_t* quiet_moves, const int quiet_number)
	{
		const auto* pi = pos.info();
		auto* cmh = pi->move_counter_values;
		auto* fmh = (pi - 1)->move_counter_values;
		auto* fmh2 = (pi - 3)->move_counter_values;
		auto* ti = pos.thread_info();
		auto& history = state_check ? ti->evasion_history : ti->history;

		if (constexpr auto update_stats_quiet_max_depth = 18; depth < update_stats_quiet_max_depth * plies)
		{
			const auto bonus = static_cast<int>(depth);
			const auto hist_bonus = static_cast<int>(depth);
			for (auto i = 0; i < quiet_number; ++i)
			{
				const auto offset = move_value_stats::calculate_offset(pos.moved_piece(quiet_moves[i]), to_square(quiet_moves[i]));
				history.update_minus(offset, hist_bonus);
				if (cmh)
					cmh->update_minus(offset, bonus);

				if (fmh)
					fmh->update_minus(offset, bonus);

				if (fmh2)
					fmh2->update_minus(offset, bonus);
			}
		}
	}

	int value_to_hash(const int val, const int ply)
	{
		assert(val != no_score);

		return val >= longest_mate_score ? val + ply
			: val <= longest_mated_score ? val - ply
			: val;
	}

	int value_from_hash(const int val, const int ply)
	{
		return val == no_score ? no_score
			: val >= longest_mate_score ? val - ply
			: val <= longest_mated_score ? val + ply
			: val;
	}
}

void mainthread::begin_search()
{
	search::running = true;
	root_position->copy_position(thread_pool.root_position, nullptr, nullptr);
	const auto me = root_position->on_move();
	time_control.init(search::param, me, root_position->game_ply());
	search::previous_info_time = 0;
	interrupt_counter = 0;

	thread_pool.contempt_color = me;
	thread_pool.analysis_mode = !search::param.use_time_calculating();

	thread_pool.fifty_move_distance = std::min(50, std::max(thread_pool.fifty_move_distance, root_position->fifty_move_counter() / 2 + 5));
	thread_pool.piece_contempt = uci_contempt;
	if (thread_pool.piece_contempt)
	{
		if (thread_pool.analysis_mode)
			thread_pool.contempt_color = white;

		const auto temp_contempt = thread_pool.piece_contempt;
		thread_pool.piece_contempt = 0;
		const auto v1 = evaluate::eval(*root_position);
		thread_pool.piece_contempt = temp_contempt;

		if (const auto v2 = evaluate::eval(*root_position); abs(v1) < win_score && abs(v2) < win_score)
			thread_pool.root_contempt_value = v2 - v1;
		else
			thread_pool.root_contempt_value = score_0;
	}
	thread_pool.multi_pv = thread_pool.multi_pv_max = uci_multipv;
	thread_pool.active_thread_count = thread_pool.thread_count;

	if (thread_pool.analysis_mode)
	{
		search::draw[me] = draw_score;
		search::draw[~me] = draw_score;
	}
	else
	{
		constexpr auto default_draw_value = 24;
		search::draw[me] = draw_score - default_draw_value * root_position->game_phase() / middlegame_phase;
		search::draw[~me] = draw_score + default_draw_value * root_position->game_phase() / middlegame_phase;
	}

	if (!search::param.ponder)
		main_hash.new_age();
	tb_root_in_tb = false;
	egtb::use_rule50 = uci_syzygy_50_move_rule;
	tb_probe_depth = uci_syzygy_probe_depth * plies;
	tb_number = std::max(egtb::max_pieces_wdl, std::max(egtb::max_pieces_dtm, egtb::max_pieces_dtz));

	root_moves.move_number = 0;
	for (const auto& move : legal_move_list(*root_position))
		if (search::param.search_moves.empty() || search::param.search_moves.find(move) >= 0)
			root_moves.add(rootmove(move));

	if (thread_pool.analysis_mode)
	{
		auto* pi = root_position->info();
		auto e = std::min(pi->draw50_moves, pi->distance_to_null_move);
		while (e > 0)
		{
			pi--;
			e--;
			auto* stst = pi;
			auto found = false;
			for (auto i = 2; i <= e; i += 2)
			{
				stst = stst - 2;
				if (stst->key == pi->key)
				{
					found = true;
					break;
				}
			}

			if (!found)
				pi->key = 0;
		}
	}

	if (root_moves.move_number == 0)
	{
		root_moves.add(rootmove(no_move));
		root_moves[0].score = root_position->is_in_check() ? -mate_score : draw_score;
		root_moves[0].depth = main_thread_inc;
		thread_pool.active_thread_count = 1;
	}
	else
	{
		if (root_position->total_num_pieces() <= tb_number
			&& !root_position->castling_possible(all))
		{
			filter_root_moves(*root_position);

			if (tb_root_in_tb && root_moves.move_number == 1)
			{
				root_moves[0].depth = main_thread_inc;
				root_moves[0].score = tb_score;

				if (thread_pool.analysis_mode && abs(tb_score) > longest_mate_score)
				{
					root_position->play_move(root_moves[0].pv[0]);
					search::calculate_egtb_mate_pv(*root_position, tb_score, root_moves[0].pv);
					root_position->take_move_back(root_moves[0].pv[0]);
					root_moves[0].depth = main_thread_inc * root_moves[0].pv.size();
				}
				thread_pool.active_thread_count = 1;
				goto NO_ANALYSIS;
			}
		}

		thread_pool.multi_pv_max = std::min(thread_pool.multi_pv_max, root_moves.move_number);
		thread_pool.multi_pv = std::min(thread_pool.multi_pv, root_moves.move_number);

		if (thread_pool.active_thread_count > 1)
		{
			thread_pool.root_moves = root_moves;
			thread_pool.root_position_info = root_position->info();
		}

		for (auto i = 1; i < thread_pool.active_thread_count; ++i)
			thread_pool.threads[i]->wake(true);

		if (uci_mcts == true)
		{
			const auto mc = new monte_carlo(*root_position);
			if (!mc)
				exit(EXIT_FAILURE);

			mc->search();
			delete mc;
			mcts.clear(); 
		}
		else
			thread::begin_search();
	}

NO_ANALYSIS:

	if (!search::signals.stop_analyzing && (search::param.ponder || search::param.infinite))
	{

		if (root_moves[0].depth == main_thread_inc)
			root_moves[0].depth = 99 * main_thread_inc;

		search::signals.stop_if_ponder_hit = true;
		wait(search::signals.stop_analyzing);
	}

	search::signals.stop_analyzing = true;

	for (auto i = 1; i < thread_pool.active_thread_count; ++i)
		thread_pool.threads[i]->wait_for_search_to_end();

	thread* best_thread = this;

	if (!this->quick_move_played
		&& thread_pool.multi_pv == 1
		&& !search::param.depth
		&& root_moves[0].pv[0] != no_move)
	{
		for (auto i = 1; i < thread_pool.active_thread_count; ++i)
		{
			if (auto* th = thread_pool.threads[i]; th->root_moves[0].score > best_thread->root_moves[0].score
				&& th->completed_depth > best_thread->completed_depth)
				best_thread = th;
		}
	}

	previous_root_score = best_thread->root_moves[0].score;
	previous_root_depth = best_thread->root_moves[0].depth;

	if (best_thread != this || search::signals.stop_if_ponder_hit)
		best_thread->root_moves[0].depth = root_moves[0].depth;

	if (!bench_active)
	{
		acout() << print_pv(*best_thread->root_position, -max_score, max_score, active_pv, 0) << std::endl;
		acout() << "bestmove " << util::move_to_string(best_thread->root_moves[0].pv[0], *root_position);
		if (best_thread->root_moves[0].pv.size() > 1 || best_thread->root_moves[0].ponder_move_from_hash(*root_position))
			acout() << " ponder " << util::move_to_string(best_thread->root_moves[0].pv[1], *root_position);
		acout() << std::endl;
	}

	thread_pool.total_analyze_time += static_cast<int>(time_control.elapsed());

	search::running = false;
}

void thread::begin_search()
{
	int alpha;
	int delta_alpha;
	int delta_beta;
	uint32_t fast_move = 0;

	auto* main_thread = this == thread_pool.main() ? thread_pool.main() : nullptr;
	if (!main_thread)
	{
		root_position->copy_position(thread_pool.root_position, this, thread_pool.root_position_info);
		root_moves = thread_pool.root_moves;
	}

	auto* pi = root_position->info();

	std::memset(pi + 1, 0, 2 * sizeof(position_info));
	pi->killers[0] = pi->killers[1] = no_move;
	pi->previous_move = no_move;
	(pi - 2)->position_value = score_0;
	(pi - 1)->position_value = score_0;
	(pi - 1)->eval_positional = no_eval;
	(pi - 1)->move_number = 0;
	(pi - 4)->move_counter_values = (pi - 3)->move_counter_values = (pi - 2)->move_counter_values = (pi - 1)->move_counter_values = pi->
		move_counter_values = nullptr;
	(pi - 1)->mp_end_list = root_position->thread_info()->move_list;
	for (auto n = 0; n <= max_ply; n++)
	{
		(pi + n)->no_early_pruning = false;
		(pi + n)->excluded_move = no_move;
		(pi + n)->lmr_reduction = 0;
		(pi + n)->ply = n + 1;
	}

	auto best_value = delta_alpha = delta_beta = alpha = -max_score;
	auto beta = max_score;
	completed_depth = 0 * plies;

	if (main_thread)
	{
		fast_move = search::easy_move.expected_move(root_position->key());
		search::easy_move.clear();
		main_thread->quick_move_played = main_thread->failed_low = false;
		main_thread->quick_move_evaluation_busy = main_thread->quick_move_evaluation_stopped = false;
		main_thread->best_move_changed = 0;
		for (auto i = 1; i <= max_ply; i++)
			(pi + i)->pawn_key = 0;
	}

	if (main_thread && !tb_root_in_tb && !search::param.ponder && !thread_pool.analysis_mode
		&& main_thread->quick_move_allow && main_thread->previous_root_depth >= 12 * plies && thread_pool.multi_pv == 1)
	{
		if (const auto* hash_entry = main_hash.probe(root_position->key()); hash_entry && hash_entry->bounds() == exact_value)
		{
			const auto hash_value = search::value_from_hash(hash_entry->value(), pi->ply);
			const auto hash_move = hash_entry->move();

			if (const auto hash_depth = hash_entry->depth(); hash_depth >= main_thread->previous_root_depth - 3 * plies
				&& hash_move
				&& root_position->legal_move(hash_move)
				&& abs(hash_value) < win_score)
			{
				constexpr auto v_singular_margin = 102;
				const auto depth_singular = std::max(main_thread->previous_root_depth / 2, main_thread->previous_root_depth - 8 * plies);
				const auto v_singular = hash_value - static_cast<int>(v_singular_margin);
				pi->excluded_move = hash_move;
				pi->position_value = evaluate::eval(*root_position);
				main_thread->quick_move_evaluation_busy = true;
				const auto val = search::alpha_beta<search::nonPV>(*root_position, v_singular - score_1, v_singular, depth_singular, false);
				main_thread->quick_move_evaluation_busy = false;
				pi->excluded_move = no_move;

				if (!main_thread->quick_move_evaluation_stopped && val < v_singular)
				{
					search::signals.stop_analyzing = true;
					root_moves[0].score = hash_value;
					root_moves[0].pv.resize(1);
					root_moves[0].pv[0] = hash_move;
					root_moves[0].pv_from_hash(*root_position);
					root_moves[0].depth = hash_depth;
					main_thread->quick_move_allow = false;
					main_thread->quick_move_played = true;
					search::easy_move.clear();
					completed_depth = main_thread->previous_root_depth - 2 * plies;
					return;
				}
				main_thread->quick_move_evaluation_stopped = false;
			}
		}
	}
	if (main_thread)
		main_thread->quick_move_allow = true;

	auto search_iteration = 0;
	auto root_depth = plies / 2;

	while (++search_iteration < 100)
	{
		constexpr auto root_depth_mate_value_bv_add = 10;
		constexpr auto improvement_factor_bv_mult = 12;
		constexpr auto improvement_factor_max_mult = 160;
		constexpr auto improvement_factor_max_base = 652;
		constexpr auto improvement_factor_min_base = 1304;
		root_depth += main_thread ? main_thread_inc : other_thread_inc;

		if (main_thread)
		{
			if (search::param.depth && search_iteration - 1 >= search::param.depth)
				search::signals.stop_analyzing = true;
		}

		if (search::signals.stop_analyzing)
			break;

		if (main_thread)
		{
			main_thread->best_move_changed /= 2;
			main_thread->failed_low = false;
		}

		if (constexpr auto info_depth_interval = 1000; !bench_active && main_thread && time_control.elapsed() > info_depth_interval)
			acout() << "info depth " << search_iteration << std::endl;

		for (auto i = 0; i < root_moves.move_number; i++)
			root_moves[i].previous_score = root_moves[i].score;

		for (active_pv = 0; active_pv < thread_pool.multi_pv && !search::signals.stop_analyzing; ++active_pv)
		{
			const auto prev_best_move = root_moves[active_pv].pv[0];
			auto fail_high_count = 0;

			if (root_depth >= 5 * plies)
			{
				constexpr auto delta_beta_margin = 14;
				constexpr auto delta_alpha_margin = 14;
				delta_alpha = delta_alpha_margin + (thread_index_ & 7);
				delta_beta = delta_beta_margin - (thread_index_ & 7);
				alpha = std::max(root_moves[active_pv].previous_score - delta_alpha, -max_score);
				beta = std::min(root_moves[active_pv].previous_score + delta_beta, max_score);
			}

			while (true)
			{
				constexpr auto delta_alphabeta_value_add = 4;
				constexpr auto value_pawn_mult = 20;
				if (alpha < -value_pawn_mult * value_pawn)
					alpha = -max_score;
				if (beta > value_pawn_mult * value_pawn)
					beta = max_score;

				best_value = search::alpha_beta<search::PV>(*root_position, alpha, beta, root_depth, false);

				std::stable_sort(root_moves.moves + active_pv, root_moves.moves + root_moves.move_number);

				if (search::signals.stop_analyzing)
					break;

				bool fail_high_resolve = main_thread;

				if (constexpr auto time_control_optimum_mult_1 = 124; main_thread && best_value >= beta
					&& root_moves[active_pv].pv[0] == prev_best_move
					&& !thread_pool.analysis_mode
					&& time_control.elapsed() > time_control.optimum() * time_control_optimum_mult_1 / 1024)
				{
					if (const auto play_easy_move = root_moves[0].pv[0] == fast_move && main_thread->best_move_changed < 31; play_easy_move)
						fail_high_resolve = false;

					else if (constexpr auto time_control_optimum_mult_2 = 420; time_control.elapsed() > time_control.optimum() * time_control_optimum_mult_2 / 1024)
					{
						const auto improvement_factor = std::max(420, std::min(improvement_factor_min_base,
							improvement_factor_max_base + improvement_factor_max_mult
							* main_thread->failed_low - improvement_factor_bv_mult
							* (best_value - main_thread->previous_root_score)));
						if (const auto unstable_factor = 1024 + main_thread->best_move_changed; time_control.elapsed() > time_control.optimum() * unstable_factor / 1024 * improvement_factor / 1024)
							fail_high_resolve = false;
					}
				}

				if (best_value <= alpha)
				{
					beta = (alpha + beta) / 2;
					alpha = std::max(best_value - delta_alpha, -max_score);
					fail_high_count = 0;

					if (main_thread)
					{
						main_thread->failed_low = true;
						search::signals.stop_if_ponder_hit = false;
					}
				}
				else if (constexpr auto best_value_vp_mult = 8; best_value >= beta && (fail_high_resolve || best_value >= value_pawn * best_value_vp_mult))
				{
					alpha = (alpha + beta) / 2;
					beta = std::min(best_value + delta_beta, max_score);
					++fail_high_count;
				}
				else
					break;

				delta_alpha += delta_alpha / 4 + static_cast<int>(delta_alphabeta_value_add);
				delta_beta += delta_beta / 4 + static_cast<int>(delta_alphabeta_value_add);

				assert(alpha >= -max_score && beta <= max_score);
			}

			std::stable_sort(root_moves.moves + 0, root_moves.moves + active_pv + 1);
		}

		if (!search::signals.stop_analyzing)
			completed_depth = root_depth;

		if (!main_thread)
			continue;

		if (search::param.mate
			&& best_value >= longest_mate_score
			&& mate_score - best_value <= 2 * search::param.mate)
			search::signals.stop_analyzing = true;

		if (!thread_pool.analysis_mode && !search::param.ponder && best_value > mate_score - 32
			&& root_depth >= (mate_score - best_value + root_depth_mate_value_bv_add) * plies)
			search::signals.stop_analyzing = true;

		if (!thread_pool.analysis_mode && !search::param.ponder && best_value < -mate_score + 32
			&& root_depth >= (mate_score + best_value + root_depth_mate_value_bv_add) * plies)
			search::signals.stop_analyzing = true;

		if (!thread_pool.analysis_mode)
		{
			if (!search::signals.stop_analyzing && !search::signals.stop_if_ponder_hit)
			{
				const auto improvement_factor = std::max(420, std::min(improvement_factor_min_base, improvement_factor_max_base + improvement_factor_max_mult
					* main_thread->failed_low - improvement_factor_bv_mult
					* (best_value - main_thread->previous_root_score)));
				const auto unstable_factor = 1024 + main_thread->best_move_changed;

				if (const auto play_easy_move = root_moves[0].pv[0] == fast_move
					&& main_thread->best_move_changed < 31
					&& time_control.elapsed() > time_control.optimum() * 124 / 1024; root_moves.move_number == 1 && search_iteration > 10
					|| time_control.elapsed() > time_control.optimum() * unstable_factor / 1024 * improvement_factor / 1024
					|| ((main_thread->quick_move_played = play_easy_move)))
				{
					if (search::param.ponder)
						search::signals.stop_if_ponder_hit = true;
					else
						search::signals.stop_analyzing = true;
				}
			}

			if (root_moves[0].pv.size() >= 3)
				search::easy_move.refresh_pv(*root_position, root_moves[0].pv);
			else
				search::easy_move.clear();
		}
	}

	if (!main_thread)
		return;

	if (search::easy_move.third_move_stable < 6 || main_thread->quick_move_played)
		search::easy_move.clear();
}

void filter_root_moves(position& pos)
{
	tb_root_in_tb = false;
	egtb::use_rule50 = uci_syzygy_50_move_rule;
	tb_probe_depth = uci_syzygy_probe_depth;
	tb_number = uci_syzygy_probe_limit;

	if (tb_number > 0)
	{
		tb_number = 0;
		tb_probe_depth = 0;
	}

	if (tb_number < pos.total_num_pieces() || pos.castling_possible(all))
		return;

	tb_root_in_tb = egtb::egtb_probe_dtz(pos);

	if (tb_root_in_tb)
		tb_number = 0;

	if (tb_root_in_tb && !egtb::use_rule50)
		tb_score = tb_score > draw_score ? mate_score - max_ply - 1
		: tb_score < draw_score ? -mate_score + max_ply + 1
		: draw_score;
}

bool rootmove::ponder_move_from_hash(position& pos)
{
	assert(pv.size() == 1);
	if (!pv[0])
		return false;

	pos.play_move(pv[0]);

	if (const auto* hash_entry = main_hash.probe(pos.key() ^ pos.draw50_key()); hash_entry)
	{
		if (const auto move = hash_entry->move(); legal_moves_list_contains_move(pos, move))
			pv.add(move);
	}
	pos.take_move_back(pv[0]);

	return pv.size() > 1;
}

std::string print_pv(const position& pos, const int alpha, const int beta, const int active_pv, const int active_move)
{
	std::stringstream ss;
	const auto elapsed = static_cast<int>(time_control.elapsed()) + 1;
	const auto& root_moves = pos.my_thread()->root_moves;
	const auto multi_pv = std::min(thread_pool.multi_pv, root_moves.move_number);
	const auto visited_nodes = thread_pool.visited_nodes();
	const auto tb_hits = thread_pool.tb_hits();
	const auto hash_full = elapsed > 1000 ? main_hash.hash_full() : 0;

	for (auto i = 0; i < multi_pv; ++i)
	{
		const auto& root_move = root_moves[i == active_pv ? active_move : i];

		auto iterate = root_move.depth / main_thread_inc;
		if (iterate < 1)
			continue;

		auto score = i <= active_pv ? root_move.score : root_move.previous_score;

		const auto tb = tb_root_in_tb && abs(score) < egtb_win_score;
		score = tb ? tb_score : score;

		if (ss.rdbuf()->in_avail())
			ss << "\n";

		int sel_depth;
		const auto* pi = thread_pool.main()->root_position->info();
		for (sel_depth = 0; sel_depth < max_ply; sel_depth++)
			if ((pi + sel_depth)->pawn_key == 0)
				break;

		ss << "info time " << elapsed
			<< " nodes " << visited_nodes
			<< " nps " << visited_nodes / elapsed * 1000
			<< " tbhits " << tb_hits;

		if (hash_full)
			ss << " hashfull " << hash_full;

		ss << " depth " << iterate
			<< " seldepth " << sel_depth
			<< " multipv " << i + 1
			<< " score " << score_cp(score);

		if (!tb && i == active_pv)
			ss << (score >= beta ? " lowerbound" : score <= alpha ? " upperbound" : "");

		ss << " pv";

		auto pv_length = root_move.pv.size();
		if (pv_length > iterate)
			pv_length = std::max(iterate, pv_length - 4);
		for (auto n = 0; n < pv_length; n++)
			ss << " " << util::move_to_string(root_move.pv[n], pos);
	}

	return ss.str();
}

void rootmove::pv_from_hash(position& pos)
{
	uint64_t keys[max_ply];
	auto number = 0;

	assert(pv.size() == 1);
	auto move = pv[0];
	keys[number++] = pos.key();

	while (true)
	{
		pos.play_move(move);
		const auto key = pos.key();

		auto repeated = false;
		for (auto i = number - 2; i >= 0; i -= 2)
			if (keys[i] == key)
			{
				repeated = true;
				break;
			}
		if (repeated)
			break;

		keys[number++] = key;

		const auto* const hash_entry = main_hash.probe(pos.key() ^ pos.draw50_key());
		if (!hash_entry)
			break;
		move = hash_entry->move();
		if (!move || !legal_moves_list_contains_move(pos, move))
			break;
		pv.add(move);
	}
	for (auto i = pv.size(); i > 0;)
		pos.take_move_back(pv[--i]);
}

std::string score_cp(const int score)
{
	std::stringstream ss;

	if (abs(score) < longest_mate_score)
	{
		if (uci_mcts == true)
			ss << "cp " << score;
		else
			ss << "cp " << score / 3;
	}
	else
		ss << "mate " << (score > 0 ? mate_score - score + 1 : -mate_score - score) / 2;

	return ss.str();
}

