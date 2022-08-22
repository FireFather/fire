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

#include "movegen.h"

#include "fire.h"
#include "pawn.h"
#include "position.h"
#include "pragma.h"

namespace movegen
{
	// generate all piece moves
	template <side me, move_gen type>
	s_move* all_piece_moves(const position& pos, s_move* moves, const uint64_t target)
	{
		const auto only_check_moves = type == quiet_checks;

		if constexpr (type != castle_moves)
		{
			moves = moves_for_pawn<me, type>(pos, moves, target);
			moves = moves_for_piece<me, pt_knight, only_check_moves>(pos, moves, target);
			moves = moves_for_piece<me, pt_bishop, only_check_moves>(pos, moves, target);
			moves = moves_for_piece<me, pt_rook, only_check_moves>(pos, moves, target);
			moves = moves_for_piece<me, pt_queen, only_check_moves>(pos, moves, target);

			if constexpr (type != quiet_checks && type != evade_check)
			{
				const auto square_k = pos.king(me);
				auto squares = pos.attack_from<pt_king>(square_k) & target;
				while (squares)
					*moves++ = make_move(square_k, pop_lsb(&squares));
			}
		}

		if (type != captures_promotions && type != evade_check && pos.castling_possible(me))
		{
			if (pos.is_chess960())
			{
				moves = get_castle<me == white ? white_short : black_short, only_check_moves, true>(pos, moves);
				moves = get_castle<me == white ? white_long : black_long, only_check_moves, true>(pos, moves);
			}
			else
			{
				moves = get_castle<me == white ? white_short : black_short, only_check_moves, false>(pos, moves);
				moves = get_castle<me == white ? white_long : black_long, only_check_moves, false>(pos, moves);
			}
		}

		return moves;
	}

	// generate forward pawn moves
	template <side me>
	s_move* generate_pawn_advance(const position& pos, s_move* moves)
	{
		const auto ranks_6_7 = me == white ? rank_6_bb | rank_7_bb : rank_3_bb | rank_2_bb;

		uint64_t squares = shift_up<me>(pos.pieces(me, pt_pawn)) & ranks_6_7 & ~pos.pieces();
		while (squares)
		{
			const auto to = pop_lsb(&squares);
			*moves++ = make_move(to - pawn_ahead(me), to);
		}

		return moves;
	}

	// generate castle moves
	template < uint8_t castle, bool only_check_moves, bool chess960>
	s_move* get_castle(const position& pos, s_move* moves)
	{
		const auto me = castle <= white_long ? white : black;
		const auto you = me == white ? black : white;
		const auto short_castle = castle == white_short || castle == black_short;

		if (pos.castling_impossible(castle) || !pos.castling_possible(castle))
			return moves;

		const auto from_k = chess960 ? pos.king(me) : relative_square(me, e1);
		const auto to_k = relative_square(me, short_castle ? g1 : c1);

		if (const auto direction = to_k > from_k ? west : east; chess960)
		{
			for (auto sq = to_k; sq != from_k; sq += direction)
				if (pos.attack_to(sq) & pos.pieces(you))
					return moves;

			if (const auto from_r = pos.castle_rook_square(to_k); attack_rook_bb(to_k, pos.pieces() ^ from_r) & pos.pieces(you, pt_rook, pt_queen))
				return moves;
		}
		else
		{
			if (pos.attack_to(to_k) & pos.pieces(you))
				return moves;
			if (pos.attack_to(to_k + direction) & pos.pieces(you))
				return moves;
		}

		const auto move = make_move(castle_move, from_k, to_k);

		if (only_check_moves && !pos.give_check(move))
			return moves;

		*moves++ = move;
		return moves;
	}

