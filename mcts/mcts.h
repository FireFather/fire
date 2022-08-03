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

#include <climits>
#include <unordered_map>

#include "../position.h"
#include "../fire.h"
#include "../thread.h"
#include "../search.h"

struct node_info;
struct edge;

typedef double reward;
typedef node_info* mc_node;

inline bool print_children = false;
constexpr int max_children = 128;

enum edge_statistic
{
	stat_ucb, stat_visits, stat_mean
};

class monte_carlo
{
public:
	explicit monte_carlo(position& p);

	uint32_t search();

	void create_root();
	[[nodiscard]] bool computational_budget() const;
	mc_node tree_policy();
	reward playout_policy(mc_node node);
	void backup(reward r);
	edge* best_child(mc_node node, edge_statistic statistic) const;

	double ucb(mc_node node, const edge& edge) const;

	[[nodiscard]] mc_node current_node() const;
	bool is_root(mc_node node) const;
	bool is_terminal(mc_node node) const;
	void do_move(uint32_t move);
	void undo_move();
	void generate_moves();

	[[nodiscard]] reward value_to_reward(int v) const;
	[[nodiscard]] int reward_to_value(reward r) const;
	[[nodiscard]] int evaluate() const;
	[[nodiscard]] reward evaluate_terminal() const;

	reward calculate_prior(uint32_t move);
	static void add_prior_to_node(mc_node node, uint32_t m, reward prior);

	void default_parameters();
	void set_exploration_constant(double c);
	[[nodiscard]] double exploration_constant() const;
	[[nodiscard]] bool should_output_result() const;
	void print_pv(int depth);

private:

	position& pos_;
	mc_node root_{};

	int ply_{};
	int maximum_ply_{};
	long descent_cnt_{};
	long playout_cnt_{};
	long do_move_cnt_{};
	long prior_cnt_{};
	time_point start_time_{};
	time_point last_output_time_{};

	long max_descents_{};
	double  backup_minimax_{};
	double ucb_unexpanded_node_{};
	double ucb_exploration_constant_{};
	double ucb_losses_avoidance_{};
	double ucb_log_term_factor_{};
	bool ucb_use_father_visits_{};
	int prior_fast_eval_depth_{};
	int prior_slow_eval_depth_{};

	mc_node nodes_buffer_[max_ply + 7]{};
	mc_node* nodes_ = nodes_buffer_ + 4;
	edge* edges_buffer_[max_ply + 7]{};
	edge** edges_ = edges_buffer_ + 4;
	search::stack_mc stack_buffer_[max_ply + 7]{};
	search::stack_mc* stack_ = stack_buffer_ + 4;
};

inline void monte_carlo::default_parameters()
{
	max_descents_ = search::param.depth ? search::param.depth : INT_MAX;
	backup_minimax_ = 1.0;
	prior_fast_eval_depth_ = 1;
	prior_slow_eval_depth_ = 1;
	ucb_unexpanded_node_ = 0.5;
	ucb_exploration_constant_ = 0.7;
	ucb_losses_avoidance_ = 1.0;
	ucb_log_term_factor_ = 0.0;
	ucb_use_father_visits_ = true;
}

inline monte_carlo::monte_carlo(position& p) : pos_(p)
{
	default_parameters();
	create_root();
}

struct edge
{
	uint32_t move;
	double visits;
	reward prior;
	reward action_value;
	reward mean_action_value;
};

inline bool comp_float(const double a, const double b, const double epsilon = 0.005)
{
	return fabs(a - b) < epsilon;
}

static struct
{
	bool operator()(const edge a, const edge b) const
	{
		return a.prior > b.prior;
	}
} compare_prior;

static struct
{
	bool operator()(const edge a, const edge b) const
	{
		return a.visits > b.visits || (comp_float(a.visits, b.visits, 0.005) && a.prior > b.prior);
	}
} compare_visits;

static struct
{
	bool operator()(const edge a, const edge b) const
	{
		return a.mean_action_value > b.mean_action_value;
	}
} compare_mean_action;

static struct
{
	bool operator()(const edge a, const edge b) const
	{
		return 10 * a.visits + a.prior > 10 * b.visits + b.prior;
	}
} compare_robust_choice;

struct node_info
{
	uint32_t  last_move() const
	{
		return prev_move;
	}

	edge* children_list()
	{
		return &children[0];
	}

	spinlock lock;
	uint64_t key1 = 0;
	uint64_t key2 = 0;
	long node_visits = 0;
	int number_of_sons = 0;
	int expanded_sons = 0;
	uint32_t prev_move{};
	edge children[max_children]{};
};

typedef std::unordered_multimap<uint64_t, node_info> mcts_hash_table;
extern mcts_hash_table mcts;
