/*
 * Chess Engine - Time Control Module (Implementation)
 * --------------------------------------------------
 * Implements thinking time allocation for the engine. The main entry
 * points are:
 *   - timecontrol::init(limit, me, ply): compute optimum and maximum
 *     time to spend on the current move under UCI controls.
 *   - timecontrol::adjustment_after_ponder_hit(): rescale budgets when
 *     pondering hits, so we keep a fair share of the remaining time.
 *   - timecontrol::elapsed(): milliseconds since search start.
 *   - timecontrol::calc_move_importance(ply): heuristic importance of
 *     a move at a given depth/ply, used to distribute time over moves.
 *
 * Notes:
 *   - All comments are ASCII-only to avoid UTF-8 warnings.
 *   - The math aims for smooth time usage over a horizon of moves.
 */

/*
 * Chess Engine - Chrono / Time Control Module (Implementation)
 * ------------------------------------------------------------
 * Implements time allocation logic for search. Responsible for:
 *   - Initializing optimal and maximum times per move
 *   - Adjusting times based on depth (ply), remaining moves, increments
 *   - Supporting ponder hit adjustments
 *   - Providing elapsed time tracking
 *
 * All calculations are in milliseconds and designed to balance
 * strength and safety (avoiding flag fall). ASCII-only comments.
 */

#include "chrono.h"
#include <cmath>
#include "main.h"
#include "uci.h"

/*
 * init(limit, me, ply)
 * --------------------
 * Initialize optimal and maximum time to spend on the current move.
 * Steps:
 *   1) Set start_time_ to current time.
 *   2) Base optimal and maximum on remaining time for 'me'.
 *   3) For up to moves_horizon_ (or moves_to_go if given):
 *        - Calculate "move importance" for current ply.
 *        - Use ratios to decide safe (optimal) and emergency (maximum) times.
 *        - Adjust available time by adding increment minus move_overhead.
 *   4) Clamp results between minimum_time and maximum_time.
 *   5) If pondering is enabled, increase optimal time by ponder bonus ratio.
 */

/*
 * timecontrol::init(limit, me, ply)
 * ---------------------------------
 * Compute optimum and maximum time for the current move.
 * Inputs:
 *   - limit: UCI time controls and per-move caps
 *   - me   : side to move
 *   - ply  : current search ply (used to scale importance)
 * Algorithm (simplified):
 *   1) Set available time = remaining time - move overhead.
 *   2) For up to moves_horizon_ moves ahead, apportion time using a
 *      move importance model. Keep both an "optimal" and a "maximum"
 *      allocation using different ratios (ratio1, ratio2/ratio3).
 *   3) Apply increment per move and clamp to min/max guard rails.
 *   4) If pondering is on, add a bonus portion to optimum (capped).
 */

void timecontrol::init(const search_param& limit,const side me,const int ply){
  start_time_=now();
  // Record the baseline for elapsed()
  optimal_time_=maximum_time_=limit.time[me];
  // Initial budgets equal to remaining clock time
  const int maxmoves=limit.moves_to_go?std::min(limit.moves_to_go,moves_horizon_):moves_horizon_;
  // Horizon: either moves_to_go if provided, else a fixed cap
  const double move_importance=calc_move_importance(ply)*move_importance_factor_;
  // Base importance for the current move (scaled)
  // Accumulated importance of other moves remaining in the game
  double other_moves_importance=0;
  // Time available for allocation, minus per-move overhead
  int64_t available=static_cast<int64_t>(limit.time[me])-move_overhead;
  // Remaining time minus safety overhead
  for(int n=1;n<=maxmoves;++n){
    // Distribute remaining time over the next 'maxmoves' moves
    const double ratio1=move_importance/(move_importance+other_moves_importance);
    // Balanced split between this move and future moves
    double ratio2=max_ratio_*move_importance/(max_ratio_*move_importance+other_moves_importance);
    // More aggressive share when near a tactical phase
    double ratio3=(move_importance+steal_ratio_*other_moves_importance)/
      // Limit how much time we can steal from the future
      (move_importance+other_moves_importance);
    int64_t t1=std::llround(static_cast<double>(available)*ratio1);
    // Candidate optimum budget
    int64_t t2=std::llround(static_cast<double>(available)*std::min(ratio2,ratio3));
    // Candidate maximum budget (guarded)
    optimal_time_=std::min(optimal_time_,t1);
    maximum_time_=std::min(maximum_time_,t2);
    other_moves_importance+=calc_move_importance(ply+2*n);
    // Model that later plies still need time too
    available+=static_cast<int64_t>(limit.inc[me])-move_overhead;
    // Add per-move increment, keep overhead
  }
  optimal_time_=std::clamp(optimal_time_,minimum_time,maximum_time_);
  // Keep optimum within [min, max]
  maximum_time_=std::max(maximum_time_,minimum_time);
  if(uci_ponder){
    // If pondering is enabled, allow a small bonus to optimum
    optimal_time_+=static_cast<int64_t>(optimal_time_*ponder_bonus_ratio);
    optimal_time_=std::min(optimal_time_,maximum_time_);
    // Never exceed the maximum budget
  }
}

/*
 * adjustment_after_ponder_hit()
 * -----------------------------
 * After a successful ponder hit (opponent plays expected move),
 * increase maximum time by elapsed ponder time and scale optimal_time_
 * proportionally.
 */

/*
 * timecontrol::adjustment_after_ponder_hit()
 * ------------------------------------------
 * If a pondered move was played, rescale optimum time to the new
 * maximum budget so we keep a similar fraction of time as before.
 */

void timecontrol::adjustment_after_ponder_hit(){
  const int64_t new_max_time=maximum_time_+elapsed();
  optimal_time_=optimal_time_*new_max_time/maximum_time_;
}

/*
 * calc_move_importance(ply)
 * -------------------------
 * Return a factor representing how important the current move is,
 * based on its distance from opening and endgame. Used to allocate
 * more time to critical moments (midgame) and less to trivial moves.
 * Uses a Gaussian-like curve modulated by a logistic term.
 */

/*
 * timecontrol::calc_move_importance(ply)
 * --------------------------------------
 * Heuristic importance curve over plies. The curve is a smoothed
 * bell-like function: higher importance around midgame plies and
 * tapered in opening and endgame, then skewed by a logistic term
 * to shift weight later in the game.
 */

double timecontrol::calc_move_importance(const int ply){
  double factor=1.0;
  // Parabolic term around base_moves_: peaks near midgame
  if(ply>ply_min_&&ply<ply_max_){
    const double diff=static_cast<double>(ply)-base_moves_;
    factor=factor_base_-ply_factor_*diff*diff;
  }
  // Logistic tail to shift weight later: 1 / (1 + exp((ply-x_shift_)/x_scale_))^skew_
  return factor*std::pow(1+std::exp((ply-x_shift_)/x_scale_),-skew_);
}

/*
 * elapsed()
 * ---------
 * Return milliseconds elapsed since init() call.
 */

/*
 * timecontrol::elapsed()
 * ----------------------
 * Milliseconds since the recorded start_time_.
 */

int64_t timecontrol::elapsed() const{
  return std::chrono::duration_cast<std::chrono::milliseconds>(now()-start_time_).count();
}

timecontrol time_control;
