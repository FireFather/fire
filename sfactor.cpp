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
#include "endgame.h"
#include "pawn.h"
#include "position.h"

template <side strong>
sfactor endgame_kbpk(const position& pos)
{
	const auto weak = ~strong;
	assert(pos.non_pawn_material(strong) == mat_bishop);
	assert(pos.number(strong, pt_pawn) >= 1);

	const auto black_attack_bb = endgame::attack_king_inc(pos.king(weak));

	if (const auto white_attack_bb = pos.attack_from<pt_king>(pos.king(strong))
		| pos.attack_from<pt_bishop>(pos.piece_square(strong, pt_bishop)); pos.number(strong, pt_pawn) == 1 && pos.on_move() == weak
		&& pos.pieces(strong, pt_pawn) & black_attack_bb & ~white_attack_bb)
		return draw_factor;

	auto white_king = relative_square(strong, pos.king(strong));
	auto black_king = relative_square(strong, pos.king(weak));
	auto bishop = relative_square(strong, pos.piece_square(strong, pt_bishop));
	auto pawn = relative_square(strong, pos.piece_list(strong, pt_pawn)[0]);
	uint64_t white_pawns = 0, black_pawns = 0;

	if (dark_squares & bishop)
	{
		white_king = static_cast<square>(white_king ^ 07);
		black_king = static_cast<square>(black_king ^ 07);
		bishop = static_cast<square>(bishop ^ 07);
		pawn = static_cast<square>(pawn ^ 07);

		for (auto i = 0; i < pos.number(strong, pt_pawn); i++)
			white_pawns |= relative_square(strong, static_cast<square>(pos.piece_list(strong, pt_pawn)[i] ^ 07));

		for (auto i = 0; i < pos.number(weak, pt_pawn); i++)
			black_pawns |= relative_square(strong, static_cast<square>(pos.piece_list(weak, pt_pawn)[i] ^ 07));
	}
	else
	{
		for (auto i = 0; i < pos.number(strong, pt_pawn); i++)
			white_pawns |= relative_square(strong, pos.piece_list(strong, pt_pawn)[i]);

		for (auto i = 0; i < pos.number(weak, pt_pawn); i++)
			black_pawns |= relative_square(strong, pos.piece_list(weak, pt_pawn)[i]);
	}

	const auto king_cover_bb = endgame::attack_king_inc(black_king);

	if (pos.number(strong, pt_pawn) > 1)
		pawn = no_square;

	if (!(white_pawns & ~file_h_bb))
	{
		if (king_cover_bb & bb(h8))
		{
			if (white_pawns & bb(h5) && (black_pawns & bb2(g7, h6)) == bb2(g7, h6))
			{
			}
			else
				return draw_factor;
		}
		if (!(black_pawns & file_g_bb)
			&& square_distance[g7][black_king] < square_distance[g7][white_king] + 1 - (pos.on_move() == strong)
			&& square_distance[h8][black_king] < 7 - rank_of(msb(white_pawns)) - (pos.on_move() == strong))
			return static_cast<sfactor>(normal_factor / 4);

		if (!(white_pawns & ~bb2(h2, h3)) && black_pawns & bb(h4))
			return static_cast<sfactor>(normal_factor / 4);
	}

	if (pawn == g6 && bishop == h7 && king_cover_bb & bb(h8))
		return draw_factor;

	if (!(white_pawns & ~file_a_bb)
		&& white_pawns & bb(a6)
		&& black_pawns & bb(a7)
		&& king_cover_bb & bb(b8)
		&& (!(black_pawns & file_b_bb)
		|| rank_of(lsb(white_pawns & file_a_bb)) >= rank_of(msb(black_pawns & file_b_bb))))
		return draw_factor;

	if (!(white_pawns & ~file_g_bb)
		&& white_pawns & bb(g6)
		&& black_pawns & bb(g7)
		&& king_cover_bb & bb2(g8, f8))
		return draw_factor;

	if (pawn == b6
		&& black_pawns & bb(b7)
		&& king_cover_bb & bb2(b8, c8))
		return draw_factor;

	if (white_pawns == bb2(a6, b5)
		&& (black_pawns & (bb2(a7, b6) | bb3(b7, c7, c6))) == bb2(a7, b6)
		&& king_cover_bb & bb(b8))
		return draw_factor;

	if (white_pawns == bb3(a6, b5, c4)
		&& (black_pawns & (bb3(a7, b6, c5) | bb3(b7, c7, c6) | bb3(d7, d6, d5))) == bb3(a7, b6, c5)
		&& king_cover_bb & bb(b8))
		return draw_factor;

	return no_factor;
}

