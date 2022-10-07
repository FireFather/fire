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
#include <atomic>
#include <vector>

#include "chrono.h"
#include "fire.h"
#include "movepick.h"
#include "position.h"

typedef movelist<max_pv> principal_variation;

struct search_signals
{
	std::atomic_bool stop_analyzing, stop_if_ponder_hit;
};

namespace search
{
	inline search_signals signals;
	inline search_param param;
	inline bool running;

	void init();
	void reset();
	void adjust_time_after_ponder_hit();
	extern uint8_t lm_reductions[2][2][64 * static_cast<int>(plies)][64];

	enum nodetype
	{
		PV, nonPV
	};

	inline int counter_move_bonus[max_ply];

	inline int counter_move_value(const int d)
	{
		return counter_move_bonus[static_cast<uint32_t>(d) / plies];
	}

	inline int history_bonus(const int d)
	{
		return counter_move_bonus[static_cast<uint32_t>(d) / plies];
	}

	struct easy_move_manager
	{
		void clear()
		{
			key_after_two_moves = 0;
			third_move_stable = 0;
			pv[0] = pv[1] = pv[2] = no_move;
		}

		[[nodiscard]] uint32_t expected_move(const uint64_t key) const
		{
			return key_after_two_moves == key ? pv[2] : no_move;
		}

		void refresh_pv(position& pos, const principal_variation& pv_new)
		{
			assert(pv_new.size() >= 3);

			third_move_stable = pv_new[2] == pv[2] ? third_move_stable + 1 : 0;

			if (pv_new[0] != pv[0] || pv_new[1] != pv[1] || pv_new[2] != pv[2])
			{
				pv[0] = pv_new[0];
				pv[1] = pv_new[1];
				pv[2] = pv_new[2];

				pos.play_move(pv_new[0]);
				pos.play_move(pv_new[1]);
				key_after_two_moves = pos.key();
				pos.take_move_back(pv_new[1]);
				pos.take_move_back(pv_new[0]);
			}
		}

		int third_move_stable;
		uint64_t key_after_two_moves;
		uint32_t pv[3];
	};

	inline easy_move_manager easy_move;
	inline int draw[num_sides];
	inline uint64_t previous_info_time;

	template <nodetype nt>
	int alpha_beta(position& pos, int alpha, int beta, int depth, bool cut_node);

	template <nodetype nt, bool state_check>
	int q_search(position& pos, int alpha, int beta, int depth);

	int value_to_hash(int val, int ply);
	int value_from_hash(int val, int ply);
	void copy_pv(uint32_t* pv, uint32_t move, uint32_t* pv_lower);
	void update_stats(const position& pos, bool state_check, uint32_t move, int depth, const uint32_t* quiet_moves, int quiet_number);
	void update_stats_quiet(const position& pos, bool state_check, int depth, const uint32_t* quiet_moves, int quiet_number);
	void update_stats_minus(const position& pos, bool state_check, uint32_t move, int depth);
	void send_time_info();

	inline uint8_t lm_reductions[2][2][64 * static_cast<int>(plies)][64];

	inline constexpr int razor_margin = 384;

	// futility pruning values
	inline constexpr auto futility_value_0 = 0;
	inline constexpr auto futility_value_1 = 112;
	inline constexpr auto futility_value_2 = 243;
	inline constexpr auto futility_value_3 = 376;
	inline constexpr auto futility_value_4 = 510;
	inline constexpr auto futility_value_5 = 646;
	inline constexpr auto futility_value_6 = 784;
	inline constexpr auto futility_margin_ext_mult = 160;
	inline constexpr auto futility_margin_ext_base = 204;

	inline constexpr int futility_values[7] =
	{
		static_cast<int>(futility_value_0), static_cast<int>(futility_value_1), static_cast<int>(futility_value_2),
		static_cast<int>(futility_value_3), static_cast<int>(futility_value_4), static_cast<int>(futility_value_5),
		static_cast<int>(futility_value_6)
	};

	inline int futility_margin(const int d)
	{
		return futility_values[static_cast<uint32_t>(d) / plies];
	}

