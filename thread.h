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
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "fire.h"

#include "endgame.h"
#include "material.h"
#include "movepick.h"
#include "mutex.h"
#include "pawn.h"
#include "position.h"
#include "search.h"

class thread
{
	std::thread native_thread_;
	Mutex mutex_;
	ConditionVariable sleep_condition_;
	bool exit_, search_active_;
	int thread_index_;

public:
	thread();
	virtual ~thread();
	virtual void begin_search();
	void idle_loop();
	void wake(bool activate_search);
	void wait_for_search_to_end();
	void wait(const std::atomic_bool& condition);

	threadinfo* ti{};
	cmhinfo* cmhi{};
	position* root_position{};
	search::rootmoves_mc mc_rootmoves;
	rootmoves root_moves;
	int completed_depth = no_depth;
	int active_pv{};
	continuation_history cont_history{};
	void clear();
};

struct cmhinfo
{
	counter_move_history counter_move_stats;
};

struct threadinfo
{
	position root_position{};
	position_info position_inf[1024]{};
	s_move move_list[8192]{};
	move_value_stats history{};
	move_value_stats evasion_history{};
	max_gain_stats max_gain_table;
	counter_move_stats counter_moves{};
	counter_follow_up_move_stats counter_followup_moves;
	move_value_stats capture_history{};
	material::material_hash material_table{};
	pawn::pawn_hash pawn_table{};
};

struct mainthread final : thread
{
	void begin_search() override;
	void check_time();
	bool quick_move_allow = false;
	bool quick_move_played = false;
	bool quick_move_evaluation_busy = false;
	bool quick_move_evaluation_stopped = false;
	bool failed_low = false;
	int best_move_changed = 0;
	int previous_root_score = score_0;
	int interrupt_counter = 0;
	int previous_root_depth = {};
	int calls_cnt{};
};

struct threadpool : std::vector<thread*>
{
	void init();
	void exit();

	int thread_count{};
	time_point start{};
	int total_analyze_time{};
	thread* threads[max_threads]{};

	[[nodiscard]] mainthread* main() const
	{
		return static_cast<mainthread*>(threads[0]);
	}
	void begin_search(position&, const search_param&);
	void change_thread_count(int num_threads);
	[[nodiscard]] uint64_t visited_nodes() const;
	[[nodiscard]] uint64_t tb_hits() const;
	static void delete_counter_move_history();

	int active_thread_count{};
	side contempt_color = num_sides;
	int piece_contempt{};
	int root_contempt_value = score_0;
	endgames end_games;
	position* root_position{};
	rootmoves root_moves;
	position_info* root_position_info{};
	bool analysis_mode{};
	int fifty_move_distance{};
	int multi_pv{}, multi_pv_max{};
	bool dummy_null_move_threat{}, dummy_prob_cut{};
};

class spinlock {
	std::atomic_int lock_;

public:
	spinlock() : lock_(1) {
	}
	spinlock(const spinlock&) : lock_(1) {
	}

	void acquire() {
		while (lock_.fetch_sub(1, std::memory_order_acquire) != 1)
			while (lock_.load(std::memory_order_relaxed) <= 0)
				std::this_thread::yield();
	}
	void release() { lock_.store(1, std::memory_order_release); }
};

extern threadpool thread_pool;
