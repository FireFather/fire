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

#include "position.h"

#include <iomanip>
#include <sstream>

#include "bitboard.h"
#include "define.h"
#include "macro/side.h"
#include "macro/square.h"
#include "macro/file.h"
#include "fire.h"
#include "hash.h"
#include "movegen.h"
#include "pragma.h"
#include "thread.h"
#include "util/util.h"
#include "zobrist.h"

// position functions
uint64_t position::attack_to(const square sq, const uint64_t occupied) const
{
	return (attack_from<pt_pawn>(sq, black) & pieces(white, pt_pawn))
		| (attack_from<pt_pawn>(sq, white) & pieces(black, pt_pawn))
		| (attack_from<pt_knight>(sq) & pieces(pt_knight))
		| (attack_rook_bb(sq, occupied) & pieces(pt_rook, pt_queen))
		| (attack_bishop_bb(sq, occupied) & pieces(pt_bishop, pt_queen))
		| (attack_from<pt_king>(sq) & pieces(pt_king));
}

void position::calculate_bishop_color_key() const
{
	uint64_t key = 0;
	if (pieces(white, pt_bishop) & dark_squares)
		key ^= 0xF3094B57AC4789A2;
	if (pieces(white, pt_bishop) & ~dark_squares)
		key ^= 0x89A2F3094B57AC47;
	if (pieces(black, pt_bishop) & dark_squares)
		key ^= 0xAC4789A2F3094B57;
	if (pieces(black, pt_bishop) & ~dark_squares)
		key ^= 0x4B57AC4789A2F309;
	pos_info_->bishop_color_key = key;
}

void position::calculate_check_pins() const
{
	calculate_pins<white>();
	calculate_pins<black>();
	const auto square_k = king(~on_move_);
	pos_info_->check_squares[pt_pawn] = attack_from<pt_pawn>(square_k, ~on_move_);
	pos_info_->check_squares[pt_knight] = attack_from<pt_knight>(square_k);
	pos_info_->check_squares[pt_bishop] = attack_from<pt_bishop>(square_k);
	pos_info_->check_squares[pt_rook] = attack_from<pt_rook>(square_k);
	pos_info_->check_squares[pt_queen] = pos_info_->check_squares[pt_bishop] | pos_info_->check_squares[pt_rook];
	pos_info_->check_squares[pt_king] = 0;
}

template <side color>
void position::calculate_pins() const
{
	uint64_t result = 0;
	const auto square_k = king(color);

	auto pinners = (empty_attack[pt_rook][square_k] & pieces(~color, pt_queen, pt_rook))
		| (empty_attack[pt_bishop][square_k] & pieces(~color, pt_queen, pt_bishop));

	while (pinners)
	{
		const auto sq = pop_lsb(&pinners);

		if (const auto b = get_between(square_k, sq) & pieces(); b && !more_than_one(b))
		{
			result |= b;
			pos_info_->pin_by[lsb(b)] = sq;
		}
	}
	pos_info_->x_ray[color] = result;
}

square position::calculate_threat() const
{
	if (!pos_info_->move_counter_values)
		return no_square;

	const auto to = to_square(pos_info_->previous_move);
	switch (pos_info_->moved_piece)
	{
	case w_pawn:
		if (const auto b = pawnattack[white][to] & pieces_excluded(black, pt_pawn))
			return lsb(b);
		break;
	case b_pawn:
		if (const auto b = pawnattack[black][to] & pieces_excluded(white, pt_pawn))
			return msb(b);
		break;
	case w_knight:
		if (const auto b = empty_attack[pt_knight][to] & pieces(black, pt_rook, pt_queen))
			return lsb(b);
		break;
	case b_knight:
		if (const auto b = empty_attack[pt_knight][to] & pieces(white, pt_rook, pt_queen))
			return msb(b);
		break;
	case w_bishop:
		if (const auto b = attack_bishop_bb(to, pieces()) & pieces(black, pt_rook, pt_queen))
			return lsb(b);
		break;
	case b_bishop:
		if (const auto b = attack_bishop_bb(to, pieces()) & pieces(white, pt_rook, pt_queen))
			return msb(b);
		break;
	case w_rook:
		if (const auto b = attack_rook_bb(to, pieces()) & pieces(black, pt_queen))
			return lsb(b);
		break;
	case b_rook:
		if (const auto b = attack_rook_bb(to, pieces()) & pieces(white, pt_queen))
			return msb(b);
		break;
	case no_piece: break;
	case w_king: break;
	case w_queen: break;
	case b_king: break;
	case b_queen: break;
	case num_pieces: break;
	default: break;
	}

	return no_square;
}

