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
#include "bitop.h"

inline constexpr uint64_t file_a_bb = 0x0101010101010101ULL;
inline constexpr uint64_t file_b_bb = file_a_bb << 1;
inline constexpr uint64_t file_c_bb = file_a_bb << 2;
inline constexpr uint64_t file_d_bb = file_a_bb << 3;
inline constexpr uint64_t file_e_bb = file_a_bb << 4;
inline constexpr uint64_t file_f_bb = file_a_bb << 5;
inline constexpr uint64_t file_g_bb = file_a_bb << 6;
inline constexpr uint64_t file_h_bb = file_a_bb << 7;

inline constexpr uint64_t rank_1_bb = 0xFF;
inline constexpr uint64_t rank_2_bb = rank_1_bb << 8;
inline constexpr uint64_t rank_3_bb = rank_1_bb << 16;
inline constexpr uint64_t rank_4_bb = rank_1_bb << 24;
inline constexpr uint64_t rank_5_bb = rank_1_bb << 32;
inline constexpr uint64_t rank_6_bb = rank_1_bb << 40;
inline constexpr uint64_t rank_7_bb = rank_1_bb << 48;
inline constexpr uint64_t rank_8_bb = rank_1_bb << 56;

inline constexpr uint64_t light_squares = 0x55AA55AA55AA55AAULL;
inline constexpr uint64_t dark_squares = 0xAA55AA55AA55AA55ULL;

namespace bitboard
{
	void init();

	inline uint64_t magic_attack_r[102400];
#ifdef USE_PEXT
	inline uint64_t magic_attack_b[5248];
#endif

#ifndef USE_PEXT
	const int bishop_magic_index[64] =
	{
		16530,  9162,  9674,
		18532, 19172, 17700,
		5730, 19661, 17065,
		12921, 15683, 17764,
		19684, 18724,  4108,
		12936, 15747,  4066,
		14359, 36039, 20457,
		43291,  5606,  9497,
		15715, 13388,  5986,
		11814, 92656,  9529,
		18118,  5826,  4620,
		12958, 55229,  9892,
		33767, 20023,  6515,
		6483, 19622,  6274,
		18404, 14226, 17990,
		18920, 13862, 19590,
		5884, 12946,  5570,
		18740,  6242, 12326,
		4156, 12876, 17047,
		17780,  2494, 17716,
		17067,  9465, 16196,
		6166
	};

	const int rook_magic_index[64] =
	{
		85487, 43101,     0,
		49085, 93168, 78956,
		60703, 64799, 30640,
		9256, 28647, 10404,
		63775, 14500, 52819,
		2048, 52037, 16435,
		29104, 83439, 86842,
		27623, 26599, 89583,
		7042, 84463, 82415,
		95216, 35015, 10790,
		53279, 70684, 38640,
		32743, 68894, 62751,
		41670, 25575,  3042,
		36591, 69918,  9092,
		17401, 40688, 96240,
		91632, 32495, 51133,
		78319, 12595,  5152,
		32110, 13894,  2546,
		41052, 77676, 73580,
		44947, 73565, 17682,
		56607, 56135, 44989,
		21479
	};

