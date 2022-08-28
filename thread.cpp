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

#include "thread.h"

#include <iomanip>
#include <iostream>

#include "fire.h"

cmhinfo* cmh_data;

thread::thread() : exit_(false), search_active_(true), thread_index_(thread_pool.thread_count)
{
	std::unique_lock lk(mutex_);

	native_thread_ = std::thread(&thread::idle_loop, this);
	sleep_condition_.wait(lk, [&]
	{
		return !search_active_;
	});
}

thread::~thread()
{
	mutex_.lock();
	exit_ = true;
	sleep_condition_.notify_one();
	mutex_.unlock();
	native_thread_.join();
}

void threadpool::init()
{
	cmh_data = static_cast<cmhinfo*>(calloc(sizeof(cmhinfo), true));

	threads[0] = new mainthread;
	thread_count = 1;
	end_games.init_endgames();
	end_games.init_scale_factors();
	change_thread_count(thread_count);
	fifty_move_distance = 50;
	multi_pv = 1;
	total_analyze_time = 0;
}

void threadpool::begin_search(position& pos, const search_param& time)
{
	main()->wait_for_search_to_end();

	search::signals.stop_if_ponder_hit = search::signals.stop_analyzing = false;
	search::param = time;

	root_position = &pos;

	main()->wake(true);
}

void threadpool::delete_counter_move_history()
{
	cmh_data->counter_move_stats.clear();
}

void threadpool::change_thread_count(int const num_threads)
{
	assert(uci_threads > 0);

	while (thread_count < num_threads)
		threads[thread_count++] = new thread;

	while (thread_count > num_threads)
		delete threads[--thread_count];
}

void threadpool::exit()
{
	while (thread_count > 0)
		delete threads[--thread_count];

	free(cmh_data);
}

void thread::idle_loop()
{
	cmhi = cmh_data;

	auto* p = calloc(sizeof(threadinfo), true);
	std::memset(p, 0, sizeof(threadinfo));
	ti = new(p) threadinfo;

	root_position = &ti->root_position;

	while (!exit_)
	{
		std::unique_lock lk(mutex_);

		search_active_ = false;

		while (!search_active_ && !exit_)
		{
			sleep_condition_.notify_one();
			sleep_condition_.wait(lk);
		}

		lk.unlock();

		if (!exit_)
			begin_search();
	}

	free(p);
}

uint64_t threadpool::tb_hits() const
{
	uint64_t hits = 0;
	for (auto i = 0; i < active_thread_count; ++i)
		hits += threads[i]->root_position->tb_hits();
	return hits;
}

void thread::wait(const std::atomic_bool& condition)
{
	std::unique_lock lk(mutex_);
	sleep_condition_.wait(lk, [&]
	{
		return static_cast<bool>(condition);
	});
}

void thread::wait_for_search_to_end()
{
	std::unique_lock lk(mutex_);
	sleep_condition_.wait(lk, [&]
	{
		return !search_active_;
	});
}

void thread::wake(const bool activate_search)
{
	std::unique_lock lk(mutex_);

	if (activate_search)
		search_active_ = true;

	sleep_condition_.notify_one();
}

uint64_t threadpool::visited_nodes() const
{
	uint64_t nodes = 0;
	for (auto i = 0; i < active_thread_count; ++i)
		nodes += threads[i]->root_position->visited_nodes();
	return nodes;
}

threadpool thread_pool;