void position::copy_position(const position* pos, thread* th, const position_info* copy_state)
{
	std::memcpy(this, pos, sizeof(position));
	if (th)
	{
		this_thread_ = th;
		thread_info_ = th->ti;
		cmh_info_ = th->cmhi;
		pos_info_ = th->ti->position_inf + 5;

		auto* orig_st = pos->thread_info_->position_inf + 5;
		while (orig_st < copy_state - 4)
		{
			pos_info_->key = orig_st->key;
			pos_info_++;
			orig_st++;
		}
		while (orig_st <= copy_state)
		{
			*pos_info_ = *orig_st;
			pos_info_++;
			orig_st++;
		}
		pos_info_--;
	}
}

int position::game_phase() const
{
	return std::max(0, std::min(middlegame_phase, static_cast<int>(pos_info_->phase) - 6));
}

bool position::give_check(const uint32_t move) const
{
	assert(is_ok(move));
	assert(piece_color(moved_piece(move)) == on_move_);

	const auto from = from_square(move);
	const auto to = to_square(move);
	const auto square_k = king(~on_move_);

	if (pos_info_->check_squares[piece_type(piece_on_square(from))] & to)
		return true;

	if (pos_info_->x_ray[~on_move_] & from && !aligned(from, to, square_k))
		return true;

	if (move < static_cast<uint32_t>(castle_move))
		return false;

	if (move >= static_cast<uint32_t>(promotion_p))
		return attack_bb(promotion_piece(move), to, pieces() ^ from) & square_k;

	if (move < static_cast<uint32_t>(enpassant))
	{
		const auto from_r = castle_rook_square(to);
		const auto to_r = relative_square(on_move_, from_r > from ? f1 : d1);

		return empty_attack[pt_rook][to_r] & square_k
			&& attack_rook_bb(to_r, pieces() ^ from ^ from_r | to_r | to) & square_k;
	}

	{
		const auto ep_square = make_square(file_of(to), rank_of(from));
		const auto b = pieces() ^ from ^ ep_square | to;

		return (attack_rook_bb(square_k, b) & pieces(on_move_, pt_queen, pt_rook))
			| (attack_bishop_bb(square_k, b) & pieces(on_move_, pt_queen, pt_bishop));
	}
}

void position::init()
{
	util::random rng(static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count()));

	for (auto color = white; color <= black; ++color)
		for (auto piece = pt_king; piece <= pt_queen; ++piece)
			for (auto sq = a1; sq <= h8; ++sq)
				zobrist::psq[make_piece(color, piece)][sq] = rng.rand<uint64_t>();

	for (auto f = file_a; f <= file_h; ++f)
		zobrist::enpassant[f] = rng.rand<uint64_t>();

	for (int castle = no_castle; castle <= all; ++castle)
	{
		zobrist::castle[castle] = 0;
		uint64_t b = castle;
		while (b)
		{
			const auto k = zobrist::castle[1ULL << pop_lsb(&b)];
			zobrist::castle[castle] ^= k ? k : rng.rand<uint64_t>();
		}
	}

	zobrist::on_move = rng.rand<uint64_t>();
	init_hash_move50(50);
}

void position::init_hash_move50(const int fifty_move_distance)
{
	for (auto i = 0; i < 32; i++)
		if (4 * i + 50 < 2 * fifty_move_distance)
			zobrist::hash_50_move[i] = 0;
		else
			zobrist::hash_50_move[i] = 0x0001000100010001 << i;
}

