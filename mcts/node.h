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

#include "mcts.h"

#include "../position.h"
#include "../pragma.h"

constexpr reward reward_none = 0.0;
constexpr reward reward_mated = 0.0;
constexpr reward reward_draw = 0.5;
constexpr reward reward_mate = 1.0;

mc_node get_node(const position& pos);

inline edge edge_none =
{
	0, 0, reward_none, reward_none, reward_none
};

inline uint32_t move_of(mc_node node)
{
	return node->last_move();
}

inline edge* get_list_of_children(mc_node node)
{
	return node->children_list();
}

inline int number_of_sons(mc_node node)
{
	return node->number_of_sons;
}
