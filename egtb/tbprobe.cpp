/*
  Copyright (c) 2011-2013 Ronald de Man
  This file may be redistributed and/or modified without restrictions.

  tbcore.c contains engine-independent routines of the tablebase probing code.
  This file should not need too much adaptation to add tablebase probing to
  a particular engine, provided the engine is written in C or C++.
*/

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../position.h"
#include "../zobrist.h"
#include "../movegen.h"
#include "../bitboard.h"

#include "tbprobe.h"
#include "tbcore.h"

#include "tbcore.cpp"

#if !defined(_MSC_VER)
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

enum syzygy_piece_type
{
	syz_none,
	syz_pawn,
	syz_knight,
	syz_bishop,
	syz_rook,
	syz_queen,
	syz_king,
	syz_nb
};

inline syzygy_piece_type& operator++(syzygy_piece_type& d)
{
	return d = static_cast<syzygy_piece_type>(static_cast<int>(d) + 1);
}

inline syzygy_piece_type& operator--(syzygy_piece_type& d)
{
	return d = static_cast<syzygy_piece_type>(static_cast<int>(d) - 1);
}

constexpr uint8_t pt_from_syz[syz_nb] = {
	no_piecetype, pt_pawn, pt_knight, pt_bishop, pt_rook, pt_queen, pt_king
};

constexpr syzygy_piece_type syz_from_pt[num_piecetypes] = {
	syz_none, syz_king, syz_pawn, syz_knight, syz_bishop, syz_rook, syz_queen
};


static void prt_str(const position& pos, char* str, const int mirror)
{
	auto pt = syz_none;
	auto i = 0;

	auto color = !mirror ? white : black;
	for (pt = syz_king; pt >= syz_pawn; --pt)
		for (i = pos.number(color, pt_from_syz[pt]); i > 0; i--)
			*str++ = pchr[6 - pt];
	*str++ = 'v';
	color = ~color;
	for (pt = syz_king; pt >= syz_pawn; --pt)
		for (i = pos.number(color, pt_from_syz[pt]); i > 0; i--)
			*str++ = pchr[6 - pt];
	*str++ = 0;
}

static uint64 calc_key(const position& pos, const int mirror)
{
	auto pt = syz_none;
	auto i = 0;
	uint64 key = 0;

	auto color = !mirror ? white : black;
	for (pt = syz_pawn; pt <= syz_king; ++pt)
		for (i = pos.number(color, pt_from_syz[pt]); i > 0; i--)
			key ^= zobrist::psq[make_piece(white, pt_from_syz[pt])][i - 1];
	color = ~color;
	for (pt = syz_pawn; pt <= syz_king; ++pt)
		for (i = pos.number(color, pt_from_syz[pt]); i > 0; i--)
			key ^= zobrist::psq[make_piece(black, pt_from_syz[pt])][i - 1];

	return key;
}

static uint64 calc_key_from_pcs(const int* pcs, const int mirror)
{
	auto pt = syz_none;
	auto i = 0;
	uint64 key = 0;

	auto color = !mirror ? 0 : 8;
	for (pt = syz_pawn; pt <= syz_king; ++pt)
		for (i = 0; i < pcs[color + pt]; i++)
			key ^= zobrist::psq[make_piece(white, pt_from_syz[pt])][i];
	color ^= 8;
	for (pt = syz_pawn; pt <= syz_king; ++pt)
		for (i = 0; i < pcs[color + pt]; i++)
			key ^= zobrist::psq[make_piece(black, pt_from_syz[pt])][i];

	return key;
}

bool is_little_endian()
{
	union
	{
		int i;
		char c[sizeof(int)];
	} x;
	x.i = 1;
	return x.c[0] == 1;
}

static ubyte decompress_pairs(pairs_data* d, const uint64 idx)
{
	static const auto islittleendian = is_little_endian();
	return islittleendian
		? decompress_pairs<true>(d, idx)
		: decompress_pairs<false>(d, idx);
}