template sfactor endgame_kbpk<white>(const position& pos);
template <side strong>
sfactor endgame_kbpkb(const position& pos)
{
	const auto weak = ~strong;
	const int weak_side_to_move = pos.on_move() == weak;
	if (weak_side_to_move
		&& pos.attack_from<pt_bishop>(pos.piece_square(strong, pt_pawn)) & pos.pieces(weak, pt_bishop))
		return draw_factor;

	if (different_color(pos.piece_square(strong, pt_bishop), pos.piece_square(weak, pt_bishop)))
	{
		const auto white_king = pos.king(strong);
		const auto white_bishop = pos.piece_square(strong, pt_bishop);
		const auto pawn = pos.piece_square(strong, pt_pawn);
		const auto black_king = pos.king(weak);
		const auto black_bishop = pos.piece_square(weak, pt_bishop);
		const auto white_attack = pos.attack_from<pt_pawn>(pawn, white)
			| pos.attack_from<pt_bishop>(white_bishop)
			| pos.attack_from<pt_king>(white_king);

		if (relative_rank(strong, pawn) <= rank_5)
			return draw_factor;

		if (forward_bb(strong, pawn) & black_king && different_color(black_king, white_bishop))
			return draw_factor;

		if (forward_bb(strong, pawn) & (pos.attack_from<pt_bishop>(black_bishop) | black_bishop)
			&& (weak_side_to_move || !(white_attack & black_bishop)))
			return draw_factor;
	}
	else
	{
		const auto flop = strong == white
			? (pos.pieces(white, pt_bishop) & dark_squares ? 0 : 07)
			: pos.pieces(black, pt_bishop) & ~dark_squares ? 070 : 077;

		const auto pawn = static_cast<square>(pos.piece_square(strong, pt_pawn) ^ flop);
		const auto white_king = static_cast<square>(pos.king(strong) ^ flop);
		const auto black_king = static_cast<square>(pos.king(weak) ^ flop);

		if (black_king > pawn
			&& file_of(pawn) == file_of(black_king)
			&& ~dark_squares & black_king)
			return draw_factor;

		if (square_distance[pawn][black_king] == 1
			&& rank_of(black_king) < rank_of(pawn)
			&& abs(file_of(black_king) - file_of(white_king)) <= weak_side_to_move
			&& !(averbakh_rule & pawn))
			return draw_factor;
	}

	return no_factor;
}

template sfactor endgame_kbpk<black>(const position& pos);

template <side strong>
sfactor endgame_kbpkn(const position& pos)
{
	const auto weak = ~strong;
	const auto pawn_sq = pos.piece_square(strong, pt_pawn);
	const auto strong_bishop_sq = pos.piece_square(strong, pt_bishop);

	if (const auto weak_king_sq = pos.king(weak); file_of(weak_king_sq) == file_of(pawn_sq)
		&& relative_rank(strong, pawn_sq) < relative_rank(strong, weak_king_sq)
		&& (different_color(weak_king_sq, strong_bishop_sq)
		|| relative_rank(strong, weak_king_sq) <= rank_6))
		return draw_factor;

	return no_factor;
}

