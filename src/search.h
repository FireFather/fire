/*
 * Chess Engine - Search Module (Interface)
 * ----------------------------------------
 * Declarations, small inline utilities, and search-related structs.
 *
 * Key types:
 * - search::nodetype: tags PV vs non-PV nodes at compile-time.
 * - search_signals: cooperative stop flags for time control & ponder.
 * - easy_move_manager: tracks a short PV prefix to opportunistically
 * play an "easy move" under time pressure.
 *
 * Key helpers:
 * - counter_move/history bonuses: learning heuristics for move ordering.
 * - futility margins & late-move formulae: forward pruning thresholds.
 * - lmr_reduction(): precomputed reductions indexed by (pv,progress,depth,move_no).
 *
 * See search.cpp for detailed algorithm-level commentary.
 */

#pragma once
#include <atomic>
#include <algorithm>
#include <string>
#include "chrono.h"
#include "position.h"
#include "movepick.h"

using principal_variation=movelist<max_pv>;

namespace search{
/**
 * Cooperative stop flags shared across threads.
 * - stop_analyzing: hard stop requested (time/node/GUI).
 * - stop_if_ponder_hit: stop early when pondered move appears on board.
 */

  struct search_signals{
    std::atomic_bool stop_analyzing=false;
    std::atomic_bool stop_if_ponder_hit=false;
  };

  inline search_signals signals;
  inline search_param param;
  inline bool running=false;

  void init();
  void reset();
  void adjust_time_after_ponder_hit();

/**
 * Node classification
 * -------------------
 * - is_pv : node lies on the principal variation; search returns exact scores,
 * pruning is conservative, and PV is maintained.
 * - non_pv : interior node used for bounds; allows more aggressive pruning.
 */

  enum nodetype : uint8_t{
    is_pv,non_pv
  };

// Heuristic learning tables
// -------------------------
// counter_move_bonus[d] is used both as history and countermove bonus.
// Depth is bucketed by 'plies' to keep arrays small while reflecting
// increased confidence from deeper cutoffs.

  inline int counter_move_bonus[max_ply];

  [[nodiscard]] inline int counter_move_value(const int d){
    return counter_move_bonus[static_cast<uint32_t>(d)/plies];
  }

  inline int history_bonus(const int d){
    return counter_move_bonus[static_cast<uint32_t>(d)/plies];
  }

/**
 * Tracks a short PV prefix to detect "easy moves".
 * If the same first three PV moves remain stable and the time is tight,
 * the engine may play pv[0] early.
 */

  struct easy_move_manager{
    void clear(){
      key_after_two_moves=0;
      third_move_stable=0;
      pv[0]=pv[1]=pv[2]=no_move;
    }

    [[nodiscard]] uint32_t expected_move(const uint64_t key) const{
      return (key_after_two_moves==key)?pv[2]:no_move;
    }

    void refresh_pv(position& pos,const principal_variation& pv_new){
      if(pv_new[2]==pv[2]){
        ++third_move_stable;
      } else{
        third_move_stable=0;
      }
      if(pv_new[0]!=pv[0]||pv_new[1]!=pv[1]||pv_new[2]!=pv[2]){
        std::copy_n(&pv_new[0],3,pv);
        pos.play_move(pv[0]);
        pos.play_move(pv[1]);
        key_after_two_moves=pos.key();
        pos.take_move_back(pv[1]);
        pos.take_move_back(pv[0]);
      }
    }

    int third_move_stable=0;
    uint64_t key_after_two_moves=0;
    uint32_t pv[3]={no_move,no_move,no_move};
  };

  inline easy_move_manager easy_move;
  inline int draw[num_sides];
  inline uint64_t previous_info_time;

  template<nodetype nt> int alpha_beta(position& pos,int alpha,int beta,int depth,bool cut_node);
  template<nodetype nt,bool state_check> int q_search(position& pos,int alpha,int beta,int depth);

