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
#include "fire.h"

typedef std::chrono::milliseconds::rep time_point;
typedef movelist<max_moves> max_moves_list;

inline time_point now()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct search_param
{
	search_param() : moves_to_go(0), depth(0), move_time(0), mate(0), infinite(0), ponder(0), nodes(time[white]
	= time[black] = inc[white] = inc[black] = moves_to_go = depth = move_time = mate = infinite = ponder = 0)
	{
	}

	[[nodiscard]] bool use_time_calculating() const
	{
		return !(mate | move_time | depth | nodes | infinite);
	}
	
	int time[num_sides]
	{
	},
	inc[num_sides]
	{
	},
	moves_to_go,
	depth,
	move_time,
	mate,
	infinite,
	ponder;
	
	uint64_t nodes;
	max_moves_list search_moves;
	time_point start_time = 0;	
};

class timecontrol
{
public:
	void init(const search_param& limit, side me, int ply);
	[[nodiscard]] int64_t optimum() const { return optimal_time_; }
	[[nodiscard]] int64_t maximum() const { return maximum_time_; }
	[[nodiscard]] int64_t elapsed() const;
	[[nodiscard]] double calc_move_importance(int ply) const;
	void adjustment_after_ponder_hit();
	int64_t uci_minimum_time = 1;
	int uci_move_overhead = 50;

private:
	time_point start_time_ = 0;
	int64_t optimal_time_ = 0;
	int64_t maximum_time_ = 0;

	// calc_move_importance parameters
	double x_scale_ = 7.64;
	double x_shift_ = 58.4;
	double skew_ = 0.183;
	double factor_base_ = 1.225;
	double ply_factor_ = 0.00025;
	int ply_min_ = 10;
	int ply_max_ = 70;
	int base_moves_ = 50;

	double move_importance_factor_ = 0.89;
	int moves_horizon_ = 50;
	double max_ratio_ = 7.09;
	double steal_ratio_ = 0.35;
};

extern timecontrol time_control;