bool position::is_draw() const
{
	if (pos_info_->draw50_moves >= 2 * thread_pool.fifty_move_distance)
	{
		if (pos_info_->draw50_moves == 100)
			return !pos_info_->in_check || at_least_one_legal_move(*this);
		return true;
	}

	auto n = std::min(pos_info_->draw50_moves, pos_info_->distance_to_null_move) - 4;
	if (n < 0)
		return false;

	auto* stp = pos_info_ - 4;
	do
	{
		if (stp->key == pos_info_->key)
			return true;

		stp -= 2;
		n -= 2;
	} while (n >= 0);

	return false;
}

uint64_t position::key_after_move(const uint32_t move) const
{
	const auto from = from_square(move);
	const auto to = to_square(move);
	const auto piece = piece_on_square(from);
	const auto capture_piece = piece_on_square(to);
	auto key = pos_info_->key ^ zobrist::on_move;

	if (capture_piece)
		key ^= zobrist::psq[capture_piece][to];

	return key ^ zobrist::psq[piece][to] ^ zobrist::psq[piece][from];
}

bool position::legal_move(const uint32_t move) const
{
	assert(is_ok(move));

	const auto me = on_move_;
	const auto from = from_square(move);

	assert(piece_color(moved_piece(move)) == me);
	assert(piece_on_square(king(me)) == make_piece(me, pt_king));

	if (move_type(move) == enpassant)
	{
		const auto square_k = king(me);
		const auto to = to_square(move);
		const auto capture_square = to - pawn_ahead(me);
		const auto occupied = pieces() ^ from ^ capture_square | to;

		assert(to == enpassant_square());
		assert(moved_piece(move) == make_piece(me, pt_pawn));
		assert(piece_on_square(capture_square) == make_piece(~me, pt_pawn));
		assert(piece_on_square(to) == no_piece);

		return !(attack_rook_bb(square_k, occupied) & pieces(~me, pt_queen, pt_rook))
			&& !(attack_bishop_bb(square_k, occupied) & pieces(~me, pt_queen, pt_bishop));
	}

	if (piece_type(piece_on_square(from)) == pt_king)
		return move_type(move) == castle_move || !(attack_to(to_square(move)) & pieces(~me));

	return !(pos_info_->x_ray[on_move_] & from) || aligned(from, to_square(move), king(me));
}

template <bool yes>
void position::do_castle_move(const side me, const square from, const square to, square& from_r, square& to_r)
{
	from_r = castle_rook_square(to);
	to_r = relative_square(me, from_r > from ? f1 : d1);

	if (!chess960_)
	{
		relocate_piece(me, make_piece(me, pt_king), yes ? from : to, yes ? to : from);
		relocate_piece(me, make_piece(me, pt_rook), yes ? from_r : to_r, yes ? to_r : from_r);
	}
	else
	{
		delete_piece(me, make_piece(me, pt_king), yes ? from : to);
		delete_piece(me, make_piece(me, pt_rook), yes ? from_r : to_r);
		board_[yes ? from : to] = board_[yes ? from_r : to_r] = no_piece;
		move_piece(me, make_piece(me, pt_king), yes ? to : from);
		move_piece(me, make_piece(me, pt_rook), yes ? to_r : from_r);
	}
}

void position::play_move(const uint32_t move)
{
	const bool gives_check = move < static_cast<uint32_t>(castle_move) && !discovered_check_possible()
		? pos_info_->check_squares[piece_type(piece_on_square(from_square(move)))] & to_square(move)
		: give_check(move);

	play_move(move, gives_check);
}

