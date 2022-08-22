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

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <string>
#include <chrono>

// board square representation
// perspective from black side
enum square : int8_t
{
	a1, b1, c1, d1, e1, f1, g1, h1,
	a2, b2, c2, d2, e2, f2, g2, h2,
	a3, b3, c3, d3, e3, f3, g3, h3,
	a4, b4, c4, d4, e4, f4, g4, h4,
	a5, b5, c5, d5, e5, f5, g5, h5,
	a6, b6, c6, d6, e6, f6, g6, h6,
	a7, b7, c7, d7, e7, f7, g7, h7,
	a8, b8, c8, d8, e8, f8, g8, h8
};

enum file
{
	file_a, file_b, file_c, file_d, file_e, file_f, file_g, file_h
};

enum rank
{
	rank_1,
	rank_2,
	rank_3,
	rank_4,
	rank_5,
	rank_6,
	rank_7,
	rank_8
};

// engine constants
constexpr int default_hash = 64;
constexpr int max_hash = 1048576;
constexpr int max_threads = 256;
constexpr int max_moves = 256;
constexpr int max_ply = 128;
constexpr int max_pv = 63;

constexpr int value_pawn = 200;
constexpr int value_knight = 800;
constexpr int value_bishop = 850;
constexpr int value_rook = 1250;
constexpr int value_queen = 2500;

constexpr double score_factor = 1.6;

constexpr int sort_zero = 0;
constexpr int sort_max = 999999;

constexpr int eval_0 = 0;
constexpr int draw_eval = 0;
constexpr int no_eval = 199999;
constexpr int pawn_eval = 100 * 16;
constexpr int bishop_eval = 360 * 16;

constexpr int mat_0 = 0;
constexpr int mat_knight = 41;
constexpr int mat_bishop = 42;
constexpr int mat_rook = 64;
constexpr int mat_queen = 127;

constexpr int see_0 = 0;
constexpr int see_pawn = 100;
constexpr int see_knight = 328;
constexpr int see_bishop = 336;
constexpr int see_rook = 512;
constexpr int see_queen = 1016;

constexpr int score_0 = 0;
constexpr int score_1 = 1;
constexpr int draw_score = 0;
constexpr int win_score = 10000;

constexpr int mate_score = 30256;
constexpr int longest_mate_score = mate_score - 2 * max_ply; // 30000;
constexpr int longest_mated_score = -mate_score + 2 * max_ply; // -30000;
constexpr int egtb_win_score = mate_score - max_ply; // 30128

constexpr int max_score = 30257;
constexpr int no_score = 30258;

constexpr uint8_t no_castle = 0;
constexpr uint8_t white_short = 1;
constexpr uint8_t white_long = 2;
constexpr uint8_t black_short = 4;
constexpr uint8_t black_long = 8;
constexpr uint8_t all = white_short | white_long | black_short | black_long;
constexpr uint8_t castle_possible_n = 16;

constexpr uint8_t no_piecetype = 0;
constexpr uint8_t pt_king = 1;
constexpr uint8_t pt_pawn = 2;
constexpr uint8_t pt_knight = 3;
constexpr uint8_t pt_bishop = 4;
constexpr uint8_t pt_rook = 5;
constexpr uint8_t pt_queen = 6;
constexpr uint8_t pieces_without_king = 7;
constexpr uint8_t num_piecetypes = 8;
constexpr uint8_t all_pieces = 0;

constexpr int middlegame_phase = 26;

constexpr uint32_t no_move = 0;
constexpr uint32_t null_move = 65;

constexpr int castle_move = 9 << 12;
constexpr int enpassant = 10 << 12;
constexpr int promotion_p = 11 << 12;
constexpr int promotion_b = 12 << 12;
constexpr int promotion_r = 13 << 12;
constexpr int promotion_q = 14 << 12;

constexpr int plies = 8;
constexpr int main_thread_inc = 9;
constexpr int other_thread_inc = 9;

constexpr int depth_0 = 0;
constexpr int no_depth = -6 * plies;
constexpr int max_depth = max_ply * plies;

enum side : int;
constexpr side white = static_cast<side>(0);
constexpr side black = static_cast<side>(1);
constexpr side num_sides = static_cast<side>(2);

constexpr square north = static_cast<square>(8);
constexpr square south = static_cast<square>(-8);
constexpr square east = static_cast<square>(1);
constexpr square west = static_cast<square>(-1);

constexpr square north_east = static_cast<square>(9);
constexpr square south_east = static_cast<square>(-7);
constexpr square south_west = static_cast<square>(-9);
constexpr square north_west = static_cast<square>(7);
constexpr square no_square = static_cast<square>(127);
constexpr square num_squares = static_cast<square>(64);

constexpr square num_files = static_cast<square>(8);
constexpr square num_ranks = static_cast<square>(8);

enum score : int;

