#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include "main.h"
#include "movepick.h"
#include "mutex.h"
#include "position.h"
#include "search.h"

class thread {
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

  rootmoves root_moves;
  int completed_depth = no_depth;
  int active_pv{};
};

struct cmhinfo {
  counter_move_history counter_move_stats;
};

struct threadinfo {
  position root_position{};
  counter_follow_up_move_stats counter_followup_moves;
  counter_move_stats counter_moves{};
  max_gain_stats max_gain_table;
  move_value_stats capture_history{};
  move_value_stats evasion_history{};
  move_value_stats history{};
  position_info position_inf[1024]{};
  s_move move_list[8192]{};
};

struct mainthread final : thread {
  void begin_search() override;

  bool quick_move_allow = false;
  bool quick_move_played = false;
  bool quick_move_evaluation_busy = false;
  bool quick_move_evaluation_stopped = false;
  bool failed_low = false;

  int best_move_changed = 0;
  int previous_root_score = score_0;
  int interrupt_counter = 0;
  int previous_root_depth = {};
};

struct threadpool : std::vector<thread*> {
  [[nodiscard]] mainthread* main() const { return static_cast<mainthread*>(threads[0]); }
  [[nodiscard]] uint64_t visited_nodes() const;
  bool analysis_mode{};
  bool dummy_null_move_threat{}, dummy_prob_cut{};
  int active_thread_count{};
  int fifty_move_distance{};
  int multi_pv{}, multi_pv_max{};
  int piece_contempt{};
  int root_contempt_value = score_0;
  int thread_count{};
  int total_analyze_time{};
  position_info* root_position_info{};
  position* root_position{};
  rootmoves root_moves;
  side contempt_color = num_sides;
  static void delete_counter_move_history();
  thread* threads[max_threads]{};
  time_point start{};
  void begin_search(position&, const search_param&);
  void change_thread_count(int num_threads);
  void exit();
  void init();
};

extern threadpool thread_pool;
