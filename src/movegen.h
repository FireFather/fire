#pragma once
#include "main.h"

class position;

struct s_move {
  uint32_t move;
  int value;
  operator uint32_t() const { return move; }
  void operator=(const uint32_t z) { move = z; }
};

enum move_gen {
  captures_promotions,
  quiet_moves,
  quiet_checks,
  evade_check,
  all_moves,
  pawn_advances,
  queen_checks,
  castle_moves
};

namespace movegen {
  template <side me, move_gen mg>
  s_move* moves_for_pawn(const position& pos, s_move* moves, uint64_t target);

  template <side me, uint8_t piece, bool only_check_moves>
  s_move* moves_for_piece(const position& pos, s_move* moves, uint64_t target);

  template <uint8_t castle, bool only_check_moves, bool chess960>
  s_move* get_castle(const position& pos, s_move* moves);
}   

inline bool operator<(const s_move& f, const s_move& s) { return f.value < s.value; }

template <move_gen>
s_move* generate_moves(const position& pos, s_move* moves);
s_move* generate_captures_on_square(const position& pos, s_move* moves,
  square sq);
s_move* generate_legal_moves(const position& pos, s_move* moves);

struct legal_move_list {
  explicit legal_move_list(const position& pos) { last_ = generate_legal_moves(pos, moves_); }

  [[nodiscard]] const s_move* begin() const { return moves_; }
  [[nodiscard]] const s_move* end() const { return last_; }
  [[nodiscard]] size_t size() const { return last_ - moves_; }

private:
  s_move* last_;
  s_move moves_[max_moves]{};
};

bool legal_moves_list_contains_move(const position& pos, uint32_t move);
bool at_least_one_legal_move(const position& pos);
bool legal_move_list_contains_castle(const position& pos, uint32_t move);
