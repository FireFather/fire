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

#include "bitboard.h"
#include "endgame.h"
#include "fire.h"
#include "movegen.h"
#include "pragma.h"
#include "thread.h"

namespace zobrist
{
	extern uint64_t psq[num_pieces][num_squares];
}

namespace endgame
{
	square normalize_pawn_side(const position& pos, const side strong_side, square sq)
	{
		assert(pos.number(strong_side, pt_pawn) == 1);

		if (file_of(pos.piece_square(strong_side, pt_pawn)) >= file_e)
			sq = static_cast<square>(sq ^ 7);

		if (strong_side == black)
			sq = ~sq;

		return sq;
	}
	// read char string position representation of endgame, then calculate and return 
	// a unique material key via binary XOR of position's piece square table values
	uint64_t material_key(const side color, const char* pieces)
	{
		uint64_t material_key = 0;
		for (auto piece = pt_king; piece <= pt_queen; ++piece)
		{
			auto number = pieces[piece] - '0';
			for (auto cnt = 0; cnt < number; ++cnt)
				material_key ^= zobrist::psq[make_piece(color, piece)][cnt];

			number = pieces[piece + 8] - '0';
			for (auto cnt = 0; cnt < number; ++cnt)
				material_key ^= zobrist::psq[make_piece(~color, piece)][cnt];
		}
		return material_key;
	}

	uint64_t attack_king_inc(const square s)
	{
		return empty_attack[pt_king][s] | s;
	}
}

endgames::endgames() = default;

void endgames::add_scale_factor(const char* pieces, const endgame_scale_factor f_w, const endgame_scale_factor f_b)
{
	map_scale_factor_[endgame::material_key(white, pieces)] = factor_number_;
	factor_functions[factor_number_++] = f_w;

	map_scale_factor_[endgame::material_key(black, pieces) ^ black_modifier] = factor_number_;
	factor_functions[factor_number_++] = f_b;
}

void endgames::add_value(const char* pieces, const endgame_value f_w, const endgame_value f_b)
{
	map_value_[endgame::material_key(white, pieces)] = value_number_;
	value_functions[value_number_++] = f_w;

	map_value_[endgame::material_key(black, pieces)] = value_number_;
	value_functions[value_number_++] = f_b;
}

// king and pawn vs lone king
// probes the king and pawn vs king table (see kpk.cpp)
template <side strong>
int endgame_kpk(const position& pos)
{
	const auto weak = ~strong;
	const auto strong_k = endgame::normalize_pawn_side(pos, strong, pos.king(strong));
	const auto weak_k = endgame::normalize_pawn_side(pos, strong, pos.king(weak));
	const auto pawn = endgame::normalize_pawn_side(pos, strong, pos.piece_square(strong, pt_pawn));

	if (const auto me = strong == pos.on_move() ? white : black; !kpk::probe(strong_k, pawn, weak_k, me))
	{
		pos.info()->eval_is_exact = true;
		return draw_score;
	}

	const auto result = win_score + 40 * static_cast<int>(rank_of(pawn));

	return strong == pos.on_move() ? result : -result;
}

// king & queen vs king & rook
template <side strong>
int endgame_kqkr(const position& pos)
{
	const auto weak = ~strong;
	const auto strong_k = pos.king(strong);
	const auto weak_k = pos.king(weak);

	const auto result = 4 * value_pawn
		+ endgame::push_to_side[weak_k]
		+ endgame::draw_closer[distance(strong_k, weak_k)];

	return strong == pos.on_move() ? result : -result;
}

// king and mating material vs lone king
template <side strong>
int endgame_kxk(const position& pos)
{
	if (pos.is_in_check())
		return score_0;

	const auto weak = ~strong;

	if (pos.on_move() == weak && !at_least_one_legal_move(pos))
		return draw_score;

	const auto strong_k = pos.king(strong);
	const auto weak_k = pos.king(weak);

	auto result = value_of_material(pos.non_pawn_material(strong))
		+ pos.number(strong, pt_pawn) * value_pawn
		+ endgame::push_to_side[weak_k]
		+ endgame::draw_closer[distance(strong_k, weak_k)];

	if (result < win_score)
		if (pos.number(strong, pt_queen) + pos.number(strong, pt_rook)
			|| pos.number(strong, pt_bishop) && pos.number(strong, pt_knight)
			|| pos.pieces(strong, pt_bishop) & dark_squares && pos.pieces(strong, pt_bishop) & ~dark_squares)
			result += win_score;

	return strong == pos.on_move() ? result : -result;
}

template int endgame_kxk<white>(const position& pos);
template int endgame_kxk<black>(const position& pos);

// we use text strings that resemble binary numbers to represent the pieces for each side in a position
// the number order corresponds with non-enumerated piece type values listed in fire.h
// for ex: 0110000 0100000 = white king & pawn vs black king (0, king, pawn, knight, bishop, rook, queen) 
// process these strings with add_value() and material_key()
void endgames::init_endgames()
{
	if (!map_value_.empty())
		return;

	value_number_ = factor_number_ = 0;

	value_functions[value_number_++] = &endgame_kxk<white>;
	value_functions[value_number_++] = &endgame_kxk<black>;

	add_value("0110000 0100000", &endgame_kpk<white>, &endgame_kpk<black>);
	add_value("0100001 0100010", &endgame_kqkr<white>, &endgame_kqkr<black>);

	factor_functions[factor_number_++] = &endgame_kbpk<white>;
	factor_functions[factor_number_++] = &endgame_kbpk<black>;
	factor_functions[factor_number_++] = &endgame_kpk<white>;
	factor_functions[factor_number_++] = &endgame_kpk<black>;
	factor_functions[factor_number_++] = &endgame_kpkp<white>;
	factor_functions[factor_number_++] = &endgame_kpkp<black>;
	factor_functions[factor_number_++] = &endgame_kqkrp<white>;
	factor_functions[factor_number_++] = &endgame_kqkrp<black>;
}

int endgames::probe_scale_factor(const uint64_t key, side& strong_side)
{
	function_index_map::const_iterator iteration = map_scale_factor_.find(key);

	if (iteration != map_scale_factor_.end())
		return strong_side = white, iteration->second;

	iteration = map_scale_factor_.find(key ^ black_modifier);

	if (iteration != map_scale_factor_.end())
		return strong_side = black, iteration->second;

	return -1;
}

int endgames::probe_value(const uint64_t key)
{
	const function_index_map::const_iterator iteration = map_value_.find(key);
	return iteration == map_value_.end() ? -1 : iteration->second;
}

inline int value_of_material(const int val)
{
	return 16 * static_cast<int>(val);
}