template <side strong>
sfactor endgame_kbppkb(const position& pos)
{
	const auto weak = ~strong;
	const auto wb_sq = pos.piece_square(strong, pt_bishop);
	const auto bb_sq = pos.piece_square(weak, pt_bishop);

	if (!different_color(wb_sq, bb_sq))
		return no_factor;

	const auto square_k = pos.king(weak);
	const auto psq1 = pos.piece_list(strong, pt_pawn)[0];
	const auto psq2 = pos.piece_list(strong, pt_pawn)[1];
	square block_sq1, block_sq2;

	if (relative_rank(strong, psq1) > relative_rank(strong, psq2))
	{
		block_sq1 = psq1 + pawn_ahead(strong);
		block_sq2 = make_square(file_of(psq2), rank_of(psq1));
	}
	else
	{
		block_sq1 = psq2 + pawn_ahead(strong);
		block_sq2 = make_square(file_of(psq1), rank_of(psq2));
	}

	switch (file_distance(psq1, psq2))
	{
	case 0:
	{
		if (file_of(square_k) == file_of(block_sq1)
			&& relative_rank(strong, square_k) >= relative_rank(strong, block_sq1)
			&& different_color(square_k, wb_sq))
			return draw_factor;
		return no_factor;
	}

	case 1:
	{
		if (square_k == block_sq1
			&& different_color(square_k, wb_sq)
			&& (bb_sq == block_sq2
			|| pos.attack_from<pt_bishop>(block_sq2) & pos.pieces(weak, pt_bishop)
			|| rank_distance(psq1, psq2) >= 2))
			return draw_factor;

		if (square_k == block_sq2
			&& different_color(square_k, wb_sq)
			&& (bb_sq == block_sq1
			|| pos.attack_from<pt_bishop>(block_sq1) & pos.pieces(weak, pt_bishop)))
			return draw_factor;

		return no_factor;
	}

	default:
		return no_factor;
	}
}

template <side strong>
sfactor endgame_knpk(const position& pos)
{
	const auto weak = ~strong;
	const auto white_king = endgame::normalize_pawn_side(pos, strong, pos.king(strong));
	const auto knight = endgame::normalize_pawn_side(pos, strong, pos.piece_square(strong, pt_knight));
	const auto pawn = endgame::normalize_pawn_side(pos, strong, pos.piece_square(strong, pt_pawn));
	const auto black_king = endgame::normalize_pawn_side(pos, strong, pos.king(weak));

	if (pawn == a7
		&& distance(a8, black_king) <= 1)
		return draw_factor;

	if (pawn == a7 && white_king == a8
		&& (black_king == c7
		&& !(pos.on_move() == strong) ^ !(~dark_squares & knight)
		|| black_king == c8 && !(pos.on_move() == strong) ^ !(dark_squares & knight)))
		return draw_factor;

	return no_factor;
}

template <side strong>
sfactor endgame_knpkb(const position& pos)
{
	const auto weak = ~strong;
	const auto pawn_sq = pos.piece_square(strong, pt_pawn);
	const auto bishop_sq = pos.piece_square(weak, pt_bishop);

	if (const auto weak_king_sq = pos.king(weak); forward_bb(strong, pawn_sq) & pos.attack_from<pt_bishop>(bishop_sq))
		return static_cast<sfactor>(2 * distance(weak_king_sq, pawn_sq));

	return no_factor;
}

template <side strong>
sfactor endgame_kpk(const position& pos)
{
	const auto weak = ~strong;
	assert(pos.non_pawn_material(strong) == mat_0);
	assert(pos.number(strong, pt_pawn) >= 2);

	const auto square_k = pos.king(weak);

	if (const auto pawns = pos.pieces(strong, pt_pawn); !(pawns & ~ranks_forward_bb(weak, rank_of(square_k)))
		&& !(pawns & ~file_a_bb
		&& pawns & ~file_h_bb)
		&& file_distance(square_k, lsb(pawns)) <= 1)
		return draw_factor;

	return no_factor;
}

template sfactor endgame_kpk<white>(const position& pos);
template sfactor endgame_kpk<black>(const position& pos);

template <side strong>
sfactor endgame_kpkp(const position& pos)
{
	const auto weak = ~strong;
	const auto wk_sq = endgame::normalize_pawn_side(pos, strong, pos.king(strong));
	const auto bk_sq = endgame::normalize_pawn_side(pos, strong, pos.king(weak));
	const auto p_sq = endgame::normalize_pawn_side(pos, strong, pos.piece_square(strong, pt_pawn));

	const auto us = strong == pos.on_move() ? white : black;

	if (rank_of(p_sq) >= rank_5 && file_of(p_sq) != file_a)
		return no_factor;

	return kpk::probe(wk_sq, p_sq, bk_sq, us) ? no_factor : draw_factor;
}

template sfactor endgame_kpkp<white>(const position& pos);
template sfactor endgame_kpkp<black>(const position& pos);

