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

inline cmhinfo* cmh_data;

class thread{
  std::thread native_thread;
  std_mutex mutex;
  cond_var sleep_condition;
  bool exit_,search_active;
  int thread_index;
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
  int completed_depth=no_depth;
  int active_pv{};
};

struct cmhinfo{
  counter_move_history counter_move_stats;
};

struct threadinfo{
  position root_position{};
  position_info position_inf[1024]{};
  s_move move_list[8192]{};
  move_value_stats history{};
  move_value_stats evasion_history{};
  max_gain_stats max_gain_table;
  counter_move_stats counter_moves{};
  counter_follow_up_move_stats counter_followup_moves;
  move_value_stats capture_history{};
};

struct mainthread final:thread{
  void begin_search() override;
  bool quick_move_allow=false;
  bool quick_move_played=false;
  bool quick_move_evaluation_busy=false;
  bool quick_move_evaluation_stopped=false;
  bool failed_low=false;
  int best_move_changed=0;
  int previous_root_score=score_0;
  int interrupt_counter=0;
  int previous_root_depth={};
};

struct threadpool:std::vector<thread*>{
  void init();
  void exit();

  int thread_count{};
  time_point start{};
  int total_analyze_time{};
  thread* threads[max_threads]{};

  [[nodiscard]] mainthread* main() const{
    return static_cast<mainthread*>(threads[0]);
  }

  void begin_search(position&,const search_param&);
  void change_thread_count(int num_threads);
  [[nodiscard]] uint64_t visited_nodes() const;
  static void delete_counter_move_history();

  int active_thread_count{};
  side contempt_color=num_sides;
  int piece_contempt{};
  int root_contempt_value=score_0;
  position* root_position{};
  rootmoves root_moves;
  position_info* root_position_info{};
  bool analysis_mode{};
  int fifty_move_distance{};
  int multi_pv{},multi_pv_max{};
  bool dummy_null_move_threat{},dummy_prob_cut{};
};

extern threadpool thread_pool;