	inline const uint64_t bishop_magics[64] =
	{
		0x007bfeffbfeffbffull, 0x003effbfeffbfe08ull, 0x0000401020200000ull,
		0x0000200810000000ull, 0x0000110080000000ull, 0x0000080100800000ull,
		0x0007efe0bfff8000ull, 0x00000fb0203fff80ull, 0x00007dff7fdff7fdull,
		0x0000011fdff7efffull, 0x0000004010202000ull, 0x0000002008100000ull,
		0x0000001100800000ull, 0x0000000801008000ull, 0x000007efe0bfff80ull,
		0x000000080f9fffc0ull, 0x0000400080808080ull, 0x0000200040404040ull,
		0x0000400080808080ull, 0x0000200200801000ull, 0x0000240080840000ull,
		0x0000080080840080ull, 0x0000040010410040ull, 0x0000020008208020ull,
		0x0000804000810100ull, 0x0000402000408080ull, 0x0000804000810100ull,
		0x0000404004010200ull, 0x0000404004010040ull, 0x0000101000804400ull,
		0x0000080800104100ull, 0x0000040400082080ull, 0x0000410040008200ull,
		0x0000208020004100ull, 0x0000110080040008ull, 0x0000020080080080ull,
		0x0000404040040100ull, 0x0000202040008040ull, 0x0000101010002080ull,
		0x0000080808001040ull, 0x0000208200400080ull, 0x0000104100200040ull,
		0x0000208200400080ull, 0x0000008840200040ull, 0x0000020040100100ull,
		0x007fff80c0280050ull, 0x0000202020200040ull, 0x0000101010100020ull,
		0x0007ffdfc17f8000ull, 0x0003ffefe0bfc000ull, 0x0000000820806000ull,
		0x00000003ff004000ull, 0x0000000100202000ull, 0x0000004040802000ull,
		0x007ffeffbfeff820ull, 0x003fff7fdff7fc10ull, 0x0003ffdfdfc27f80ull,
		0x000003ffefe0bfc0ull, 0x0000000008208060ull, 0x0000000003ff0040ull,
		0x0000000001002020ull, 0x0000000040408020ull, 0x00007ffeffbfeff9ull,
		0x007ffdff7fdff7fdull
	};

	inline const uint64_t rook_magics[64] =
	{
		0x00a801f7fbfeffffull, 0x00180012000bffffull, 0x0040080010004004ull,
		0x0040040008004002ull, 0x0040020004004001ull, 0x0020008020010202ull,
		0x0040004000800100ull, 0x0810020990202010ull, 0x000028020a13fffeull,
		0x003fec008104ffffull, 0x00001800043fffe8ull, 0x00001800217fffe8ull,
		0x0000200100020020ull, 0x0000200080010020ull, 0x0000300043ffff40ull,
		0x000038010843fffdull, 0x00d00018010bfff8ull, 0x0009000c000efffcull,
		0x0004000801020008ull, 0x0002002004002002ull, 0x0001002002002001ull,
		0x0001001000801040ull, 0x0000004040008001ull, 0x0000802000200040ull,
		0x0040200010080010ull, 0x0000080010040010ull, 0x0004010008020008ull,
		0x0000020020040020ull, 0x0000010020020020ull, 0x0000008020010020ull,
		0x0000008020200040ull, 0x0000200020004081ull, 0x0040001000200020ull,
		0x0000080400100010ull, 0x0004010200080008ull, 0x0000200200200400ull,
		0x0000200100200200ull, 0x0000200080200100ull, 0x0000008000404001ull,
		0x0000802000200040ull, 0x00ffffb50c001800ull, 0x007fff98ff7fec00ull,
		0x003ffff919400800ull, 0x001ffff01fc03000ull, 0x0000010002002020ull,
		0x0000008001002020ull, 0x0003fff673ffa802ull, 0x0001fffe6fff9001ull,
		0x00ffffd800140028ull, 0x007fffe87ff7ffecull, 0x003fffd800408028ull,
		0x001ffff111018010ull, 0x000ffff810280028ull, 0x0007fffeb7ff7fd8ull,
		0x0003fffc0c480048ull, 0x0001ffffa2280028ull, 0x00ffffe4ffdfa3baull,
		0x007ffb7fbfdfeff6ull, 0x003fffbfdfeff7faull, 0x001fffeff7fbfc22ull,
		0x000ffffbf7fc2ffeull, 0x0007fffdfa03ffffull, 0x0003ffdeff7fbdecull,
		0x0001ffff99ffab2full
	};
#endif
}

inline int8_t square_distance[num_squares][num_squares];

inline uint64_t rook_mask[num_squares];
inline uint64_t bishop_mask[num_squares];
inline uint64_t* bishop_attack_table[64];
inline uint64_t* rook_attack_table[64];

inline uint64_t square_bb[num_squares];
inline uint64_t adjacent_files_bb[num_files];
inline uint64_t ranks_in_front_bb[num_sides][num_ranks];
inline uint64_t between_bb[num_squares][num_squares];
inline uint64_t connection_bb[num_squares][num_squares];
inline uint64_t in_front_bb[num_sides][num_squares];