template <side strong>
sfactor endgame_krkp(const position& pos)
{
	const auto weak = ~strong;
	const auto white_king = relative_square(strong, pos.king(strong));
	const auto black_king = relative_square(strong, pos.king(weak));
	const auto rook = relative_square(strong, pos.piece_square(strong, pt_rook));
	const auto pawn = relative_square(strong, pos.piece_square(weak, pt_pawn));

	const auto promotion = make_square(file_of(pawn), rank_1);

	if (!((pos.attack_from<pt_king>(white_king) | pos.pieces(strong, pt_king)) & ranks_forward_bb(weak, pawn)) &&
		(rank_of(pawn) <= rank_3 || rank_of(black_king) <= rank_of(pawn)))
	{
		const auto bk_distance = static_cast<char16_t>(square_distance[black_king][pawn]);

		if (const bool pawn_attacked = (pos.attack_from<pt_rook>(pos.piece_square(strong, pt_rook))
			| pos.attack_from<pt_king>(pos.king(strong))) & pos.piece_square(weak, pt_pawn); bk_distance <= 1 + (pos.on_move() == weak || !pawn_attacked))
		{
			const auto rook_behind_pawn = file_of(pawn) == file_of(rook);
			const auto promotion_distance = rank_of(pawn) + square_distance[black_king][promotion] - 2 + rook_behind_pawn;
			auto wk_distance = square_distance[white_king][promotion] - (pos.on_move() == strong);

			if ((file_of(white_king) < file_of(pawn)
				&& file_of(black_king) < file_of(pawn)
				|| file_of(white_king) > file_of(pawn)
				&& file_of(black_king) > file_of(pawn))
				&& rank_of(white_king) >= rank_of(black_king)) ++wk_distance;

			if ((file_of(white_king) < file_of(black_king)
				&& file_of(black_king) < file_of(pawn)
				|| file_of(white_king) > file_of(black_king)
				&& file_of(black_king) > file_of(pawn))
				&& rank_of(white_king) >= rank_of(black_king))
				++wk_distance;

			if (wk_distance > promotion_distance)
			{
				if (const auto wk_distance2 = square_distance[white_king][pawn] - (pos.on_move() == strong); wk_distance2 > bk_distance
					|| rank_of(white_king) - (pos.on_move() == strong) > rank_of(pawn))
					return draw_factor;
			}
		}
	}
	return no_factor;
}

template <side strong>
sfactor endgame_krpkb(const position& pos)
{
	const auto weak = ~strong;

	if (pos.pieces(pt_pawn) & (file_a_bb | file_h_bb))
	{
		const auto square_k = pos.king(weak);
		const auto bsq = pos.piece_square(weak, pt_bishop);
		const auto psq = pos.piece_square(strong, pt_pawn);
		const auto rk = relative_rank(strong, psq);
		const auto push = pawn_ahead(strong);

		if (rk == rank_5 && !different_color(bsq, psq))
		{
			if (const auto d = distance(psq + 3 * push, square_k); d <= 2 && !(d == 0 && square_k == pos.king(strong) + 2 * push))
				return static_cast<sfactor>(38);
			return static_cast<sfactor>(75);
		}

		if (rk == rank_6
			&& distance(psq + 2 * push, square_k) <= 1
			&& empty_attack[pt_bishop][bsq] & psq + push
			&& file_distance(bsq, psq) >= 2)
			return static_cast<sfactor>(12);
	}

	return no_factor;
}