	inline int futility_margin_ext(const int d)
	{
		return futility_margin_ext_base + futility_margin_ext_mult * static_cast<int>(static_cast<uint32_t>(d) / plies);
	}

	inline constexpr int late_move_number_values[2][32] =
	{
		{0, 0, 3, 3, 4, 5, 6, 7, 8, 10, 12, 15, 17, 20, 23, 26, 30, 33, 37, 40, 44, 49, 53, 58, 63, 68, 73, 78, 83, 88, 94, 100},
		{0, 0, 5, 5, 6, 7, 9, 11, 14, 17, 20, 23, 27, 31, 35, 40, 45, 50, 55, 60, 65, 71, 77, 84, 91, 98, 105, 112, 119, 127, 135, 143}
	};

	inline int late_move_number(const int d, const bool progress)
	{
		return late_move_number_values[progress][static_cast<uint32_t>(d) / (plies / 2)];
	}

	inline int lmr_reduction(const bool pv, const bool vg, const int d, const int n)
	{
		return lm_reductions[pv][vg][std::min(d, 64 * static_cast<int>(plies) - 1)][std::min(n, 63)];
	}

	struct rootmove_mc {

		explicit rootmove_mc(const uint32_t m) : pv(1, m) {}
		bool operator==(const uint32_t& m) const
		{
			return pv[0] == m;
		}
		bool operator<(const rootmove_mc& m) const
		{
			return m.score != score ? m.score < score
				: m.previous_score < previous_score;
		}

		int score = -max_score;
		int previous_score = -max_score;
		int sel_depth = 0;
		int tb_rank = 0;
		int best_move_count = 0;
		int tb_score{};
		std::vector<uint32_t> pv;
	};

	typedef std::vector<rootmove_mc> rootmoves_mc;

	struct stack_mc
	{
		uint32_t* pv;
		piece_to_history* continuation_history;
		int ply;
		uint32_t current_move;
		uint32_t excluded_move;
		uint32_t killers[2];
		int static_eval;
		int stat_score;
		int move_count;
	};
}

template <int max_plus, int max_min>
struct piece_square_stats;
typedef piece_square_stats<24576, 24576> counter_move_values;

inline constexpr int egtb_helpful = 0 * plies;
inline constexpr int egtb_not_helpful = 10 * plies;

inline int tb_number;
inline bool tb_root_in_tb;
inline int tb_probe_depth;
inline int tb_score;

typedef int (*egtb_probe)(position& pos);
void filter_root_moves(position& pos);
std::string score_cp(int score);

struct rootmove
{
	rootmove() = default;

	explicit rootmove(const uint32_t move)
	{
		pv.move_number = 1;
		pv.moves[0] = move;
	}

	bool operator<(const rootmove& root_move) const
	{
		return root_move.score < score;
	}

	bool operator==(const uint32_t& move) const
	{
		return pv[0] == move;
	}

	bool ponder_move_from_hash(position& pos);
	void pv_from_hash(position& pos);

	int depth = depth_0;
	int score = -max_score;
	int previous_score = -max_score;
	int start_value = score_0;
	principal_variation pv;
};

struct rootmoves
{
	rootmoves() = default;

	int move_number = 0, dummy = 0;
	rootmove moves[max_moves];

	void add(const rootmove root_move)
	{
		moves[move_number++] = root_move;
	}

	rootmove& operator[](const int index)
	{
		return moves[index];
	}

	const rootmove& operator[](const int index) const
	{
		return moves[index];
	}

	void clear()
	{
		move_number = 0;
	}

	int find(const uint32_t move)
	{
		for (auto i = 0; i < move_number; i++)
			if (moves[i].pv[0] == move)
				return i;
		return -1;
	}
};

namespace egtb
{
	extern int max_pieces_wdl, max_pieces_dtz, max_pieces_dtm;
	extern egtb_probe egtb_probe_wdl;
	extern egtb_probe egtb_probe_dtz;
	extern egtb_probe egtb_probe_dtm;
	extern bool use_rule50;
	extern void syzygy_init(const std::string& path);
}