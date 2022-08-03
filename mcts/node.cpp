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

#include <cassert>
#include <cstring>    
#include <unordered_map>

#include "mcts.h"
#include "node.h"

#include "../fire.h"
#include "../movepick.h"
#include "../position.h"
#include "../pragma.h"
#include "../search.h"
#include "../thread.h"
#include "../uci.h"
#include "../util/util.h"

mc_node get_node(const position& pos)
{
	uint64_t key1 = pos.key();
	const uint64_t key2 = pos.pawn_key();

	const auto [fst, snd] = mcts.equal_range(key1);
	auto iterater1 = fst;
	const auto iterator2 = snd;
	while (iterater1 != iterator2)
	{
		if (mc_node node = &(iterater1->second); node->key1 == key1 && node->key2 == key2)
			return node;

		++iterater1;
	}

	node_info info;

	info.key1 = key1;
	info.key2 = key2;
	info.node_visits = 0;
	info.number_of_sons = 0;
	info.expanded_sons = 0;
	info.prev_move = 0;

	const auto it = mcts.insert(std::make_pair(key1, info));
	return &(it->second);
}

mc_node monte_carlo::current_node() const
{
	return nodes_[ply_];
}

void monte_carlo::create_root()
{
	ply_ = 1;
	maximum_ply_ = ply_;
	do_move_cnt_ = 0;
	descent_cnt_ = 0;
	playout_cnt_ = 0;
	prior_cnt_ = 0;
	start_time_ = now();
	last_output_time_ = start_time_;

	std::memset(stack_buffer_, 0, sizeof(stack_buffer_));
	for (int i = -4; i <= max_ply + 2; i++)
		stack_[i].continuation_history = &(pos_.my_thread()->cont_history[no_piece][0]);

	std::memset(nodes_buffer_, 0, sizeof(nodes_buffer_));
	root_ = nodes_[ply_] = get_node(pos_);

	if (current_node()->node_visits == 0)
		generate_moves();

	assert(ply_ == 1);
	assert(root_ == nodes_[ply_]);
	assert(root_ == current_node());
}

bool monte_carlo::is_root(mc_node node) const
{
	return (ply_ == 1
		&& node == current_node()
		&& node == root_);
}

bool monte_carlo::is_terminal(mc_node node) const
{
	assert(node == current_node());

	if (node->node_visits > 0 && number_of_sons(node) == 0)
		return true;

	if (ply_ >= max_ply - 2)
		return true;

	return false;
}

reward monte_carlo::evaluate_terminal() const
{
	mc_node node = current_node();

	assert(is_terminal(node));

	if (number_of_sons(node) == 0)
		return pos_.is_in_check() ? reward_mated : reward_draw;

	if (ply_ >= max_ply - 2)
		return reward_draw;

	return reward_draw;
}

edge* monte_carlo::best_child(mc_node node, const edge_statistic statistic) const
{
	if (number_of_sons(node) <= 0)
		return &edge_none;

	edge* children = get_list_of_children(node);

	int best = -1;
	double best_value = -1000000000000.0;
	for (int k = 0; k < number_of_sons(node); k++)
	{
		if (const double r = (statistic == stat_visits ? children[k].visits
			: statistic == stat_mean ? children[k].mean_action_value
			: statistic == stat_ucb ? ucb(node, children[k])
			: 0.0); r > best_value)
		{
			best_value = r;
			best = k;
		}
	}

	if (!bench_active && print_children == true)
	{
		//for (int k = 0; k < number_of_sons(node); k++)
		for (int k = number_of_sons(node) - 1; k >= 0; k--)
		{
			acout() << "info string move "
				<< k + 1
				<< " "
				<< util::move_to_string(children[k].move, pos_)

				<< std::setprecision(3)
				<< " win% "
				<< children[k].prior * 100

				<< std::fixed
				<< std::setprecision(0)
				<< " visits "
				<< children[k].visits
				<< std::endl;
		}
		print_children = false;
	}

	return &children[best];
}

void monte_carlo::add_prior_to_node(mc_node node, const uint32_t m, const reward prior)
{
	assert(node->number_of_sons < max_children);
	assert(prior >= 0 && prior <= 1.0);

	if (const int n = node->number_of_sons; n < max_children)
	{
		node->children[n].visits = 0;
		node->children[n].move = m;
		node->children[n].prior = prior;
		node->children[n].action_value = 0.0;
		node->children[n].mean_action_value = 0.0;
		node->number_of_sons++;
	}
}