  int value_to_hash(int val,int ply);
  int value_from_hash(int val,int ply);
  void copy_pv(uint32_t* pv,uint32_t move,const uint32_t* pv_lower);
  void update_stats(const position& pos,bool state_check,uint32_t move,int depth,const uint32_t* quiet_moves,int quiet_number);
  void update_stats_quiet(const position& pos,bool state_check,int depth,const uint32_t* quiet_moves,int quiet_number);
  void update_stats_minus(const position& pos,bool state_check,uint32_t move,int depth);
  void send_time_info();

  inline uint8_t lm_reductions[2][2][64*plies][64];

// Pruning thresholds & margins
// ----------------------------
// Tuned constants used by:
// - Razoring (preQS pruning when eval is far below alpha)
// - Futility pruning (skip hopeless nodes at shallow depths)
// - Extended futility (for deeper, nonPV nodes)

  inline constexpr int razor_margin=384;
  inline constexpr int futility_value_0=0;
  inline constexpr int futility_value_1=112;
  inline constexpr int futility_value_2=243;
  inline constexpr int futility_value_3=376;
  inline constexpr int futility_value_4=510;
  inline constexpr int futility_value_5=646;
  inline constexpr int futility_value_6=784;
  inline constexpr int futility_margin_ext_mult=160;
  inline constexpr int futility_margin_ext_base=204;

  constexpr int futility_values[7]={
  futility_value_0,futility_value_1,futility_value_2,futility_value_3,
  futility_value_4,futility_value_5,futility_value_6
  };

  inline int futility_margin(const int d){
    return futility_values[static_cast<uint32_t>(d)/plies];
  }

  inline int futility_margin_ext(const int d){
    return futility_margin_ext_base+
      futility_margin_ext_mult*(static_cast<int>(static_cast<uint32_t>(d)/plies));
  }

  inline constexpr int late_move_number_values[2][32]={
  {0,0,3,3,4,5,6,7,8,10,12,15,17,20,23,26,30,33,37,40,44,49,53,58,63,68,73,78,83,88,94,100},
  {0,0,5,5,6,7,9,11,14,17,20,23,27,31,35,40,45,50,55,60,65,71,77,84,91,98,105,112,119,127,135,143}
  };

// Latemove counting
// ------------------
// Determines from (depth, progress) how many moves are considered "early".
// Moves after this index are eligible for significant LMR reductions.

  inline int late_move_number(const int d,const bool progress){
    return late_move_number_values[progress][static_cast<uint32_t>(d)/(plies/2)];
  }

// Lookup precomputed LMR reductions for (pv, progress, depth, move_no).
// Values are measured in 'plies' and clamped for safety.

  inline int lmr_reduction(const bool pv,const bool vg,const int d,const int n){
    const int d_idx=(std::min)(d,64*plies-1);
    const int n_idx=(std::min)(n,63);
    return lm_reductions[pv][vg][d_idx][n_idx];
  }
}

template<int max_plus,int max_min> struct piece_square_stats;
using counter_move_values=piece_square_stats<24576,24576>;
std::string score_cp(int score);

struct rootmove{
  rootmove() = default;

  explicit rootmove(const uint32_t move){
    pv.move_number=1;
    pv.moves[0]=move;
  }

  bool operator<(const rootmove& rhs) const{
    return rhs.score<score;
  }

  bool operator==(const uint32_t& move) const{
    return pv[0]==move;
  }

  bool ponder_move_from_hash(position& pos);
  void pv_from_hash(position& pos);

  int depth=depth_0;
  int score=-max_score;
  int previous_score=-max_score;
  int start_value=score_0;
  principal_variation pv;
};

struct rootmoves{
  rootmoves() = default;

  int move_number=0;
  int dummy=0;
  rootmove moves[max_moves];

  void add(const rootmove& rm){
    moves[move_number++]=rm;
  }

  rootmove& operator[](const int i){
    return moves[i];
  }

  const rootmove& operator[](const int i) const{
    return moves[i];
  }

  void clear(){
    move_number=0;
  }

  [[nodiscard]] int find(const uint32_t move) const{
    for(int i=0;i<move_number;++i) if(moves[i].pv[0]==move) return i;
    return -1;
  }
};
