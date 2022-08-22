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

#include "movepick.h"

#include "fire.h"
#include "pragma.h"
#include "thread.h"

// move ordering for main search, quiescent search, and prob cut search
// assigns a value to each move in the list, then sorts the list via insertion sort
namespace movepick
{
	void init_search(const position& pos, const uint32_t hash_move, const int depth, const bool only_quiet_check_moves)
	{
		assert(depth >= plies);
		auto* pi = pos.info();
		pi->mp_depth = depth;
		pi->mp_only_quiet_check_moves = only_quiet_check_moves;
		pi->mp_hash_move = hash_move && pos.valid_move(hash_move) ? hash_move : no_move;

		if (pos.is_in_check())
			pi->mp_stage = pi->mp_hash_move ? check_evasions : gen_check_evasions;
		else
		{
			pi->mp_stage = pi->mp_hash_move ? normal_search : gen_good_captures;
			if (pi->move_counter_values)
			{
				pi->mp_counter_move = static_cast<uint32_t>(pos.thread_info()->counter_moves.get(pi->moved_piece, to_square(pi->previous_move)));
				if (!pi->mp_hash_move && (pi - 1)->move_counter_values
					&& (!pi->mp_counter_move || !pos.valid_move(pi->mp_counter_move) || pos.capture_or_promotion(pi->mp_counter_move)))
				{
					pi->mp_counter_move = pos.thread_info()->counter_followup_moves.get((pi - 1)->moved_piece, to_square((pi - 1)->previous_move),
						pi->moved_piece, to_square(pi->previous_move));
				}
			}
			else
				pi->mp_counter_move = no_move;
		}
	}

	void init_q_search(const position& pos, const uint32_t hash_move, const int depth, const square sq)
	{
		assert(depth <= depth_0);
		auto* pi = pos.info();

		if (pos.is_in_check())
			pi->mp_stage = check_evasions;

		else if (depth == depth_0)
			pi->mp_stage = q_search_with_checks;

		else if (depth >= -4 * plies)
			pi->mp_stage = q_search_no_checks;

		else
		{
			pi->mp_stage = gen_recaptures;
			pi->mp_capture_square = sq;
			return;
		}

		pi->mp_hash_move = hash_move && pos.valid_move(hash_move) ? hash_move : no_move;
		if (!pi->mp_hash_move)
			++pi->mp_stage;
	}

	void init_prob_cut(const position& pos, const uint32_t hash_move, const int limit)
	{
		auto* pi = pos.info();
		pi->mp_threshold = limit + 1;

		pi->mp_hash_move = hash_move
			&& pos.valid_move(hash_move)
			&& pos.capture_or_promotion(hash_move)
			&& pos.see_test(hash_move, pi->mp_threshold)
			? hash_move
			: no_move;

		pi->mp_stage = pi->mp_hash_move ? probcut : gen_probcut;
	}

	template <>
	void score<captures_promotions>(const position& pos)
	{
		const auto* const pi = pos.info();
		for (auto* z = pi->mp_current_move; z < pi->mp_end_list; z++)
			z->value = capture_sort_values[pos.piece_on_square(to_square(z->move))]
			- 200 * relative_rank(pos.on_move(), to_square(z->move));
	}

	template <>
	void score<quiet_moves>(const position& pos)
	{
		const auto& history = pos.thread_info()->history;

		const auto* pi = pos.info();
		const counter_move_values* cm = pi->move_counter_values ? pi->move_counter_values : &pos.cmh_info()->counter_move_stats[no_piece][a1];
		const counter_move_values* fm = (pi - 1)->move_counter_values
			? (pi - 1)->move_counter_values
			: &pos.cmh_info()->counter_move_stats[no_piece][a1];
		const counter_move_values* f2 = (pi - 3)->move_counter_values
			? (pi - 3)->move_counter_values
			: &pos.cmh_info()->counter_move_stats[no_piece][a1];

		const auto threat = pi->mp_depth < 6 * plies ? pos.calculate_threat() : no_square;

		for (auto* z = pi->mp_current_move; z < pi->mp_end_list; z++)
		{
			const auto offset = move_value_stats::calculate_offset(pos.moved_piece(z->move), to_square(z->move));
			z->value = static_cast<int>(history.value_at_offset(offset))
				+ static_cast<int>(cm->value_at_offset(offset))
				+ static_cast<int>(fm->value_at_offset(offset))
				+ static_cast<int>(f2->value_at_offset(offset));
			z->value += 8 * pos.thread_info()->max_gain_table.get(pos.moved_piece(z->move), z->move);

			if (from_square(z->move) == threat)
				z->value += 9000 - 1000 * (pi->mp_depth / plies);
		}
	}