	// generate promotions
	template <side me, move_gen mg, square delta>
	s_move* get_promotions(const position& pos, s_move* moves, const square to)
	{
		const auto you = me == white ? black : white;

		if (mg == captures_promotions || mg == evade_check || mg == all_moves)
			*moves++ = make_move(promotion_q, to - delta, to);

		if (mg == quiet_moves || mg == evade_check || mg == all_moves)
		{
			*moves++ = make_move(promotion_r, to - delta, to);
			*moves++ = make_move(promotion_b, to - delta, to);
			*moves++ = make_move(promotion_p, to - delta, to);
		}

		if (mg == quiet_checks && empty_attack[pt_knight][to] & pos.king(you))
			*moves++ = make_move(promotion_p, to - delta, to);

		return moves;
	}

	// generate pawn moves
	template <side me, move_gen mg>
	s_move* moves_for_pawn(const position& pos, s_move* moves, const uint64_t target)
	{
		const auto you = me == white ? black : white;
		const auto eighth_rank = me == white ? rank_8_bb : rank_1_bb;
		const auto seventh_rank = me == white ? rank_7_bb : rank_2_bb;
		const auto third_rank = me == white ? rank_3_bb : rank_6_bb;
		const auto straight_ahead = me == white ? north : south;
		const auto capture_right = me == white ? north_east : south_west;
		const auto capture_left = me == white ? north_west : south_east;

		uint64_t empty_squares = 0;
		const auto pawns_7th_rank = pos.pieces(me, pt_pawn) & seventh_rank;
		const auto pawns_not_7th_rank = pos.pieces(me, pt_pawn) & ~seventh_rank;

		auto your_pieces = mg == evade_check
			? pos.pieces(you) & target
			: mg == captures_promotions
			? target
			: pos.pieces(you);

		if constexpr (mg != captures_promotions)
		{
			empty_squares = mg == quiet_moves || mg == quiet_checks ? target : ~pos.pieces();

			uint64_t multi_move_bb = shift_up<me>(pawns_not_7th_rank) & empty_squares;
			uint64_t double_move_bb = shift_up<me>(multi_move_bb & third_rank) & empty_squares;

			if constexpr (mg == evade_check)
			{
				multi_move_bb &= target;
				double_move_bb &= target;
			}

			if constexpr (mg == quiet_checks)
			{
				multi_move_bb &= pos.attack_from<pt_pawn>(pos.king(you), you);
				double_move_bb &= pos.attack_from<pt_pawn>(pos.king(you), you);

				if (const auto discovered_check = pos.info()->x_ray[~pos.on_move()]; pawns_not_7th_rank & discovered_check)
				{
					const uint64_t deduction_forward = shift_up<me>(pawns_not_7th_rank & discovered_check) & empty_squares & ~get_file(
						pos.king(you));
					const uint64_t deduction_double = shift_up<me>(deduction_forward & third_rank) & empty_squares;

					multi_move_bb |= deduction_forward;
					double_move_bb |= deduction_double;
				}
			}

			while (multi_move_bb)
			{
				const auto to = pop_lsb(&multi_move_bb);
				*moves++ = make_move(to - straight_ahead, to);
			}

			while (double_move_bb)
			{
				const auto to = pop_lsb(&double_move_bb);
				*moves++ = make_move(to - straight_ahead - straight_ahead, to);
			}
		}

		if (pawns_7th_rank && (mg != evade_check || target & eighth_rank))
		{
			if constexpr (mg == captures_promotions)
				empty_squares = ~pos.pieces();

			if constexpr (mg == evade_check)
				empty_squares &= target;

			uint64_t promotion_right = shift_bb<capture_right>(pawns_7th_rank) & your_pieces;
			uint64_t promotion_left = shift_bb<capture_left>(pawns_7th_rank) & your_pieces;
			uint64_t promotion_forward = shift_up<me>(pawns_7th_rank) & empty_squares;

			while (promotion_right)
				moves = get_promotions<me, mg, capture_right>(pos, moves, pop_lsb(&promotion_right));

			while (promotion_left)
				moves = get_promotions<me, mg, capture_left>(pos, moves, pop_lsb(&promotion_left));

			while (promotion_forward)
				moves = get_promotions<me, mg, straight_ahead>(pos, moves, pop_lsb(&promotion_forward));
		}

		if constexpr (mg == captures_promotions || mg == evade_check || mg == all_moves)
		{
			uint64_t captureright = shift_bb<capture_right>(pawns_not_7th_rank) & your_pieces;
			uint64_t captureleft = shift_bb<capture_left>(pawns_not_7th_rank) & your_pieces;

			while (captureright)
			{
				const auto to = pop_lsb(&captureright);
				*moves++ = make_move(to - capture_right, to);
			}

			while (captureleft)
			{
				const auto to = pop_lsb(&captureleft);
				*moves++ = make_move(to - capture_left, to);
			}

			if (pos.enpassant_square() != no_square)
			{
				if (mg == evade_check && !(target & pos.enpassant_square() - straight_ahead))
					return moves;

				captureright = pawns_not_7th_rank & pos.attack_from<pt_pawn>(pos.enpassant_square(), you);

				assert(captureright);

				while (captureright)
					*moves++ = make_move(enpassant, pop_lsb(&captureright), pos.enpassant_square());
			}
		}

		return moves;
	}