static int probe_wdl_table(const position& pos, int* success)
{
	auto i = 0;
	ubyte res = 0;
	int p[tb_pieces];

	const auto key = pos.material_key();

	if (key == (zobrist::psq[w_king][0] ^ zobrist::psq[b_king][0]))
		return 0;

	auto* ptr2 = tb_hash[key >> (64 - tb_hash_bits)];
	for (i = 0; i < hash_max; i++)
		if (ptr2[i].key == key) break;
	if (i == hash_max)
	{
		*success = 0;
		return 0;
	}

	auto* ptr = ptr2[i].ptr;
	if (!ptr->ready)
	{
		LOCK(tb_mutex);
		if (!ptr->ready)
		{
			char str[16];
			prt_str(pos, str, ptr->key != key);
			if (!init_table_wdl(ptr, str))
			{
				ptr2[i].key = 0ULL;
				*success = 0;
				UNLOCK(tb_mutex);
				return 0;
			}
#ifdef _MSC_VER
			_ReadWriteBarrier();
#else
			__asm__ __volatile__("" ::: "memory");
#endif
			ptr->ready = 1;
		}
		UNLOCK(tb_mutex);
	}

	auto b_side = 0, mirror = 0, c_mirror = 0;
	if (!ptr->symmetric)
	{
		if (key != ptr->key)
		{
			c_mirror = 8;
			mirror = 0x38;
			b_side = pos.on_move() == white;
		}
		else
		{
			c_mirror = mirror = 0;
			b_side = !(pos.on_move() == white);
		}
	}
	else
	{
		c_mirror = pos.on_move() == white ? 0 : 8;
		mirror = pos.on_move() == white ? 0 : 0x38;
		b_side = 0;
	}

	if (uint64 idx = 0; !ptr->has_pawns)
	{
		const auto* entry = reinterpret_cast<struct tb_entry_piece*>(ptr);
		const auto* pc = entry->pieces[b_side];
		for (i = 0; i < entry->num;)
		{
			auto bb = pos.pieces(static_cast<side>((pc[i] ^ c_mirror) >> 3),
				pt_from_syz[pc[i] & 0x07]);
			do
			{
				p[i++] = pop_lsb(&bb);
			} while (bb);
		}
		idx = encode_piece(entry, entry->norm[b_side], p, entry->factor[b_side]);
		res = decompress_pairs(entry->precomp[b_side], idx);
	}
	else
	{
		const auto* entry = reinterpret_cast<struct tb_entry_pawn*>(ptr);
		const auto k = entry->file[0].pieces[0][0] ^ c_mirror;
		auto bb = pos.pieces(static_cast<side>(k >> 3), pt_from_syz[k & 0x07]);
		i = 0;
		do
		{
			p[i++] = pop_lsb(&bb) ^ mirror;
		} while (bb);
		const auto f = pawn_file(entry, p);
		const auto* pc = entry->file[f].pieces[b_side];
		while (i < entry->num)
		{
			bb = pos.pieces(static_cast<side>((pc[i] ^ c_mirror) >> 3),
				pt_from_syz[pc[i] & 0x07]);
			do
			{
				p[i++] = pop_lsb(&bb) ^ mirror;
			} while (bb);
		}
		idx = encode_pawn(entry, entry->file[f].norm[b_side], p, entry->file[f].factor[b_side]);
		res = decompress_pairs(entry->file[f].precomp[b_side], idx);
	}

	return static_cast<int>(res) - 2;
}

