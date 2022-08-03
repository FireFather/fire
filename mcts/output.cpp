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

#include <algorithm>
#include <cassert>
#include <cmath>
#include <sstream>
#include <unordered_map>

#include "mcts.h"
#include "node.h"

#include "../fire.h"
#include "../hash.h"
#include "../position.h"
#include "../pragma.h"
#include "../search.h"
#include "../thread.h"
#include "../uci.h"
#include "../util/util.h"

bool monte_carlo::should_output_result() const
{
	const time_point elapsed = now() - start_time_ + 1;
	const time_point output_delay = now() - last_output_time_;

	if (elapsed < 1100)            return output_delay >= 100;
	if (elapsed < 11000)       return output_delay >= 1000;
	if (elapsed < 61000)       return output_delay >= 10000;
	if (elapsed < 360000)   return output_delay >= 30000;
	if (elapsed < 3660000)  return output_delay >= 60000;

	return output_delay >= 60000;
}

std::string create_pv(const position& pos, const int depth, const int alpha, const int beta)
{
	std::stringstream ss;
	const time_point elapsed = time_control.elapsed() + 1;
	const search::rootmoves_mc& mc_root_moves = pos.my_thread()->mc_rootmoves;
	const size_t pv_id = pos.my_thread()->active_pv;
	const auto multi_pv = std::min(static_cast<size_t>(uci_multipv), mc_root_moves.size());
	const uint64_t nodes_searched = thread_pool.visited_nodes();
	const uint64_t tb_hits = thread_pool.tb_hits() + (tb_root_in_tb ? mc_root_moves.size() : 0);
	const auto hash_full = elapsed > 1000 ? main_hash.hash_full() : 0;

	for (size_t i = 0; i < multi_pv; ++i)
	{
		const bool updated = mc_root_moves[i].score != -max_score;

		if (depth == 1 && !updated)
			continue;

		const int d = updated ? depth : depth - 1;
		int score = updated ? mc_root_moves[i].score : mc_root_moves[i].previous_score;

		const auto tb = tb_root_in_tb && abs(score) < egtb_win_score;
		score = tb ? tb_score : score;

		if (ss.rdbuf()->in_avail())
			ss << "\n";

		ss << "info"
			<< " depth " << d
			<< " seldepth " << mc_root_moves[i].sel_depth
			<< " multipv " << i + 1
			<< " score " << score_cp(score);

		if (!tb && i == pv_id)
			ss << (score >= beta ? " lowerbound" : score <= alpha ? " upperbound" : "");

		ss << " nodes " << nodes_searched
			<< " nps " << nodes_searched * 1000 / elapsed;

		if (elapsed > 1000)
			ss << " hashfull " << hash_full;

		ss << " tbhits " << tb_hits
			<< " time " << elapsed
			<< " pv";

		for (const uint32_t m : mc_root_moves[i].pv)
			ss << " " << util::move_to_string(m, pos);
	}
	return ss.str();
}

void monte_carlo::print_pv(const int depth)
{
	assert(is_root(current_node()));

	std::string pv;
	const edge* children = get_list_of_children(root_);
	const int n = number_of_sons(root_);

	edge list[max_children]{};
	for (int k = 0; k < n; k++)
		list[k] = children[k];

	std::sort(list, list + n, compare_robust_choice);

	search::rootmoves_mc& mc_root_moves = pos_.my_thread()->mc_rootmoves;
	mc_root_moves.clear();

	if (n > 0)
	{
		for (int k = 0; k < n; k++)
		{
			search::rootmove_mc rm(list[k].move);

			rm.previous_score = reward_to_value(list[k].mean_action_value);
			rm.score = rm.previous_score;
			rm.sel_depth = maximum_ply_;

			mc_root_moves.push_back(rm);
		}

		uint32_t move = mc_root_moves[0].pv[0];
		int cnt = 0;
		while (pos_.legal_move(move))
		{
			cnt++;
			do_move(move);

			nodes_[ply_] = get_node(pos_);

			if (is_terminal(current_node())
				|| number_of_sons(current_node()) <= 0
				|| current_node()->node_visits <= 0)
				break;

			move = best_child(current_node(), stat_visits)->move;

			if (pos_.legal_move(move))
				mc_root_moves[0].pv.push_back(move);
		}
		for (int k = 0; k < cnt; k++)
			undo_move();

		assert(static_cast<int>(mc_root_moves.size()) == number_of_sons(root_));
		assert(is_root(current_node()));

		if (!bench_active)
			pv = create_pv(pos_, depth, -max_score, max_score);
	}
	else
	{
		mc_root_moves.emplace_back(no_move);

		if (!bench_active)
			pv = "info depth 0 score " + score_cp(pos_.is_in_check() ? -mate_score : draw_score);
	}

	if (!bench_active)
		acout() << pv << std::endl;

	last_output_time_ = now();
}
