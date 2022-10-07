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

#include "fire.h"
#include "pragma.h"
#include "macro/file.h"
#include "macro/rank.h"
#include "macro/side.h"
#include "macro/square.h"

// init all bitboard arrays
void bitboard::init()
{
	for (auto sq = a1; sq <= h8; ++sq)
		square_bb[sq] = 1ULL << sq;

	for (auto f = file_a; f <= file_h; ++f)
		adjacent_files_bb[f] = (f > file_a ? file_bb[f - 1] : 0) | (f < file_h ? file_bb[f + 1] : 0);

	for (auto r = rank_1; r < rank_8; ++r)
		ranks_in_front_bb[white][r] = ~(ranks_in_front_bb[black][r + 1] = ranks_in_front_bb[black][r] | rank_bb[r]);

	for (auto color = white; color <= black; ++color)
		for (auto sq = a1; sq <= h8; ++sq)
		{
			in_front_bb[color][sq] = ranks_in_front_bb[color][rank_of(sq)] & file_bb[file_of(sq)];
			pawn_attack_span[color][sq] = ranks_in_front_bb[color][rank_of(sq)] & adjacent_files_bb[file_of(sq)];
			passed_pawn_mask[color][sq] = in_front_bb[color][sq] | pawn_attack_span[color][sq];
		}

	for (auto square1 = a1; square1 <= h8; ++square1)
		for (auto square2 = a1; square2 <= h8; ++square2)
			square_distance[square1][square2] = static_cast<int8_t>(std::max(file_distance(square1, square2), rank_distance(square1, square2)));

	for (auto sq = a1; sq <= h8; ++sq)
	{
		pawnattack[white][sq] = pawn_attack<white>(square_bb[sq]);
		pawnattack[black][sq] = pawn_attack<black>(square_bb[sq]);
	}

	for (auto piece = pt_king; piece <= pt_knight; ++++piece)
		for (auto sq = a1; sq <= h8; ++sq)
		{
			uint64_t b = 0;
			for (auto i = 0; i < 8; ++i)
			{
				if (const auto to = sq + static_cast<square>(kp_delta[piece][i]);
					to >= a1 && to <= h8 && distance(sq, to) < 3)
					b |= to;
			}
			empty_attack[piece][sq] = b;
		}

	for (auto sq = a1; sq <= h8; ++sq)
	{
		auto b = empty_attack[pt_king][sq];
		if (file_of(sq) == file_a)
			b |= b << 1;
		else if (file_of(sq) == file_h)
			b |= b >> 1;
		if (rank_of(sq) == rank_1)
			b |= b << 8;
		else if (rank_of(sq) == rank_8)
			b |= b >> 8;
		king_zone[sq] = b;
	}

	init_magic_sliders();

	for (auto square1 = a1; square1 <= h8; ++square1)
	{
		empty_attack[pt_bishop][square1] = attack_bishop_bb(square1, 0);
		empty_attack[pt_rook][square1] = attack_rook_bb(square1, 0);
		empty_attack[pt_queen][square1] = empty_attack[pt_bishop][square1] | empty_attack[pt_rook][square1];

		for (auto piece = pt_bishop; piece <= pt_rook; ++piece)
			for (auto square2 = a1; square2 <= h8; ++square2)
			{
				if (!(empty_attack[piece][square1] & square2))
					continue;

				connection_bb[square1][square2] = (attack_bb(piece, square1, 0) & attack_bb(piece, square2, 0))
					| square1 | square2;
				between_bb[square1][square2] = attack_bb(piece, square1, square_bb[square2])
					& attack_bb(piece, square2, square_bb[square1]);
			}
	}
}

// init_magic_bb() calculates all rook and bishop attacks. 
// the bitboards are used to look up sliding piece attacks
// If the system supports BMI2 (Bit Manipulation Instruction Set 2), specifically Parallel bit extract, then use:
#ifdef USE_PEXT
void init_magic_bb_pext(uint64_t* attack, uint64_t* square_index[], uint64_t* mask, const int deltas[4][2])
{
	for (auto sq = 0; sq < 64; sq++)
	{
		square_index[sq] = attack;
		mask[sq] = sliding_attacks(sq, 0, deltas, 1, 6, 1, 6);

		uint64_t b = 0;
		do
		{
			square_index[sq][pext(b, mask[sq])] = sliding_attacks(sq, b, deltas, 0, 7, 0, 7);
			b = b - mask[sq] & mask[sq];
			attack++;
		} while (b);
	}
}
#else
// otherwise use this function, which is only slightly slower (1-2%) on an Intel® Core™ i9-9900K @3.60ghz
void init_magic_bb(uint64_t* attack, const int attack_index[], uint64_t* square_index[], uint64_t* mask,
	const int shift, const uint64_t mult[], const int deltas[4][2])
{
	for (auto sq = 0; sq < 64; sq++)
	{
		const auto index = attack_index[sq];
		square_index[sq] = attack + index;
		mask[sq] = sliding_attacks(sq, 0, deltas, 1, 6, 1, 6);

		uint64_t b = 0;
		do
		{
			const int offset = static_cast<uint32_t>((b * mult[sq]) >> shift);
			square_index[sq][offset] = sliding_attacks(sq, b, deltas, 0, 7, 0, 7);
			b = (b - mask[sq]) & mask[sq];
		} while (b);
	}
}
#endif

void init_magic_sliders()
{
#ifdef USE_PEXT
	init_magic_bb_pext(bitboard::magic_attack_r, rook_attack_table, rook_mask, rook_deltas);
	init_magic_bb_pext(bitboard::magic_attack_b, bishop_attack_table, bishop_mask, bishop_deltas);
#else
	init_magic_bb(bitboard::magic_attack_r, bitboard::rook_magic_index, rook_attack_table, rook_mask, 52, bitboard::rook_magics, rook_deltas);
	init_magic_bb(bitboard::magic_attack_r, bitboard::bishop_magic_index, bishop_attack_table, bishop_mask, 55, bitboard::bishop_magics, bishop_deltas);
#endif
}

uint64_t sliding_attacks(const int sq, const uint64_t block, const int deltas[4][2],
	const int f_min, const int f_max, const int r_min, const int r_max)
{
	uint64_t result = 0;
	const auto rk = sq / 8;
	const auto fl = sq % 8;

	for (auto direction = 0; direction < 4; direction++)
	{
		const auto dx = deltas[direction][0];
		const auto dy = deltas[direction][1];
		for (auto f = fl + dx, r = rk + dy;
			(dx == 0 || f >= f_min && f <= f_max) && (dy == 0 || r >= r_min && r <= r_max);
			f += dx, r += dy)
		{
			result |= square_bb[f + r * 8];
			if (block & square_bb[f + r * 8])
				break;
		}
	}
	return result;
}