inline uint64_t passed_pawn_mask[num_sides][num_squares];
inline uint64_t pawn_attack_span[num_sides][num_squares];
inline uint64_t pawnattack[num_sides][num_squares];

inline uint64_t empty_attack[num_piecetypes][num_squares];
inline uint64_t king_zone[num_squares];

namespace kpk
{
	bool probe(square wk_sq, square wp_sq, square bk_sq, side me);
}

// array of file bitboards
inline constexpr uint64_t file_bb[num_files] =
{
	file_a_bb, file_b_bb, file_c_bb, file_d_bb, file_e_bb, file_f_bb, file_g_bb, file_h_bb
};

// array of rank bitboards
inline constexpr uint64_t rank_bb[num_ranks] =
{
	rank_1_bb, rank_2_bb, rank_3_bb, rank_4_bb, rank_5_bb, rank_6_bb, rank_7_bb, rank_8_bb
};

template <square delta>
uint64_t shift_bb(const uint64_t b)
{
	return delta == north ? b << 8
		: delta == south ? b >> 8
		: delta == north_east ? (b & ~file_h_bb) << 9
		: delta == south_east ? (b & ~file_h_bb) >> 7
		: delta == north_west ? (b & ~file_a_bb) << 7
		: delta == south_west ? (b & ~file_a_bb) >> 9
		: 0;
}

inline constexpr int kp_delta[][8] =
{
{},
{
	9, 7, -7, -9, 8, 1, -1, -8
},
{},
{
	17, 15, 10, 6, -6, -10, -15, -17
}
};

inline constexpr int rook_deltas[4][2] =
{
	{0, 1},
	{0, -1},
	{1, 0},
	{-1, 0}
};

inline constexpr int bishop_deltas[4][2] =
{
	{1, 1},
	{-1, 1},
	{1, -1},
	{-1, -1}
};

// bitboard representations of multiple squares
constexpr uint64_t bb(const square s)
{
	return 1ULL << s;
}

inline uint64_t bb2(const square s1, const square s2)
{
	return 1ULL << s1 | 1ULL << s2;
}

inline uint64_t bb3(const square s1, const square s2, const square s3)
{
	return 1ULL << s1 | 1ULL << s2 | 1ULL << s3;
}

inline uint64_t bb4(const square s1, const square s2, const square s3, const square s4)
{
	return 1ULL << s1 | 1ULL << s2 | 1ULL << s3 | 1ULL << s4;
}

// bitwise operator AND for bitboards (64-bit unsigned integers)
inline uint64_t operator&(const uint64_t b, const square sq)
{
	return b & bb(sq);
}

// bitwise operator inclusive OR for bitboards 
inline uint64_t operator|(const uint64_t b, const square sq)
{
	return b | bb(sq);
}

// bitwise operator XOR (exclusive OR) for bitboards 
inline uint64_t operator^(const uint64_t b, const square sq)
{
	return b ^ bb(sq);
}

// compound bitwise operator OR for bitboards 
inline uint64_t& operator|=(uint64_t& b, const square sq)
{
	return b |= bb(sq);
}

// compound bitwise operator XOR for bitboards 
inline uint64_t& operator^=(uint64_t& b, const square sq)
{
	return b ^= bb(sq);
}

inline bool more_than_one(const uint64_t b)
{
	return b & b - 1;
}

// returns a bitboard for complete rank given a specific sq
inline uint64_t get_rank(const square sq)
{
	return rank_bb[rank_of(sq)];
}

// returns a bitboard for complete file given a specific sq
inline uint64_t get_file(const square sq)
{
	return file_bb[file_of(sq)];
}

// returns a bitboard for complete file given a specific file
inline uint64_t get_file(const file f)
{
	return file_bb[f];
}

// returns a bitboard for adjacent file given a specific file
inline uint64_t get_adjacent_files(const file f)
{
	return adjacent_files_bb[f];
}

// returns a bitboard for squares between 2 specific squares
inline uint64_t get_between(const square square1, const square square2)
{
	return between_bb[square1][square2];
}

// returns a bitboard for ranks in front
inline uint64_t ranks_forward_bb(const side color, const rank r)
{
	return ranks_in_front_bb[color][r];
}