void position::play_move(const uint32_t move, const bool gives_check)
{
	assert(is_ok(move));

	++nodes_;
	auto key = pos_info_->key ^ zobrist::on_move;

	std::memcpy(pos_info_ + 1, pos_info_, offsetof(position_info, key));
	pos_info_++;

	pos_info_->draw50_moves = (pos_info_ - 1)->draw50_moves + 1;
	pos_info_->distance_to_null_move = (pos_info_ - 1)->distance_to_null_move + 1;

	const auto me = on_move_;
	const auto you = ~me;
	const auto from = from_square(move);
	const auto to = to_square(move);
	const auto piece = piece_on_square(from);
	auto capture_piece = no_piece;

	assert(piece_color(piece) == me);

	if (move_type(move) == castle_move)
	{
		assert(piece_type(piece) == pt_king);

		auto from_r = no_square, to_r = no_square;
		do_castle_move<true>(me, from, to, from_r, to_r);

		capture_piece = no_piece;
		const auto my_rook = make_piece(me, pt_rook);
		pos_info_->psq += pst::psq[my_rook][to_r] - pst::psq[my_rook][from_r];
		key ^= zobrist::psq[my_rook][from_r] ^ zobrist::psq[my_rook][to_r];
	}
	else
		capture_piece = move_type(move) == enpassant ? make_piece(you, pt_pawn) : piece_on_square(to);

	if (capture_piece)
	{
		assert(piece_type(capture_piece) != pt_king);
		assert(piece_color(capture_piece) == you);
		auto capture_square = to;

		if (piece_type(capture_piece) == pt_pawn)
		{
			if (move_type(move) == enpassant)
			{
				capture_square -= pawn_ahead(me);

				assert(piece_type(piece) == pt_pawn);
				assert(to == pos_info_->enpassant_square);
				assert(relative_rank(me, to) == rank_6);
				assert(piece_on_square(to) == no_piece);
				assert(piece_on_square(capture_square) == make_piece(you, pt_pawn));

				board_[capture_square] = no_piece;
			}

			pos_info_->pawn_key ^= zobrist::psq[capture_piece][capture_square];
		}
		else
			pos_info_->non_pawn_material[you] -= material_value[capture_piece];

		pos_info_->phase -= static_cast<uint8_t>(piece_phase[capture_piece]);

		delete_piece(you, capture_piece, capture_square);

		key ^= zobrist::psq[capture_piece][capture_square];
		pos_info_->material_key ^= zobrist::psq[capture_piece][piece_number_[capture_piece]];
		prefetch(thread_info_->material_table[pos_info_->material_key ^ pos_info_->bishop_color_key]);

		if (piece_type(capture_piece) == pt_bishop)
			calculate_bishop_color_key();

		pos_info_->psq -= pst::psq[capture_piece][capture_square];

		pos_info_->draw50_moves = 0;
	}

	key ^= zobrist::psq[piece][from] ^ zobrist::psq[piece][to];

	if (pos_info_->enpassant_square != no_square)
	{
		key ^= zobrist::enpassant[file_of(pos_info_->enpassant_square)];
		pos_info_->enpassant_square = no_square;
	}

	if (pos_info_->castle_possibilities && castle_mask_[from] | castle_mask_[to])
	{
		const uint8_t castle = castle_mask_[from] | castle_mask_[to];
		key ^= zobrist::castle[pos_info_->castle_possibilities & castle];
		pos_info_->castle_possibilities &= ~castle;
	}

	if (move_type(move) != castle_move)
		relocate_piece(me, piece, from, to);

	if (piece_type(piece) == pt_pawn)
	{
		if ((static_cast<int>(to) ^ static_cast<int>(from)) == 16
			&& attack_from<pt_pawn>(to - pawn_ahead(me), me) & pieces(you, pt_pawn))
		{
			pos_info_->enpassant_square = (from + to) / 2;
			key ^= zobrist::enpassant[file_of(pos_info_->enpassant_square)];
		}

		else if (move >= static_cast<uint32_t>(promotion_p))
		{
			const auto promotion = make_piece(me, promotion_piece(move));

			assert(relative_rank(me, to) == rank_8);
			assert(piece_type(promotion) >= pt_knight && piece_type(promotion) <= pt_queen);

			delete_piece(me, piece, to);
			move_piece(me, promotion, to);

			key ^= zobrist::psq[piece][to] ^ zobrist::psq[promotion][to];
			pos_info_->pawn_key ^= zobrist::psq[piece][to];
			pos_info_->material_key ^= zobrist::psq[promotion][piece_number_[promotion] - 1]
				^ zobrist::psq[piece][piece_number_[piece]];

			if (piece_type(promotion) == pt_bishop)
				calculate_bishop_color_key();

			pos_info_->psq += pst::psq[promotion][to] - pst::psq[piece][to];

			pos_info_->non_pawn_material[me] += material_value[promotion];
			pos_info_->phase += static_cast<uint8_t>(piece_phase[promotion]);
		}

		pos_info_->pawn_key ^= zobrist::psq[piece][from] ^ zobrist::psq[piece][to];
		prefetch2(thread_info_->pawn_table[pos_info_->pawn_key]);

		pos_info_->draw50_moves = 0;
	}

	main_hash.prefetch_entry(key);

	piece_bb_[all_pieces] = color_bb_[white] | color_bb_[black];

	pos_info_->psq += pst::psq[piece][to] - pst::psq[piece][from];

	pos_info_->captured_piece = capture_piece;
	pos_info_->moved_piece = piece_on_square(to);
	pos_info_->previous_move = move;
	pos_info_->move_counter_values = &cmh_info_->counter_move_stats[piece][to];
	pos_info_->eval_positional = no_eval;

	pos_info_->key = key;

	on_move_ = ~on_move_;
	pos_info_->in_check = gives_check ? attack_to(king(you)) & pieces(me) : 0;
	pos_info_->move_repetition = is_draw();
	calculate_check_pins();
}