template <side strong>
sfactor endgame_krpkr(const position& pos)
{
	const auto weak = ~strong;
	const auto wk_sq = endgame::normalize_pawn_side(pos, strong, pos.king(strong));
	const auto bk_sq = endgame::normalize_pawn_side(pos, strong, pos.king(weak));
	const auto wr_sq = endgame::normalize_pawn_side(pos, strong, pos.piece_square(strong, pt_rook));
	const auto wp_sq = endgame::normalize_pawn_side(pos, strong, pos.piece_square(strong, pt_pawn));
	const auto br_sq = endgame::normalize_pawn_side(pos, strong, pos.piece_square(weak, pt_rook));

	const auto f = file_of(wp_sq);
	const auto r = rank_of(wp_sq);
	const auto queening_sq = make_square(f, rank_8);
	const int tempo = pos.on_move() == strong;

	if (r <= rank_5
		&& distance(bk_sq, queening_sq) <= 1
		&& wk_sq <= h5
		&& (rank_of(br_sq) == rank_6 || r <= rank_3 && rank_of(wr_sq) != rank_6))
		return draw_factor;

	if (r == rank_6
		&& distance(bk_sq, queening_sq) <= 1
		&& rank_of(wk_sq) + tempo <= rank_6
		&& (rank_of(br_sq) == rank_1 || !tempo && file_distance(br_sq, wp_sq) >= 3))
		return draw_factor;

	if (r >= rank_6
		&& bk_sq == queening_sq
		&& rank_of(br_sq) == rank_1
		&& (!tempo || distance(wk_sq, wp_sq) >= 2))
		return draw_factor;

	if (wp_sq == a7
		&& wr_sq == a8
		&& (bk_sq == h7 || bk_sq == g7)
		&& file_of(br_sq) == file_a
		&& (rank_of(br_sq) <= rank_3 || file_of(wk_sq) >= file_d || rank_of(wk_sq) <= rank_5))
		return draw_factor;

	if (r <= rank_5
		&& bk_sq == wp_sq + north
		&& distance(wk_sq, wp_sq) - tempo >= 2
		&& distance(wk_sq, br_sq) - tempo >= 2)
		return draw_factor;

	if (r == rank_7
		&& f != file_a
		&& file_of(wr_sq) == f
		&& wr_sq != queening_sq
		&& distance(wk_sq, queening_sq) < distance(bk_sq, queening_sq) - 2 + tempo
		&& distance(wk_sq, queening_sq) < distance(bk_sq, wr_sq) + tempo)
		return static_cast<sfactor>(max_factor - 3 * distance(wk_sq, queening_sq));

	if (f != file_a
		&& file_of(wr_sq) == f
		&& wr_sq < wp_sq
		&& distance(wk_sq, queening_sq) < distance(bk_sq, queening_sq) - 2 + tempo
		&& distance(wk_sq, wp_sq + north) < distance(bk_sq, wp_sq + north) - 2 + tempo
		&& (distance(bk_sq, wr_sq) + tempo >= 3
			|| distance(wk_sq, queening_sq) < distance(bk_sq, wr_sq) + tempo
			&& distance(wk_sq, wp_sq + north) < distance(bk_sq, wr_sq) + tempo))
		return static_cast<sfactor>(max_factor - 12 * distance(wp_sq, queening_sq) - 3 * distance(wk_sq, queening_sq));

	if (r <= rank_4 && bk_sq > wp_sq)
	{
		if (file_of(bk_sq) == file_of(wp_sq))
			return static_cast<sfactor>(16);

		if (file_distance(bk_sq, wp_sq) == 1
			&& distance(wk_sq, bk_sq) > 2)
			return static_cast<sfactor>(38 - 3 * distance(wk_sq, bk_sq));
	}
	return no_factor;
}

template <side strong>
sfactor endgame_krppkrp(const position& pos)
{
	const auto weak = ~strong;
	const auto wp_sq1 = pos.piece_list(strong, pt_pawn)[0];
	const auto wp_sq2 = pos.piece_list(strong, pt_pawn)[1];
	const auto bk_sq = pos.king(weak);

	if (pos.is_passed_pawn(strong, wp_sq1) || pos.is_passed_pawn(strong, wp_sq2))
		return no_factor;

	if (const auto r = std::max(relative_rank(strong, wp_sq1), relative_rank(strong, wp_sq2)); file_distance(bk_sq, wp_sq1) <= 1
		&& file_distance(bk_sq, wp_sq2) <= 1
		&& relative_rank(strong, bk_sq) > r)
	{
		assert(r > rank_1 && r < rank_7);
		return static_cast<sfactor>(endgame::krppkrp_scale_factors[r]);
	}
	return no_factor;
}