	template <>
	void score<evade_check>(const position& pos)
	{
		const auto* const pi = pos.info();
		const auto& history = pos.thread_info()->evasion_history;

		for (auto* z = pi->mp_current_move; z < pi->mp_end_list; z++)
		{
			if (pos.is_capture_move(z->move))
				z->value = capture_sort_values[pos.piece_on_square(to_square(z->move))]
				- piece_order[pos.moved_piece(z->move)] + sort_max;
			else
			{
				const auto offset = move_value_stats::calculate_offset(pos.moved_piece(z->move), to_square(z->move));
				z->value = static_cast<int>(history.value_at_offset(offset));
			}
		}
	}

	inline void insertion_sort(s_move* begin, const s_move* end)
	{
		s_move* q = nullptr;

		for (auto* p = begin + 1; p < end; ++p)
		{
			auto tmp = *p;
			for (q = p; q != begin && *(q - 1) < tmp; --q)
				*q = *(q - 1);
			*q = tmp;
		}
	}

	inline s_move* partition(s_move* begin, s_move* end, const int val)
	{
		while (true)
		{
			while (true)
			{
				if (begin == end)
					return begin;
				if (begin->value > val)
					begin++;
				else
					break;
			}
			end--;
			while (true)
			{
				if (begin == end)
					return begin;
				if (!(end->value > val))
					end--;
				else
					break;
			}
			const auto tmp = *begin;
			*begin = *end;
			*end = tmp;
			begin++;
		}
	}

	inline uint32_t find_best_move(s_move* begin, const s_move* end)
	{
		auto* best = begin;
		for (auto* z = begin + 1; z < end; z++)
			if (z->value > best->value)
				best = z;
		const auto move = best->move;
		*best = *begin;
		return move;
	}

	uint16_t crc16(const uint8_t* data_p, uint8_t length)
	{
		uint16_t crc = 0xFFFF;

		while (length--)
		{
			uint8_t x = crc >> 8 ^ *data_p++;
			x ^= x >> 4;
			crc = crc << 8 ^ static_cast<uint16_t>(x << 12) ^ static_cast<uint16_t>(x << 5) ^ static_cast<uint16_t>(x);
		}
		return crc;
	}

	int hash_bb(const uint64_t bb)
	{
		const auto crc = crc16(reinterpret_cast<const uint8_t*>(&bb), 8);
		return crc;
	}

