/*
 * Chess Engine - Time Control Module (Interface)
 * ----------------------------------------------
 * Declares the search_param struct (UCI limits) and the timecontrol
 * class which manages per-move time budgets and elapsed timing.
 */

/*
 * Chess Engine - Chrono / Time Control Module (Interface)
 * -------------------------------------------------------
 * Declares the timecontrol class and supporting types.
 * Provides methods to initialize, query, and adjust time usage.
 * Used by the UCI front end and search loop to decide how long
 * to think on a move.
 */

#pragma once
#include <chrono>
#include "main.h"

// Alias for a bounded move list used in search parameters

using max_moves_list=movelist<max_moves>;

// Steady clock time point used for timing

using time_point=std::chrono::steady_clock::time_point;

// Return current steady_clock time point

// Monotonic clock helper

inline time_point now(){
  return std::chrono::steady_clock::now();
}

// Search parameters provided from UCI "go" command and used by timecontrol

/*
 * search_param
 * ------------
 * Container for UCI search limits and runtime state:
 *   - time[], inc[]: remaining milliseconds and increments
 *   - moves_to_go: moves until the next time control (0 if unknown)
 *   - depth, nodes, move_time, mate, infinite, ponder: search limits
 *   - search_moves: optional restriction to a subset of moves
 *   - start_time: when the current search started
 */

struct search_param{
  search_param() : moves_to_go(0), depth(0), move_time(0), mate(0), infinite(0), ponder(0),
  nodes(0){
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

// Time control logic for allocating search time per move

/*
 * timecontrol
 * -----------
 * Computes per-move time budgets based on search_param and heuristics.
 * Provides optimum() and maximum() getters and elapsed() timing.
 */

class timecontrol{
public:
  // Compute time budgets for this move
  void init(const search_param& limit,side me,int ply);

  [[nodiscard]]// Preferred think time for this move (ms)
  int64_t optimum() const{
    return optimal_time_;
  }

  [[nodiscard]]// Hard cap for this move (ms)
  int64_t maximum() const{
    return maximum_time_;
  }

  [[nodiscard]]// Milliseconds since init()
  int64_t elapsed() const;
  [[nodiscard]]// Importance curve used to distribute time
  static double calc_move_importance(int ply);
  // Rescale budgets on ponder hit
  void adjustment_after_ponder_hit();

  // Safety margin for GUI/OS overhead (ms)
  int move_overhead=50;
private:
  time_point start_time_{};
  int64_t optimal_time_=0;
  int64_t maximum_time_=0;

  // Parameters for the importance curve and allocation policy
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

// Global instance of timecontrol used by search
extern timecontrol time_control;