inline uint64_t ranks_forward_bb(const side color, const square sq)
{
	return ranks_in_front_bb[color][rank_of(sq)];
}

inline uint64_t forward_bb(const side color, const square sq)
{
	return in_front_bb[color][sq];
}

inline uint64_t pawn_attack_range(const side color, const square sq)
{
	return pawn_attack_span[color][sq];
}

// does sq contain a passed pawn?
inline uint64_t passedpawn_mask(const side color, const square sq)
{
	return passed_pawn_mask[color][sq];
}

// returns a bitboard representing aligned squares (straight or diagonal)
inline bool aligned(const square square1, const square square2, const square square3)
{
	return connection_bb[square1][square2] & square3;
}

// returns a bitboard representing the distance between 2 specific squares
inline int distance(const square x, const square y)
{
	return square_distance[x][y];
}

// returns a bitboard representing the file distance between 2 specific squares
inline int file_distance(const square x, const square y)
{
	return abs(file_of(x) - file_of(y));
}

// returns a bitboard representing the rank distance between 2 specific squares
inline int rank_distance(const square x, const square y)
{
	return abs(rank_of(x) - rank_of(y));
}

// returns a bitboard representing the color of the sq in front
inline square front_square(const side color, const uint64_t b)
{
	return color == white ? msb(b) : lsb(b);
}

// returns a bitboard representing the color of the sq behind
inline square rear_square(const side color, const uint64_t b)
{
	return color == white ? lsb(b) : msb(b);
}

// bishop attack macro
inline uint64_t attack_bishop_bb(const square sq, const uint64_t occupied)
{
#ifdef USE_PEXT
	return bishop_attack_table[sq][pext(occupied, bishop_mask[sq])];
#else
	return bishop_attack_table[sq][((occupied & bishop_mask[sq]) * bitboard::bishop_magics[sq]) >> 55];
#endif
}

// rook attack macro
inline uint64_t attack_rook_bb(const square sq, const uint64_t occupied)
{
#ifdef USE_PEXT
	return rook_attack_table[sq][pext(occupied, rook_mask[sq])];
#else
	return rook_attack_table[sq][((occupied & rook_mask[sq]) * bitboard::rook_magics[sq]) >> 52];
#endif
}

inline uint64_t attack_bb(const uint8_t piece_t, const square sq, const uint64_t occupied)
{
	assert(piece_t != pt_pawn);

	switch (piece_t)
	{
	case pt_bishop: return attack_bishop_bb(sq, occupied);
	case pt_rook: return attack_rook_bb(sq, occupied);
	case pt_queen: return attack_bishop_bb(sq, occupied) | attack_rook_bb(sq, occupied);
	default: return empty_attack[piece_t][sq];
	}
}

template <side color>
inline uint64_t pawn_attack(const uint64_t bb)
{
	if constexpr (color == white)
		return shift_bb<north_west>(bb) | shift_bb<north_east>(bb);
	else
		return shift_bb<south_west>(bb) | shift_bb<south_east>(bb);
}

template <side color>
inline uint64_t shift_up(const uint64_t bb)
{
	if constexpr (color == white)
		return shift_bb<north>(bb);
	else
		return shift_bb<south>(bb);
}

template <side color>
inline uint64_t shift_down(const uint64_t bb)
{
	if constexpr (color == white)
		return shift_bb<south>(bb);
	else
		return shift_bb<north>(bb);
}

template <side color>
inline uint64_t shift_up_left(const uint64_t bb)
{
	if constexpr (color == white)
		return shift_bb<north_west>(bb);
	else
		return shift_bb<south_west>(bb);
}

template <side color>
inline uint64_t shift_up_right(const uint64_t bb)
{
	if constexpr (color == white)
		return shift_bb<north_east>(bb);
	else
		return shift_bb<south_east>(bb);
}
// clear least significant bit
inline square pop_lsb(uint64_t* b)
{
	const auto sq = lsb(*b);
	*b &= *b - 1;
	return sq;
}

uint64_t sliding_attacks(int sq, uint64_t block, const int deltas[4][2], int f_min, int f_max, int r_min, int r_max);
void init_magic_sliders();