void position::play_null_move()
{
	++nodes_;

	auto key = pos_info_->key ^ zobrist::on_move;
	if (pos_info_->enpassant_square != no_square)
		key ^= zobrist::enpassant[file_of(pos_info_->enpassant_square)];

	main_hash.prefetch_entry(key);

	std::memcpy(pos_info_ + 1, pos_info_, offsetof(position_info, key));
	pos_info_++;

	pos_info_->key = key;
	pos_info_->draw50_moves = (pos_info_ - 1)->draw50_moves + 1;
	pos_info_->distance_to_null_move = 0;

	pos_info_->enpassant_square = no_square;
	pos_info_->in_check = 0;
	pos_info_->captured_piece = no_piece;
	pos_info_->previous_move = null_move;
	pos_info_->move_counter_values = nullptr;
	pos_info_->eval_positional = (pos_info_ - 1)->eval_positional;
	pos_info_->eval_factor = (pos_info_ - 1)->eval_factor;

	on_move_ = ~on_move_;
	pos_info_->move_repetition = is_draw();
	calculate_check_pins();
}

bool position::see_test(const uint32_t move, const int limit) const
{
	if (move_type(move) == castle_move)
		return 0 >= limit;

	const auto* const see_value = see_values();
	const auto from = from_square(move);
	const auto to = to_square(move);
	auto occupied = pieces();
	const auto me = piece_color(piece_on_square(from));

	auto value = see_value[piece_on_square(to)] - limit;
	if (move_type(move) == enpassant)
	{
		occupied ^= to - pawn_ahead(me);
		value += see_value[pt_pawn];
	}
	if (value < 0)
		return false;

	value -= see_value[piece_on_square(from)];
	if (value >= 0)
		return true;

	occupied ^= from;
	auto attackers = attack_to(to, occupied) & occupied;

	do
	{
		auto my_attackers = attackers & pieces(~me);
		if (!my_attackers)
			return true;
		{
			auto pinned = my_attackers & pos_info_->x_ray[~me];
			while (pinned)
			{
				if (const auto sq = pop_lsb(&pinned); occupied & pos_info_->pin_by[sq])
				{
					my_attackers ^= sq;
					if (!my_attackers)
						return true;
				}
			}
		}

		uint64_t bb = 0;
		auto capture_piece = no_piecetype;
		if ((bb = my_attackers & pieces(pt_pawn)))
			capture_piece = pt_pawn;
		else if ((bb = my_attackers & pieces(pt_knight)))
			capture_piece = pt_knight;
		else if ((bb = my_attackers & pieces(pt_bishop)))
			capture_piece = pt_bishop;
		else if ((bb = my_attackers & pieces(pt_rook)))
			capture_piece = pt_rook;
		else if ((bb = my_attackers & pieces(pt_queen)))
			capture_piece = pt_queen;
		else
			return attackers & pieces(me);

		value += see_value[capture_piece];
		if (value < 0)
			return false;

		occupied ^= bb & -bb;

		if (!(capture_piece & 1))
			attackers |= attack_bishop_bb(to, occupied) & (pieces(pt_bishop) | pieces(pt_queen));
		if (capture_piece >= pt_rook)
			attackers |= attack_rook_bb(to, occupied) & (pieces(pt_rook) | pieces(pt_queen));

		attackers &= occupied;

		my_attackers = attackers & pieces(me);
		if (!my_attackers)
			return false;
		{
			auto pinned = my_attackers & pos_info_->x_ray[me];
			while (pinned)
			{
				if (const auto sq = pop_lsb(&pinned); occupied & pos_info_->pin_by[sq])
				{
					my_attackers ^= sq;
					if (!my_attackers)
						return false;
				}
			}
		}

		if ((bb = my_attackers & pieces(pt_pawn)))
			capture_piece = pt_pawn;
		else if ((bb = my_attackers & pieces(pt_knight)))
			capture_piece = pt_knight;
		else if ((bb = my_attackers & pieces(pt_bishop)))
			capture_piece = pt_bishop;
		else if ((bb = my_attackers & pieces(pt_rook)))
			capture_piece = pt_rook;
		else if ((bb = my_attackers & pieces(pt_queen)))
			capture_piece = pt_queen;
		else
			return !(attackers & pieces(~me));

		value -= see_value[capture_piece];
		if (value >= 0)
			return true;

		occupied ^= bb & -bb;

		if (!(capture_piece & 1))
			attackers |= attack_bishop_bb(to, occupied) & (pieces(pt_bishop) | pieces(pt_queen));
		if (capture_piece >= pt_rook)
			attackers |= attack_rook_bb(to, occupied) & (pieces(pt_rook) | pieces(pt_queen));
		attackers &= occupied;
	} while (true);
}