	uint32_t pick_move(const position& pos)
	{
		switch (auto* pi = pos.info(); pi->mp_stage)
		{
		case normal_search: case check_evasions:
		case q_search_with_checks: case q_search_no_checks:
		case probcut:
			pi->mp_end_list = (pi - 1)->mp_end_list;
			++pi->mp_stage;
			return pi->mp_hash_move;

		case gen_good_captures:
			pi->mp_current_move = (pi - 1)->mp_end_list;
			pi->mp_end_bad_capture = pi->mp_current_move;
			pi->mp_delayed_number = 0;
			pi->mp_end_list = generate_moves<captures_promotions>(pos, pi->mp_current_move);
			score<captures_promotions>(pos);
			pi->mp_stage = good_captures;

		case good_captures:
			while (pi->mp_current_move < pi->mp_end_list)
			{
				if (const auto move = find_best_move(pi->mp_current_move++, pi->mp_end_list); move != pi->mp_hash_move)
				{
					if (pos.see_test(move, see_0))
						return move;

					*pi->mp_end_bad_capture++ = move;
				}
			}

			pi->mp_stage = killers_1;
			{
				if (const auto move = pi->killers[0]; move && move != pi->mp_hash_move && pos.valid_move(move) && !pos.capture_or_promotion(move))
					return move;
			}

		case killers_1:
			pi->mp_stage = killers_2;
			{
				if (const auto move = pi->killers[1]; move && move != pi->mp_hash_move && pos.valid_move(move) && !pos.capture_or_promotion(move))
					return move;
			}

		case killers_2:
			pi->mp_stage = gen_bxp_captures;
			{
				if (const auto move = pi->mp_counter_move; move && move != pi->mp_hash_move && move != pi->killers[0]
					&& move != pi->killers[1]
					&& pos.valid_move(move) && !pos.capture_or_promotion(move))
					return move;
			}

		case gen_bxp_captures:
			pi->mp_current_move = (pi - 1)->mp_end_list;
			pi->mp_stage = bxp_captures;

		case bxp_captures:
			while (pi->mp_current_move < pi->mp_end_bad_capture)
			{
				if (const uint32_t move = *pi->mp_current_move++; piece_type(pos.piece_on_square(to_square(move))) == pt_knight
					&& piece_type(pos.piece_on_square(from_square(move))) == pt_bishop)
				{
					*(pi->mp_current_move - 1) = no_move;
					return move;
				}
			}

			pi->mp_current_move = pi->mp_end_bad_capture;
			if (pi->mp_only_quiet_check_moves && pi->move_number >= 1)
			{
				auto* z = pi->mp_current_move;
				z = generate_moves<quiet_checks>(pos, z);
				z = generate_moves<pawn_advances>(pos, z);
				pi->mp_end_list = z;
				score<quiet_moves>(pos);
				insertion_sort(pi->mp_current_move, pi->mp_end_list);
			}
			else
			{
				pi->mp_end_list = generate_moves<quiet_moves>(pos, pi->mp_current_move);
				score<quiet_moves>(pos);

				const auto* sort_tot = pi->mp_end_list;
				if (pi->mp_depth < 6 * plies)
					sort_tot = partition(pi->mp_current_move, pi->mp_end_list, 6000 - 6000 * (pi->mp_depth / plies));
				insertion_sort(pi->mp_current_move, sort_tot);
			}
			pi->mp_stage = quietmoves;

		case quietmoves:
			while (pi->mp_current_move < pi->mp_end_list)
			{
				if (const uint32_t move = *pi->mp_current_move++; move != pi->mp_hash_move
					&& move != pi->killers[0]
					&& move != pi->killers[1]
					&& move != pi->mp_counter_move)
				{
					return move;
				}
			}

			pi->mp_current_move = (pi - 1)->mp_end_list;
			pi->mp_end_list = pi->mp_end_bad_capture;
			pi->mp_stage = bad_captures;

		case bad_captures:
			while (pi->mp_current_move < pi->mp_end_list)
			{
				if (const uint32_t move = *pi->mp_current_move++; move)
					return move;
			}
			if (!pi->mp_delayed_number)
				return no_move;

			pi->mp_stage = delayed_moves;
			pi->mp_delayed_current = 0;

		case delayed_moves:
			if (pi->mp_delayed_current != pi->mp_delayed_number)
				return pi->mp_delayed[pi->mp_delayed_current++];
			return no_move;

		case gen_check_evasions:
			pi->mp_current_move = (pi - 1)->mp_end_list;
			pi->mp_end_list = generate_moves<evade_check>(pos, pi->mp_current_move);
			score<evade_check>(pos);
			pi->mp_stage = check_evasion_loop;

		case check_evasion_loop:
			while (pi->mp_current_move < pi->mp_end_list)
			{
				if (const auto move = find_best_move(pi->mp_current_move++, pi->mp_end_list); move != pi->mp_hash_move)
					return move;
			}
			return no_move;

		case q_search_1: case q_search_2:
			pi->mp_current_move = (pi - 1)->mp_end_list;
			pi->mp_end_list = generate_moves<captures_promotions>(pos, pi->mp_current_move);
			score<captures_promotions>(pos);
			++pi->mp_stage;

		case q_search_captures_1: case q_search_captures_2:
			while (pi->mp_current_move < pi->mp_end_list)
			{
				if (const auto move = find_best_move(pi->mp_current_move++, pi->mp_end_list); move != pi->mp_hash_move)
					return move;
			}

			if (pi->mp_stage == q_search_captures_2)
				return no_move;

			pi->mp_current_move = (pi - 1)->mp_end_list;
			pi->mp_end_list = generate_moves<quiet_checks>(pos, pi->mp_current_move);
			++pi->mp_stage;

		case q_search_check_moves:
			while (pi->mp_current_move < pi->mp_end_list)
			{
				if (const uint32_t move = *pi->mp_current_move++; move != pi->mp_hash_move)
					return move;
			}
			return no_move;

		case gen_probcut:
			pi->mp_current_move = (pi - 1)->mp_end_list;
			pi->mp_end_list = generate_moves<captures_promotions>(pos, pi->mp_current_move);
			score<captures_promotions>(pos);
			pi->mp_stage = probcut_captures;

		case probcut_captures:
			while (pi->mp_current_move < pi->mp_end_list)
			{
				if (const auto move = find_best_move(pi->mp_current_move++, pi->mp_end_list); move != pi->mp_hash_move && pos.see_test(move, pi->mp_threshold))
					return move;
			}
			return no_move;

		case gen_recaptures:
			pi->mp_current_move = (pi - 1)->mp_end_list;
			pi->mp_end_list = generate_captures_on_square(pos, pi->mp_current_move, pi->mp_capture_square);
			score<captures_promotions>(pos);
			pi->mp_stage = recapture_moves;

		case recapture_moves:
			while (pi->mp_current_move < pi->mp_end_list)
			{
				if (const auto move = find_best_move(pi->mp_current_move++, pi->mp_end_list); to_square(move) == pi->mp_capture_square)
					return move;
			}
			return no_move;

		default:
			assert(false);
			return no_move;
		}
	}
}

int killer_stats::index_my_pieces(const position& pos, const side color)
{
	return movepick::hash_bb(pos.pieces(color));
}

int killer_stats::index_your_pieces(const position& pos, const side color, const square to)
{
	return movepick::hash_bb(pos.pieces(color, piece_type(pos.piece_on_square(to))));
}
