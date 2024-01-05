#pragma once
#include <atomic>

#include "chrono.h"
#include "position.h"

typedef movelist<max_pv> principal_variation;

struct search_signals {
  std::atomic_bool stop_analyzing, stop_if_ponder_hit;
};

namespace search {
inline search_signals signals;
inline search_param param;
inline bool running;

void init();
void reset();
void adjust_time_after_ponder_hit();
extern uint8_t lm_reductions[2][2][64 * static_cast<int>(plies)][64];

enum nodetype { PV, nonPV };

inline int counter_move_bonus[max_ply];

inline int counter_move_value(const int d) {
  return counter_move_bonus[static_cast<uint32_t>(d) / plies];
}

inline int history_bonus(const int d) {
  return counter_move_bonus[static_cast<uint32_t>(d) / plies];
}

struct easy_move_manager {
  void clear() {
    key_after_two_moves = 0;
    third_move_stable = 0;
    pv[0] = pv[1] = pv[2] = no_move;
  }

  [[nodiscard]] uint32_t expected_move(const uint64_t key) const {
    return key_after_two_moves == key ? pv[2] : no_move;
  }

  void refresh_pv(position& pos, const principal_variation& pv_new) {
    assert(pv_new.size() >= 3);

    third_move_stable = pv_new[2] == pv[2] ? third_move_stable + 1 : 0;

    if (pv_new[0] != pv[0] || pv_new[1] != pv[1] || pv_new[2] != pv[2]) {
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
void copy_pv(uint32_t* pv, uint32_t move, const uint32_t* pv_lower);
void update_stats(const position& pos, bool state_check, uint32_t move,
                  int depth, const uint32_t* quiet_moves, int quiet_number);
void update_stats_quiet(const position& pos, bool state_check, int depth,
                        const uint32_t* quiet_moves, int quiet_number);
void update_stats_minus(const position& pos, bool state_check, uint32_t move,
                        int depth);
void send_time_info();

inline uint8_t lm_reductions[2][2][64 * static_cast<int>(plies)][64];

inline constexpr int razor_margin = 384;
inline constexpr auto futility_value_0 = 0;
inline constexpr auto futility_value_1 = 112;
inline constexpr auto futility_value_2 = 243;
inline constexpr auto futility_value_3 = 376;
inline constexpr auto futility_value_4 = 510;
inline constexpr auto futility_value_5 = 646;
inline constexpr auto futility_value_6 = 784;
inline constexpr auto futility_margin_ext_mult = 160;
inline constexpr auto futility_margin_ext_base = 204;

constexpr int futility_values[7] = {
    static_cast<int>(futility_value_0), static_cast<int>(futility_value_1),
    static_cast<int>(futility_value_2), static_cast<int>(futility_value_3),
    static_cast<int>(futility_value_4), static_cast<int>(futility_value_5),
    static_cast<int>(futility_value_6)};

inline int futility_margin(const int d) {
  return futility_values[static_cast<uint32_t>(d) / plies];
}

inline int futility_margin_ext(const int d) {
  return futility_margin_ext_base +
         futility_margin_ext_mult *
             static_cast<int>(static_cast<uint32_t>(d) / plies);
}

inline constexpr int late_move_number_values[2][32] = {
    {0,  0,  3,  3,  4,  5,  6,  7,  8,  10, 12, 15, 17, 20, 23, 26,
     30, 33, 37, 40, 44, 49, 53, 58, 63, 68, 73, 78, 83, 88, 94, 100},
    {0,  0,  5,  5,  6,  7,  9,  11, 14, 17, 20,  23,  27,  31,  35,  40,
     45, 50, 55, 60, 65, 71, 77, 84, 91, 98, 105, 112, 119, 127, 135, 143}};

inline int late_move_number(const int d, const bool progress) {
  return late_move_number_values[progress]
                                [static_cast<uint32_t>(d) / (plies / 2)];
}

inline int lmr_reduction(const bool pv, const bool vg, const int d,
                         const int n) {
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
  return lm_reductions[pv][vg][MIN(d, 64 * static_cast<int>(plies) - 1)]
                      [MIN(n, 63)];
}
}  // namespace search

template <int max_plus, int max_min>
struct piece_square_stats;
typedef piece_square_stats<24576, 24576> counter_move_values;
std::string score_cp(int score);

struct rootmove {
  rootmove() = default;

  explicit rootmove(const uint32_t move) {
    pv.move_number = 1;
    pv.moves[0] = move;
  }

  bool operator<(const rootmove& root_move) const {
    return root_move.score < score;
  }

  bool operator==(const uint32_t& move) const { return pv[0] == move; }

  bool ponder_move_from_hash(position& pos);
  void pv_from_hash(position& pos);

  int depth = depth_0;
  int score = -max_score;
  int previous_score = -max_score;
  int start_value = score_0;
  principal_variation pv;
};

struct rootmoves {
  rootmoves() = default;

  int move_number = 0, dummy = 0;
  rootmove moves[max_moves];

  void add(const rootmove& root_move) { moves[move_number++] = root_move; }

  rootmove& operator[](const int index) { return moves[index]; }

  const rootmove& operator[](const int index) const { return moves[index]; }

  void clear() { move_number = 0; }

  int find(const uint32_t move) {
    for (auto i = 0; i < move_number; i++)
      if (moves[i].pv[0] == move) return i;
    return -1;
  }
};