const int* position::see_values()
{
	return see_value_simple;
}

void position::set_castling_possibilities(const side color, const square from_r)
{
	const auto from_k = king(color);
	const auto castle = static_cast<uint8_t>(white_short << ((from_k >= from_r) + 2 * color));

	const auto to_k = relative_square(color, from_k < from_r ? g1 : c1);
	const auto to_r = relative_square(color, from_k < from_r ? f1 : d1);

	pos_info_->castle_possibilities |= castle;
	castle_mask_[from_k] |= castle;
	castle_mask_[from_r] |= castle;
	castle_rook_square_[to_k] = from_r;

	uint64_t pad = 0;
	for (auto sq = std::min(from_r, to_r); sq <= std::max(from_r, to_r); ++sq)
		pad |= sq;

	for (auto sq = std::min(from_k, to_k); sq <= std::max(from_k, to_k); ++sq)
		pad |= sq;

	castle_path_[castle] = pad & ~(square_bb[from_k] | square_bb[from_r]);

	if (from_k != relative_square(color, e1))
		chess960_ = true;
	if (from_k < from_r && from_r != relative_square(color, h1))
		chess960_ = true;
	if (from_k >= from_r && from_r != relative_square(color, a1))
		chess960_ = true;
}


void position::set_position_info(position_info* si) const
{
	si->key = si->material_key = 0;
	si->non_pawn_material[white] = si->non_pawn_material[black] = mat_0;
	si->psq = 0;
	si->phase = 0;

	si->in_check = attack_to(king(on_move_)) & pieces(~on_move_);

	for (auto b = pieces(); b;)
	{
		const auto sq = pop_lsb(&b);
		const auto piece = piece_on_square(sq);
		si->key ^= zobrist::psq[piece][sq];
		si->psq += pst::psq[piece][sq];
		si->phase += static_cast<uint8_t>(piece_phase[piece_type(piece)]);
	}

	if (si->enpassant_square != no_square)
		si->key ^= zobrist::enpassant[file_of(si->enpassant_square)];

	if (on_move_ == black)
		si->key ^= zobrist::on_move;

	si->key ^= zobrist::castle[si->castle_possibilities];

	si->pawn_key = 0x1234567890ABCDEF;
	for (auto b = pieces(pt_pawn); b;)
	{
		const auto sq = pop_lsb(&b);
		si->pawn_key ^= zobrist::psq[piece_on_square(sq)][sq];
	}

	for (auto color = white; color <= black; ++color)
		for (auto piece = pt_king; piece <= pt_queen; ++piece)
			for (auto cnt = 0; cnt < piece_number_[make_piece(color, piece)]; ++cnt)
				si->material_key ^= zobrist::psq[make_piece(color, piece)][cnt];

	calculate_bishop_color_key();

	for (auto color = white; color <= black; ++color)
		for (auto piece = pt_knight; piece <= pt_queen; ++piece)
			si->non_pawn_material[color] += material_value[piece] * static_cast<int>(piece_number_[make_piece(color, piece)]);
}