static int probe_dtz_table(const position& pos, const int wdl, int* success)
{
	tb_entry* ptr = nullptr;
	auto i = 0, res = 0;
	int p[tb_pieces];

	const auto key = pos.material_key();

	if (dtz_table[0].key1 != key && dtz_table[0].key2 != key)
	{
		for (i = 1; i < dtz_entries; i++)
			if (dtz_table[i].key1 == key) break;
		if (i < dtz_entries)
		{
			const auto table_entry = dtz_table[i];
			for (; i > 0; i--)
				dtz_table[i] = dtz_table[i - 1];
			dtz_table[0] = table_entry;
		}
		else
		{
			const auto* ptr2 = tb_hash[key >> (64 - tb_hash_bits)];
			for (i = 0; i < hash_max; i++)
				if (ptr2[i].key == key) break;
			if (i == hash_max)
			{
				*success = 0;
				return 0;
			}
			ptr = ptr2[i].ptr;
			char str[16];
			const int mirror = ptr->key != key;
			prt_str(pos, str, mirror);
			if (dtz_table[dtz_entries - 1].entry)
				free_dtz_entry(dtz_table[dtz_entries - 1].entry);
			for (i = dtz_entries - 1; i > 0; i--)
				dtz_table[i] = dtz_table[i - 1];
			load_dtz_table(str, calc_key(pos, mirror), calc_key(pos, !mirror));
		}
	}

	ptr = dtz_table[0].entry;
	if (!ptr)
	{
		*success = 0;
		return 0;
	}

	auto b_side = 0, mirror = 0, c_mirror = 0;
	if (!ptr->symmetric)
	{
		if (key != ptr->key)
		{
			c_mirror = 8;
			mirror = 0x38;
			b_side = pos.on_move() == white;
		}
		else
		{
			c_mirror = mirror = 0;
			b_side = !(pos.on_move() == white);
		}
	}
	else
	{
		c_mirror = pos.on_move() == white ? 0 : 8;
		mirror = pos.on_move() == white ? 0 : 0x38;
		b_side = 0;
	}

	if (uint64 idx = 0; !ptr->has_pawns)
	{
		auto* entry = reinterpret_cast<struct dtz_entry_piece*>(ptr);
		if ((entry->flags & 1) != b_side && !entry->symmetric)
		{
			*success = -1;
			return 0;
		}
		const auto* pc = entry->pieces;
		for (i = 0; i < entry->num;)
		{
			auto bb = pos.pieces(static_cast<side>((pc[i] ^ c_mirror) >> 3),
				pt_from_syz[pc[i] & 0x07]);
			do
			{
				p[i++] = pop_lsb(&bb);
			} while (bb);
		}
		idx = encode_piece(reinterpret_cast<tb_entry_piece*>(entry), entry->norm, p, entry->factor);
		res = decompress_pairs(entry->precomp, idx);

		if (entry->flags & 2)
			res = entry->map[entry->map_idx[wdl_to_map[wdl + 2]] + res];

		if (!(entry->flags & pa_flags[wdl + 2]) || wdl & 1)
			res *= 2;
	}
	else
	{
		auto* entry = reinterpret_cast<struct dtz_entry_pawn*>(ptr);
		const auto k = entry->file[0].pieces[0] ^ c_mirror;
		auto bb = pos.pieces(static_cast<side>(k >> 3), pt_from_syz[k & 0x07]);
		i = 0;
		do
		{
			p[i++] = pop_lsb(&bb) ^ mirror;
		} while (bb);
		const auto f = pawn_file(reinterpret_cast<struct tb_entry_pawn*>(entry), p);
		if ((entry->flags[f] & 1) != b_side)
		{
			*success = -1;
			return 0;
		}
		const auto* pc = entry->file[f].pieces;
		while (i < entry->num)
		{
			bb = pos.pieces(static_cast<side>((pc[i] ^ c_mirror) >> 3),
				pt_from_syz[pc[i] & 0x07]);
			do
			{
				p[i++] = pop_lsb(&bb) ^ mirror;
			} while (bb);
		}
		idx = encode_pawn(reinterpret_cast<tb_entry_pawn*>(entry), entry->file[f].norm, p, entry->file[f].factor);
		res = decompress_pairs(entry->file[f].precomp, idx);

		if (entry->flags[f] & 2)
			res = entry->map[entry->map_idx[f][wdl_to_map[wdl + 2]] + res];

		if (!(entry->flags[f] & pa_flags[wdl + 2]) || wdl & 1)
			res *= 2;
	}

	return res;
}

