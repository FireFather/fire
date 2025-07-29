#pragma once
#include <chrono>
#include "main.h"

using max_moves_list=movelist<max_moves>;
using time_point=std::chrono::steady_clock::time_point;

inline time_point now(){
  return std::chrono::steady_clock::now();
}

struct search_param{
  search_param() : moves_to_go(0), depth(0), move_time(0), mate(0), infinite(0), ponder(0),
  nodes(0), search_moves(), start_time(){
    time[white]=time[black]=inc[white]=inc[black]=0;
  }

  [[nodiscard]] bool use_time_calculating() const{
    return !(mate|move_time|depth|nodes|infinite);
  }

  int time[num_sides]{};
  int inc[num_sides]{};
  int moves_to_go,depth,move_time,mate,infinite,ponder;
  uint64_t nodes;
  max_moves_list search_moves;
  time_point start_time;
};

class timecontrol{
public:
  void init(const search_param& limit,side me,int ply);

  [[nodiscard]] int64_t optimum() const{
    return optimal_time_;
  }

  [[nodiscard]] int64_t maximum() const{
    return maximum_time_;
  }

  [[nodiscard]] int64_t elapsed() const;
  [[nodiscard]] static double calc_move_importance(int ply);
  void adjustment_after_ponder_hit();

  int move_overhead=50;
private:
  time_point start_time_{};
  int64_t optimal_time_=0;
  int64_t maximum_time_=0;

  static constexpr double x_scale_=7.64;
  static constexpr double x_shift_=58.4;
  static constexpr double skew_=0.183;
  static constexpr double factor_base_=1.225;
  static constexpr double ply_factor_=0.00025;
  static constexpr int ply_min_=10;
  static constexpr int ply_max_=70;
  static constexpr int base_moves_=50;

  static constexpr double move_importance_factor_=0.89;
  static constexpr int moves_horizon_=50;
  static constexpr double max_ratio_=7.09;
  static constexpr double steal_ratio_=0.35;
  static constexpr int64_t minimum_time=1;
  static constexpr double ponder_bonus_ratio=0.3;
};

extern timecontrol time_control;
