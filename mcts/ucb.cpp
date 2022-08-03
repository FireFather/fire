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
#include <unordered_map>

#include "mcts.h"

#include "../fire.h"
#include "../pragma.h"
#include "../thread.h"

void monte_carlo::set_exploration_constant(const double c)
{
	ucb_exploration_constant_ = c;
}

double monte_carlo::exploration_constant() const
{
	return ucb_exploration_constant_;
}

double monte_carlo::ucb(mc_node node, const edge& edge) const
{
	const long father_visits = node->node_visits;
	assert(father_visits > 0);

	double result = 0.0;

	if (edge.visits < 1)
		result += edge.mean_action_value;
	else
		result += ucb_unexpanded_node_;

	const double c = ucb_use_father_visits_ ? exploration_constant() * sqrt(father_visits)
		: exploration_constant();

	const double losses = edge.visits - edge.action_value;
	const double visits = edge.visits;

	const double divisor = losses * ucb_losses_avoidance_ + visits * (1.0 - ucb_losses_avoidance_);
	result += c * edge.prior / (1 + divisor);

	result += ucb_log_term_factor_ * sqrt(log(father_visits) / (1 + visits));

	return result;
}