	// generate individual piece moves
	template <side me, uint8_t piece, bool only_check_moves>
	s_move* moves_for_piece(const position& pos, s_move* moves, const uint64_t target)
	{
		assert(piece != pt_king && piece != pt_pawn);

		const auto* pl = pos.piece_list(me, piece);

		for (auto from = *pl; from != no_square; from = *++pl)
		{
			if (only_check_moves)
			{
				if ((piece == pt_bishop || piece == pt_rook || piece == pt_queen)
					&& !(empty_attack[piece][from] & target & pos.info()->check_squares[piece]))
					continue;

				if (pos.info()->x_ray[~pos.on_move()] & from)
					continue;
			}

			auto squares = pos.attack_from<piece>(from) & target;

			if (only_check_moves)
				squares &= pos.info()->check_squares[piece];

			while (squares)
				*moves++ = make_move(from, pop_lsb(&squares));
		}

		return moves;
	}
}

// generate moves
template <move_gen mg>
s_move* generate_moves(const position& pos, s_move* moves)
{
	assert(mg == captures_promotions || mg == quiet_moves || mg == all_moves || mg == castle_moves);

	const auto me = pos.on_move();

	const auto target = mg == captures_promotions
		? pos.pieces(~me)
		: mg == quiet_moves
		? ~pos.pieces()
		: mg == all_moves
		? ~pos.pieces(me)
		: 0;

	return me == white
		? movegen::all_piece_moves<white, mg>(pos, moves, target)
		: movegen::all_piece_moves<black, mg>(pos, moves, target);
}

template s_move* generate_moves<captures_promotions>(const position&, s_move*);
template s_move* generate_moves<quiet_moves>(const position&, s_move*);
template s_move* generate_moves<all_moves>(const position&, s_move*);

// generate captures by sq
s_move* generate_captures_on_square(const position& pos, s_move* moves, const square sq)
{
	const auto target = square_bb[sq];

	return pos.on_move() == white
		? movegen::all_piece_moves<white, captures_promotions>(pos, moves, target)
		: movegen::all_piece_moves<black, captures_promotions>(pos, moves, target);
}

// generate king evasion moves
template <>
s_move* generate_moves<evade_check>(const position& pos, s_move* moves)
{
	const auto me = pos.on_move();
	const auto square_k = pos.king(me);
	uint64_t attacked_squares = 0;
	auto far_attackers = pos.is_in_check() & ~pos.pieces(pt_knight, pt_pawn);

	while (far_attackers)
	{
		const auto check_square = pop_lsb(&far_attackers);
		attacked_squares |= connection_bb[check_square][square_k] ^ check_square;
	}

	auto squares = pos.attack_from<pt_king>(square_k) & ~pos.pieces(me) & ~attacked_squares;
	while (squares)
		*moves++ = make_move(square_k, pop_lsb(&squares));

	if (more_than_one(pos.is_in_check()))
		return moves;

	const auto check_square = lsb(pos.is_in_check());
	const auto target = get_between(check_square, square_k) | check_square;

	return me == white
		? movegen::all_piece_moves<white, evade_check>(pos, moves, target)
		: movegen::all_piece_moves<black, evade_check>(pos, moves, target);
}