static s_move* add_underprom_caps(const position& pos, s_move* stack, s_move* end)
{
	auto* extra = end;

	for (auto* moves = stack; moves < end; moves++)
	{
		if (const auto move = moves->move; move_type(move) >= promotion_p && !pos.empty_square(to_square(move)))
		{
			(*extra++).move = move - (1 << 12);
			(*extra++).move = move - (2 << 12);
			(*extra++).move = move - (3 << 12);
		}
	}

	return extra;
}

static int probe_ab(position& pos, int alpha, const int beta, int* success)
{
	auto val = 0;
	s_move stack[64];
	s_move* end = nullptr;

	if (!pos.is_in_check())
	{
		end = generate_moves<captures_promotions>(pos, stack);
		end = add_underprom_caps(pos, stack, end);
	}
	else
		end = generate_moves<evade_check>(pos, stack);

	for (auto* moves = stack; moves < end; moves++)
	{
		const auto capture = moves->move;
		if (!pos.is_capture_move(capture) || move_type(capture) == enpassant
			|| !pos.legal_move(capture))
			continue;
		pos.play_move(capture);
		val = -probe_ab(pos, -beta, -alpha, success);
		pos.take_move_back(capture);
		if (*success == 0) return 0;
		if (val > alpha)
		{
			if (val >= beta)
			{
				*success = 2;
				return val;
			}
			alpha = val;
		}
	}

	val = probe_wdl_table(pos, success);
	if (*success == 0) return 0;
	if (alpha >= val)
	{
		*success = 1 + (alpha > 0);
		return alpha;
	}
	*success = 1;
	return val;
}

int syzygy_probe_wdl(position& pos, int* success)
{
	*success = 1;
	auto val = probe_ab(pos, -2, 2, success);

	if (pos.enpassant_square() == no_square)
		return val;
	if (!*success) return 0;

	auto v1 = -3;
	s_move stack[192];
	s_move* moves = nullptr, * end = nullptr;

	if (!pos.is_in_check())
		end = generate_moves<captures_promotions>(pos, stack);
	else
		end = generate_moves<evade_check>(pos, stack);

	for (moves = stack; moves < end; moves++)
	{
		const auto capture = moves->move;
		if (move_type(capture) != enpassant
			|| !pos.legal_move(capture))
			continue;
		pos.play_move(capture);
		const auto v0 = -probe_ab(pos, -2, 2, success);
		pos.take_move_back(capture);
		if (*success == 0) return 0;
		if (v0 > v1) v1 = v0;
	}
	if (v1 > -3)
	{
		if (v1 >= val) val = v1;
		else if (val == 0)
		{
			for (moves = stack; moves < end; moves++)
			{
				const auto capture = moves->move;
				if (move_type(capture) == enpassant) continue;
				if (pos.legal_move(capture)) break;
			}
			if (moves == end && !pos.is_in_check())
			{
				end = generate_moves<quiet_moves>(pos, end);
				for (; moves < end; moves++)
				{
					if (const auto move = moves->move; pos.legal_move(move))
						break;
				}
			}
			if (moves == end)
				val = v1;
		}
	}

	return val;
}