// movepick gen stages
enum stage
{
	normal_search,
	gen_good_captures,
	good_captures,
	killers_1,
	killers_2,
	gen_bxp_captures,
	bxp_captures,
	quietmoves,
	bad_captures,
	delayed_moves,
	check_evasions,
	gen_check_evasions,
	check_evasion_loop,
	q_search_with_checks,
	q_search_1,
	q_search_captures_1,
	q_search_check_moves,
	q_search_no_checks,
	q_search_2,
	q_search_captures_2,
	probcut,
	gen_probcut,
	probcut_captures,
	gen_recaptures,
	recapture_moves
};

// constant expressions
constexpr score make_score(const int mg, const int eg)
{
	return static_cast<score>((static_cast<int>(mg * score_factor) << 16) + static_cast<int>(eg * score_factor));
}

constexpr int remake_score(const int mg, const int eg)
{
	return (static_cast<int>(mg) << 16) + static_cast<int>(eg);
}

inline int mg_value(const int score)
{
	const union
	{
		uint16_t u;
		int16_t s;
	} mg =
	{
	static_cast<uint16_t>(static_cast<unsigned>(score + 0x8000) >> 16)
	};
	return mg.s;
}

inline int eg_value(const int score)
{
	const union
	{
		uint16_t u;
		int16_t s;
	} eg =
	{
		static_cast<uint16_t>(static_cast<unsigned>(score))
	};
	return eg.s;
}

inline int operator/(const score score, const int i)
{
	return remake_score(mg_value(score) / i, eg_value(score) / i);
}

constexpr side operator~(const side color)
{
	return static_cast<side>(color ^ 1);
}

constexpr square operator~(const square sq)
{
	return static_cast<square>(sq ^ 56);
}

inline int mul_div(const int score, const int mul, const int div)
{
	return remake_score(mg_value(score) * mul / div, eg_value(score) * mul / div);
}

constexpr int gives_mate(const int ply)
{
	return mate_score - ply;
}

constexpr int gets_mated(const int ply)
{
	return -mate_score + ply;
}

constexpr square make_square(const file f, const rank r)
{
	return static_cast<square>((r << 3) + f);
}

constexpr file file_of(const square sq)
{
	return static_cast<file>(sq & 7);
}

inline rank rank_of(const square sq)
{
	return static_cast<rank>(sq >> 3);
}

constexpr  square relative_square(const side color, const square sq)
{
	return static_cast<square>(sq ^ color * 56);
}

constexpr rank relative_rank(const side color, const rank r)
{
	return static_cast<rank>(r ^ color * 7);
}

inline rank relative_rank(const side color, const square sq)
{
	return relative_rank(color, rank_of(sq));
}

constexpr bool different_color(const square v1, const square v2)
{
	const auto val = static_cast<int>(v1) ^ static_cast<int>(v2);
	return (val >> 3 ^ val) & 1;
}

constexpr square pawn_ahead(const side color)
{
	return color == white ? north : south;
}

constexpr square from_square(const uint32_t move)
{
	return static_cast<square>(move >> 6 & 0x3F);
}

constexpr square to_square(const uint32_t move)
{
	return static_cast<square>(move & 0x3F);
}

constexpr int move_type(const uint32_t move)
{
	return static_cast<int>(move & 15 << 12);
}

constexpr uint8_t promotion_piece(const uint32_t move)
{
	return static_cast<uint8_t>(pt_knight + ((move >> 12 & 15) - (promotion_p >> 12)));
}

constexpr uint8_t piece_moved(const uint32_t move)
{
	assert(move < castle_move);
	return static_cast<uint8_t>(move >> 12 & 7);
}

constexpr uint32_t make_move(const square from, const square to)
{
	return static_cast<uint32_t>(to + (from << 6));
}

constexpr uint32_t make_move(const int type, const square from, const square to)
{
	return static_cast<uint32_t>(to + (from << 6) + type);
}

constexpr bool is_ok(const uint32_t move)
{
	return move != no_move && move != null_move;
}

template <int capacity>
struct movelist
{
	int move_number;
	uint32_t moves[capacity] = {};

	movelist() : move_number(0)
	{
	}

	void add(uint32_t move)
	{
		if (move_number < capacity) moves[move_number++] = move;
	}

	uint32_t& operator[](int index)
	{
		return moves[index];
	}
	const uint32_t& operator[](int index) const
	{
		return moves[index];
	}
	[[nodiscard]] int size() const
	{
		return move_number;
	}

	void resize(const int new_size)
	{
		move_number = new_size;
	}

	void clear()
	{
		move_number = 0;
	}

	[[nodiscard]] bool empty() const
	{
		return move_number == 0;
	}

	int find(uint32_t move)
	{
		for (auto i = 0; i < move_number; i++)
			if (moves[i] == move)
				return i;
		return -1;
	}
};