template <>
s_move* generate_moves<pawn_advances>(const position& pos, s_move* moves)
{
	const auto me = pos.on_move();
	return me == white
		? movegen::generate_pawn_advance<white>(pos, moves)
		: movegen::generate_pawn_advance<black>(pos, moves);
}

template <>
s_move* generate_moves<queen_checks>(const position& pos, s_move* moves)
{
	return pos.on_move() == white
		? movegen::moves_for_piece<white, pt_queen, true>(pos, moves, ~pos.pieces())
		: movegen::moves_for_piece<black, pt_queen, true>(pos, moves, ~pos.pieces());
}

template <>
s_move* generate_moves<quiet_checks>(const position& pos, s_move* moves)
{
	const auto me = pos.on_move();
	auto deduction_check = pos.discovered_check_possible();

	while (deduction_check)
	{
		const auto from = pop_lsb(&deduction_check);
		const auto piece = piece_type(pos.piece_on_square(from));

		if (piece == pt_pawn)
			continue;

		auto squares = pos.attack_from(piece, from) & ~pos.pieces();

		if (piece == pt_king)
			squares &= ~empty_attack[pt_queen][pos.king(~me)];

		while (squares)
			*moves++ = make_move(from, pop_lsb(&squares));
	}

	return me == white
		? movegen::all_piece_moves<white, quiet_checks>(pos, moves, ~pos.pieces())
		: movegen::all_piece_moves<black, quiet_checks>(pos, moves, ~pos.pieces());
}

// generate all legal moves
s_move* generate_legal_moves(const position& pos, s_move* moves)
{
	const auto pinned = pos.pinned_pieces();
	const auto square_k = pos.king(pos.on_move());
	auto* p_move = moves;

	moves = pos.is_in_check()
		? generate_moves<evade_check>(pos, moves)
		: generate_moves<all_moves>(pos, moves);
	while (p_move != moves)
		if ((pinned || from_square(*p_move) == square_k || move_type(*p_move) == enpassant)
			&& !pos.legal_move(*p_move))
			*p_move = (--moves)->move;
		else
			++p_move;

	return moves;
}

// check move list for a castle move
bool legal_move_list_contains_castle(const position& pos, const uint32_t move)
{
	s_move moves[max_moves];
	if (pos.is_in_check())
		return false;

	const auto* const end = generate_moves<castle_moves>(pos, moves);

	auto* p_move = moves;
	while (p_move != end)
	{
		if (p_move->move == move)
			return true;
		p_move++;
	}
	return false;
}

// check move list for a specific move
bool legal_moves_list_contains_move(const position& pos, const uint32_t move)
{
	s_move moves[max_moves];
	const auto* const end = pos.is_in_check()
		? generate_moves<evade_check>(pos, moves)
		: generate_moves<all_moves>(pos, moves);

	auto* p_move = moves;
	while (p_move != end)
	{
		if (p_move->move == move)
			return pos.legal_move(move);
		p_move++;
	}
	return false;
}

// does the position contain at least one legal move?
bool at_least_one_legal_move(const position& pos)
{
	s_move moves[max_moves];
	const auto* const end = pos.is_in_check()
		? generate_moves<evade_check>(pos, moves)
		: generate_moves<all_moves>(pos, moves);

	auto* p_move = moves;
	while (p_move != end)
	{
		if (pos.legal_move(p_move->move))
			return true;
		p_move++;
	}
	return false;
}