static int probe_dtz_no_ep(position& pos, int* success)
{
	const auto wdl = probe_ab(pos, -2, 2, success);
	if (*success == 0) return 0;

	if (wdl == 0) return 0;

	if (*success == 2)
		return wdl == 2 ? 1 : 101;

	s_move stack[192];
	s_move* moves = nullptr;
	const s_move* end = nullptr;

	if (wdl > 0)
	{
		if (!pos.is_in_check())
			end = generate_moves<all_moves>(pos, stack);
		else
			end = generate_moves<evade_check>(pos, stack);

		for (moves = stack; moves < end; moves++)
		{
			const auto move = moves->move;
			if (piece_type(pos.moved_piece(move)) != pt_pawn || pos.is_capture_move(move)
				|| !pos.legal_move(move))
				continue;
			pos.play_move(move);
			const auto val = pos.enpassant_square() != no_square ? -syzygy_probe_wdl(pos, success) : -probe_ab(pos, -2, -wdl + 1, success);
			pos.take_move_back(move);
			if (*success == 0) return 0;
			if (val == wdl)
				return val == 2 ? 1 : 101;
		}
	}

	auto dtz = 1 + probe_dtz_table(pos, wdl, success);
	if (*success >= 0)
	{
		if (wdl & 1) dtz += 100;
		return wdl >= 0 ? dtz : -dtz;
	}

	if (wdl > 0)
	{
		auto best = 0xffff;
		for (moves = stack; moves < end; moves++)
		{
			const auto move = moves->move;
			if (pos.is_capture_move(move) || piece_type(pos.moved_piece(move)) == pt_pawn
				|| !pos.legal_move(move))
				continue;
			pos.play_move(move);
			const auto val = -syzygy_probe_dtz(pos, success);
			pos.take_move_back(move);
			if (*success == 0) return 0;
			if (val > 0 && val + 1 < best)
				best = val + 1;
		}
		return best;
	}
	auto best = -1;
	if (!pos.is_in_check())
		end = generate_moves<all_moves>(pos, stack);
	else
		end = generate_moves<evade_check>(pos, stack);
	for (moves = stack; moves < end; moves++)
	{
		auto val = 0;
		const auto move = moves->move;
		if (!pos.legal_move(move))
			continue;
		pos.play_move(move);
		if (pos.fifty_move_counter() == 0)
		{
			if (wdl == -2) val = -1;
			else
			{
				val = probe_ab(pos, 1, 2, success);
				val = val == 2 ? 0 : -101;
			}
		}
		else
		{
			val = -syzygy_probe_dtz(pos, success) - 1;
		}
		pos.take_move_back(move);
		if (*success == 0) return 0;
		if (val < best)
			best = val;
	}
	return best;
}

constexpr int wdl_to_dtz[] = {
	-1, -101, 0, 101, 1
};

int syzygy_probe_dtz(position& pos, int* success)
{
	*success = 1;
	auto val = probe_dtz_no_ep(pos, success);

	if (pos.enpassant_square() == no_square)
		return val;
	if (*success == 0) return 0;

	auto v1 = -3;

	s_move stack[192];
	s_move* moves = nullptr, * end = nullptr;

	if (!pos.is_in_check())
		end = generate_moves<captures_promotions>(pos, stack);
	else
		end = generate_moves<evade_check>(pos, stack);

	for (moves = stack; moves < end; moves++)
	{
		const auto capture = moves->move;
		if (move_type(capture) != enpassant
			|| !pos.legal_move(capture))
			continue;
		pos.play_move(capture);
		const auto v0 = -probe_ab(pos, -2, 2, success);
		pos.take_move_back(capture);
		if (*success == 0) return 0;
		if (v0 > v1) v1 = v0;
	}
	if (v1 > -3)
	{
		v1 = wdl_to_dtz[v1 + 2];
		if (val < -100)
		{
			if (v1 >= 0)
				val = v1;
		}
		else if (val < 0)
		{
			if (v1 >= 0 || v1 < -100)
				val = v1;
		}
		else if (val > 100)
		{
			if (v1 > 0)
				val = v1;
		}
		else if (val > 0)
		{
			if (v1 == 1)
				val = v1;
		}
		else if (v1 >= 0)
		{
			val = v1;
		}
		else
		{
			for (moves = stack; moves < end; moves++)
			{
				const auto move = moves->move;
				if (move_type(move) == enpassant) continue;
				if (pos.legal_move(move)) break;
			}
			if (moves == end && !pos.is_in_check())
			{
				end = generate_moves<quiet_moves>(pos, end);
				for (; moves < end; moves++)
				{
					if (const auto move = moves->move; pos.legal_move(move))
						break;
				}
			}
			if (moves == end)
				val = v1;
		}
	}

	return val;
}