position& position::set(const std::string& fen_str, const bool is_chess960, thread* th)
{
	assert(th != nullptr);

	uint8_t r = 0, token = 0;
	size_t idx = 0;
	auto sq = a8;
	std::istringstream ss(fen_str);

	std::memset(this, 0, sizeof(position));
	std::fill_n(&piece_list_[0][0], sizeof piece_list_ / sizeof(square), no_square);
	pos_info_ = th->ti->position_inf + 5;
	std::memset(pos_info_, 0, sizeof(position_info));
	chess960_ = is_chess960;

	ss >> std::noskipws;

	while (ss >> token && !isspace(token))
	{
		if (isdigit(token))
			sq += static_cast<square>(token - '0');

		else if (token == '/')
			sq -= static_cast<square>(16);

		else if ((idx = util::piece_to_char.find(token)) != std::string::npos)
		{
			move_piece(piece_color(static_cast<ptype>(idx)), static_cast<ptype>(idx), sq);
			++sq;
		}
	}
	piece_bb_[all_pieces] = color_bb_[white] | color_bb_[black];

	ss >> token;
	on_move_ = token == 'w' ? white : black;
	ss >> token;

	while (ss >> token && !isspace(token))
	{
		auto rsq = no_square;
		const auto color = islower(token) ? black : white;
		const auto rook = make_piece(color, pt_rook);

		token = static_cast<char>(toupper(token));

		if (token == 'K')
			for (rsq = relative_square(color, h1); piece_on_square(rsq) != rook; --rsq)
			{
			}

		else if (token == 'Q')
			for (rsq = relative_square(color, a1); piece_on_square(rsq) != rook; ++rsq)
			{
			}

		else if (token >= 'A' && token <= 'H')
			rsq = make_square(static_cast<file>(token - 'A'), relative_rank(color, rank_1));

		else
			continue;

		set_castling_possibilities(color, rsq);
	}

	if (uint8_t f = 0; ss >> f && (f >= 'a' && f <= 'h')
		&& (ss >> r && (r == '3' || r == '6')))
	{
		pos_info_->enpassant_square = make_square(static_cast<file>(f - 'a'), static_cast<rank>(r - '1'));

		if (!(attack_to(pos_info_->enpassant_square) & pieces(on_move_, pt_pawn)))
			pos_info_->enpassant_square = no_square;
	}
	else
		pos_info_->enpassant_square = no_square;

	ss >> std::skipws >> pos_info_->draw50_moves >> game_ply_;

	game_ply_ = std::max(2 * (game_ply_ - 1), 0) + (on_move_ == black);

	this_thread_ = th;
	thread_info_ = th->ti;
	cmh_info_ = th->cmhi;
	set_position_info(pos_info_);
	calculate_check_pins();

	return *this;
}