template <side strong>
sfactor endgame_kqkp(const position& pos)
{
	const auto weak = ~strong;

	if (!(pos.pieces(weak, pt_pawn) & (strong == white ? bb4(a2, c2, f2, h2) : bb4(a7, c7, f7, h7))))
		return no_factor;

	auto white_king = relative_square(strong, pos.king(strong));
	auto black_king = relative_square(strong, pos.king(weak));
	auto dame = relative_square(strong, pos.piece_square(strong, pt_queen));
	auto pawn = relative_square(strong, pos.piece_square(weak, pt_pawn));

	const auto attack = pos.attack_from<pt_queen>(pos.piece_square(strong, pt_queen)) | pos.attack_from<pt_king>(pos.king(strong));
	const auto white_on_move = pos.on_move() == strong;
	const auto bk_check = pos.is_in_check();

	auto flop = strong == white ? 0 : 070;

	if (const auto tempo = !white_on_move || !(attack & static_cast<square>(pawn ^ flop)); dame != pawn - 8
		&& rank_of(black_king) <= rank_2 + tempo
		&& (square_distance[black_king][pawn] <= 1 + tempo || pawn == c2
		&& black_king == a1 || pawn == f2 && black_king == h1))
	{
		if (file_of(pawn) >= file_e)
		{
			flop ^= 07;
			pawn = static_cast<square>(pawn ^ 07);
			white_king = static_cast<square>(white_king ^ 07);
			black_king = static_cast<square>(black_king ^ 07);
			dame = static_cast<square>(dame ^ 07);
		}

		if (pawn == a2
			&& (file_of(white_king) > file_e || white_king > d5))
			return draw_factor;

		if (pawn == c2
			&& file_of(black_king) < file_c
			&& (file_of(white_king) > file_e || white_king > c4))
			return draw_factor;

		if (pawn == c2
			&& file_of(black_king) >= file_c
			&& (file_of(white_king) > file_g || white_king > d5))
			return draw_factor;

		if (!white_on_move
			&& !bk_check)
		{
			if (pawn == a2
				&& (square_distance[a1][black_king] == 1
				|| file_of(dame) != file_a
				&& !(attack & static_cast<square>(a1 ^ flop))))
				return draw_factor;

			if (pawn == c2
				&& (square_distance[c1][black_king] == 1
				|| file_of(dame) != file_c
				&& !(attack & static_cast<square>(c1 ^ flop))))
				return draw_factor;
		}
	}
	return no_factor;
}

template <side strong>
sfactor endgame_kqkrp(const position& pos)
{
	const auto weak = ~strong;
	assert(pos.number(weak, pt_rook) == 1);
	assert(pos.number(weak, pt_pawn) >= 1);

	const auto king_sq = pos.king(weak);

	if (const auto rsq = pos.piece_square(weak, pt_rook); relative_rank(weak, king_sq) <= rank_2
		&& relative_rank(weak, pos.king(strong)) >= rank_4
		&& relative_rank(weak, rsq) == rank_3
		&& pos.pieces(weak, pt_pawn)
		& ~file_a_bb & ~file_h_bb
		& pos.attack_from<pt_king>(king_sq)
		& pos.attack_from<pt_pawn>(rsq, strong))
		return draw_factor;

	return one_pawn_factor;
}

template sfactor endgame_kqkrp<white>(const position& pos);
template sfactor endgame_kqkrp<black>(const position& pos);

void endgames::init_scale_factors()
{
	if (!map_value_.empty())
		return;

	factor_number_ = 0;

	factor_functions[factor_number_++] = &endgame_kbpk<white>;
	factor_functions[factor_number_++] = &endgame_kbpk<black>;
	factor_functions[factor_number_++] = &endgame_kpk<white>;
	factor_functions[factor_number_++] = &endgame_kpk<black>;
	factor_functions[factor_number_++] = &endgame_kpkp<white>;
	factor_functions[factor_number_++] = &endgame_kpkp<black>;
	factor_functions[factor_number_++] = &endgame_kqkrp<white>;
	factor_functions[factor_number_++] = &endgame_kqkrp<black>;

	add_scale_factor("0110100 0100100", &endgame_kbpkb<white>, &endgame_kbpkb<black>);
	add_scale_factor("0110100 0101000", &endgame_kbpkn<white>, &endgame_kbpkn<black>);
	add_scale_factor("0120100 0100100", &endgame_kbppkb<white>, &endgame_kbppkb<black>);
	add_scale_factor("0111000 0100000", &endgame_knpk<white>, &endgame_knpk<black>);
	add_scale_factor("0111000 0100100", &endgame_knpkb<white>, &endgame_knpkb<black>);
	add_scale_factor("0100001 0110000", &endgame_kqkp<white>, &endgame_kqkp<black>);
	add_scale_factor("0100010 0110000", &endgame_krkp<white>, &endgame_krkp<black>);
	add_scale_factor("0110010 0100100", &endgame_krpkb<white>, &endgame_krpkb<black>);
	add_scale_factor("0110010 0100010", &endgame_krpkr<white>, &endgame_krpkr<black>);
	add_scale_factor("0120010 0110010", &endgame_krppkrp<white>, &endgame_krppkrp<black>);
}

