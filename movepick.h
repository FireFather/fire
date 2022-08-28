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

#include <array>
#include <limits>

#include "fire.h"
#include "movegen.h"
#include "position.h"

namespace movepick
{
	constexpr int piece_order[num_pieces] =
	{
		0, 6, 1, 2, 3, 4, 5, 0,
		0, 6, 1, 2, 3, 4, 5, 0
	};

	constexpr int capture_sort_values[num_pieces] =
	{
		0, 0, 198, 817, 836, 1270, 2521, 0,
		0, 0, 198, 817, 836, 1270, 2521, 0
	};

	void init_search(const position&, uint32_t, int, bool);
	void init_q_search(const position&, uint32_t, int, square);
	void init_prob_cut(const position&, uint32_t, int);

	uint32_t pick_move(const position& pos);

	template <move_gen>
	void score(const position& pos);
}

template <typename tn>
struct piece_square_table
{
	const tn* operator[](ptype piece) const
	{
		return table_[piece];
	}

	tn* operator[](ptype piece)
	{
		return table_[piece];
	}

	void clear()
	{
		std::memset(table_, 0, sizeof(table_));
	}

	tn get(ptype piece, square to)
	{
		return table_[piece][to];
	}

	void update(ptype piece, square to, tn val)
	{
		table_[piece][to] = val;
	}

protected:
	tn table_[num_pieces][num_squares];
};

template <int max_plus, int max_min>
struct piece_square_stats : piece_square_table<int16_t>
{
	static int calculate_offset(const ptype piece, const square to)
	{
		return 64 * static_cast<int>(piece) + static_cast<int>(to);
	}

	[[nodiscard]] int16_t value_at_offset(const int offset) const
	{
		return *(reinterpret_cast<const int16_t*>(table_) + offset);
	}

	void fill(const int val)
	{
		const auto vv = static_cast<uint16_t>(val) << 16 | static_cast<uint16_t>(val);
		std::fill(reinterpret_cast<int*>(table_), reinterpret_cast<int*>(reinterpret_cast<char*>(table_) + sizeof table_), vv);
	}

	void update_plus(const int offset, const int val)
	{
		auto& elem = *(reinterpret_cast<int16_t*>(table_) + offset);
		elem -= elem * static_cast<int16_t>(val) / max_plus;
		elem += static_cast<int16_t>(val);
	}

	void update_minus(const int offset, const int val)
	{
		auto& elem = *(reinterpret_cast<int16_t*>(table_) + offset);
		elem -= elem * static_cast<int16_t>(val) / max_min;
		elem -= static_cast<int16_t>(val);
	}
};

template <typename T>
struct color_square_stats
{
	const T* operator[](side color) const
	{
		return table_[color];
	}

	T* operator[](side color)
	{
		return table_[color];
	}

	void clear()
	{
		std::memset(table_, 0, sizeof(table_));
	}

private:
	T table_[num_sides][num_squares];
};

struct counter_move_full_stats
{
	[[nodiscard]] uint32_t get(const side color, const uint32_t move) const
	{
		return table_[color][move & 0xfff];
	}

	void clear()
	{
		std::memset(table_, 0, sizeof table_);
	}

	void update(const side color, const uint32_t move1, const uint32_t move)
	{
		table_[color][move1 & 0xfff] = static_cast<uint16_t>(move);
	}

private:
	uint16_t table_[num_sides][64 * 64] = {};
};

struct counter_follow_up_move_stats
{
	[[nodiscard]] uint32_t get(const ptype piece1, const square to1, const ptype piece2, const square to2) const
	{
		return table_[piece1][to1][piece_type(piece2)][to2];
	}

	void clear()
	{
		std::memset(table_, 0, sizeof table_);
	}

	void update(const ptype piece1, const square to1, const ptype piece2, const square to2, const uint32_t move)
	{
		table_[piece1][to1][piece_type(piece2)][to2] = static_cast<uint16_t>(move);
	}

private:
	uint16_t table_[num_pieces][num_squares][num_piecetypes][num_squares] = {};
};

struct max_gain_stats
{
	[[nodiscard]] int get(const ptype piece, const uint32_t move) const
	{
		return table_[piece][move & 0x0fff];
	}

	void clear()
	{
		std::memset(table_, 0, sizeof table_);
	}

	void update(const ptype piece, const uint32_t move, const int gain)
	{
		auto* const p_gain = &table_[piece][move & 0x0fff];
		*p_gain += (gain - *p_gain + 8) >> 4;
	}

private:
	int table_[num_pieces][64 * 64] = {};
};

struct killer_stats
{
	static int index_my_pieces(const position& pos, side color);
	static int index_your_pieces(const position& pos, side color, square to);

	[[nodiscard]] uint32_t get(const side color, const int index) const
	{
		return table_[color][index];
	}

	void clear()
	{
		std::memset(table_, 0, sizeof table_);
	}

	void update(const side color, const int index, const uint32_t move)
	{
		table_[color][index] = move;
	}

private:
	uint32_t table_[num_sides][65536] = {};
};

typedef piece_square_table<uint16_t> counter_move_stats;
typedef piece_square_stats<8192, 8192> move_value_stats;
typedef piece_square_stats<3 * 8192, 3 * 8192> counter_move_values;
typedef piece_square_table<counter_move_values> counter_move_history;

inline stage& operator++(stage& d)
{
	return d = static_cast<stage>(static_cast<int>(d) + 1);
}

template<typename t, int d>
class pi_entry {

	t entry;

public:
	void operator=(const t& v)
	{
		entry = v;
	}
	t* operator&()
	{
		return &entry;
	}
	t* operator->()
	{
		return &entry;
	}
	explicit operator const t& () const
	{
		return entry;
	}

	void operator<<(int bonus)
	{
		assert(abs(bonus) <= D);
		static_assert(d <= std::numeric_limits<t>::max(), "D overflows T");

		entry += bonus - entry * abs(bonus) / d;

		assert(abs(entry) <= D);
	}
};

template <typename t, int d, int Size, int... sizes>
struct pos_info : std::array<pos_info<t, d, sizes...>, Size>
{
	typedef pos_info<t, d, Size, sizes...> stats;

	void fill(const t& v)
	{
		assert(std::is_standard_layout<stats>::value);

		typedef pi_entry<t, d> entry;
		entry* p = reinterpret_cast<entry*>(this);
		std::fill(p, p + sizeof(*this) / sizeof(entry), v);
	}
};

template <typename t, int d, int Size>
struct pos_info<t, d, Size> : std::array<pi_entry<t, d>, Size> {};
typedef pos_info<int16_t, 29952, 16, 64> piece_to_history;
typedef pos_info<piece_to_history, 0, 16, 64> continuation_history;


