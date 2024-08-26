#include "thread.h"
#include <iostream>
#include "main.h"

cmhinfo* cmh_data;

thread::thread()
  : exit_(false),
  search_active_(true),
  thread_index_(thread_pool.thread_count) {
  std::unique_lock lk(mutex_);
  native_thread_ = std::thread(&thread::idle_loop, this);
  sleep_condition_.wait(lk, [&] { return !search_active_; });
}

thread::~thread() {
  mutex_.lock();
  exit_ = true;
  sleep_condition_.notify_one();
  mutex_.unlock();
  native_thread_.join();
}

void threadpool::init() {
  cmh_data = static_cast<cmhinfo*>(calloc(sizeof(cmhinfo), true));
  threads[0] = new mainthread;
  thread_count = 1;
  change_thread_count(thread_count);
  fifty_move_distance = 50;
  multi_pv = 1;
  total_analyze_time = 0;
}

void threadpool::begin_search(position& pos, const search_param& time) {
  main()->wait_for_search_to_end();
  search::signals.stop_if_ponder_hit = search::signals.stop_analyzing = false;
  search::param = time;
  root_position = &pos;
  main()->wake(true);
}

void threadpool::delete_counter_move_history() { cmh_data->counter_move_stats.clear(); }

void threadpool::change_thread_count(const int num_threads) {
  assert(uci_threads > 0);
  while (thread_count < num_threads) threads[thread_count++] = new thread;
  while (thread_count > num_threads) delete threads[--thread_count];
}

void threadpool::exit() {
  while (thread_count > 0) delete threads[--thread_count];
  free(cmh_data);
}

void thread::idle_loop() {
  cmhi = cmh_data;

  auto* p = calloc(sizeof(threadinfo), true);
  if (p != nullptr) {
    std::memset(p, 0, sizeof(threadinfo));
    ti = new(p) threadinfo;
  }

  root_position = &ti->root_position;

  while (!exit_) {
    std::unique_lock lk(mutex_);
    search_active_ = false;

    while (!search_active_ && !exit_) {
      sleep_condition_.notify_one();
      sleep_condition_.wait(lk);
    }

    lk.unlock();
    if (!exit_) begin_search();
  }
  free(p);
}

void thread::wait(const std::atomic_bool& condition) {
  std::unique_lock lk(mutex_);
  sleep_condition_.wait(lk, [&] { return static_cast<bool>(condition); });
}

void thread::wait_for_search_to_end() {
  std::unique_lock lk(mutex_);
  sleep_condition_.wait(lk, [&] { return !search_active_; });
}

void thread::wake(const bool activate_search) {
  std::unique_lock lk(mutex_);
  if (activate_search) search_active_ = true;
  sleep_condition_.notify_one();
}

uint64_t threadpool::visited_nodes() const {
  uint64_t nodes = 0;
  for (auto i = 0; i < active_thread_count; ++i)
    nodes += threads[i]->root_position->visited_nodes();
  return nodes;
}
threadpool thread_pool;