void position::take_move_back(const uint32_t move)
{
	assert(is_ok(move));

	on_move_ = ~on_move_;

	const auto me = on_move_;
	const auto from = from_square(move);
	const auto to = to_square(move);
	auto piece = piece_on_square(to);

	assert(empty_square(from) || move_type(move) == castle_move);
	assert(piece_type(pos_info_->captured_piece) != pt_king);
	assert(piece == pos_info_->moved_piece);

	if (move < static_cast<uint32_t>(castle_move))
	{
		relocate_piece(me, piece, to, from);
		if (pos_info_->captured_piece)
			move_piece(~me, pos_info_->captured_piece, to);
	}
	else
	{
		if (move >= static_cast<uint32_t>(promotion_p))
		{
			assert(relative_rank(me, to) == rank_8);
			assert(piece_type(piece) == promotion_piece(move));
			assert(piece_type(piece) >= pt_knight && piece_type(piece) <= pt_queen);

			delete_piece(me, piece, to);
			piece = make_piece(me, pt_pawn);
			move_piece(me, piece, to);
		}

		if (move_type(move) == castle_move)
		{
			auto from_r = no_square, to_r = no_square;
			do_castle_move<false>(me, from, to, from_r, to_r);
		}
		else
		{
			relocate_piece(me, piece, to, from);

			if (pos_info_->captured_piece)
			{
				auto capture_square = to;

				if (move_type(move) == enpassant)
				{
					capture_square -= pawn_ahead(me);

					assert(piece_type(piece) == pt_pawn);
					assert(to == (pos_info_ - 1)->enpassant_square);
					assert(relative_rank(me, to) == rank_6);
					assert(piece_on_square(capture_square) == no_piece);
					assert(pos_info_->captured_piece == make_piece(~me, pt_pawn));
				}

				move_piece(~me, pos_info_->captured_piece, capture_square);
			}
		}
	}
	piece_bb_[all_pieces] = color_bb_[white] | color_bb_[black];

	pos_info_--;
}

void position::take_null_back()
{
	pos_info_--;
	on_move_ = ~on_move_;
}

bool position::valid_move(const uint32_t move) const
{
	const auto me = on_move_;
	const auto from = from_square(move);

	if (!(pieces(me) & from))
		return false;

	const auto to = to_square(move);
	const auto piece = piece_type(moved_piece(move));

	if (move >= static_cast<uint32_t>(castle_move))
	{
		if (move >= static_cast<uint32_t>(promotion_p))
		{
			if (piece != pt_pawn)
				return false;
		}
		else if (move < static_cast<uint32_t>(enpassant))
			return legal_move_list_contains_castle(*this, move);
		else
			return legal_moves_list_contains_move(*this, move);
	}
	else
	{
	}

	if (pieces(me) & to)
		return false;

	if (piece == pt_pawn)
	{
		if ((move >= static_cast<uint32_t>(promotion_p)) ^ (rank_of(to) == relative_rank(me, rank_8)))
			return false;

		if (!(attack_from<pt_pawn>(from, me) & pieces(~me) & to)
			&& !(from + pawn_ahead(me) == to && empty_square(to))
			&& !(from + 2 * pawn_ahead(me) == to
				&& rank_of(from) == relative_rank(me, rank_2)
				&& empty_square(to)
				&& empty_square(to - pawn_ahead(me))))
			return false;
	}
	else if (!(attack_from(piece, from) & to))
		return false;

	if (is_in_check())
	{
		if (piece != pt_king)
		{
			if (more_than_one(is_in_check()))
				return false;

			if (!((get_between(lsb(is_in_check()), king(me)) | is_in_check()) & to))
				return false;
		}
		else if (attack_to(to, pieces() ^ from) & pieces(~me))
			return false;
	}

	return true;
}




