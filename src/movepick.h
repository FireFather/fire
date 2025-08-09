/*
 * Chess Engine - Move Picker (Interface)
 * ---------------------------------------
 * Declares the move_picker class and supporting data structures.
 * Provides staged move iteration tailored to search contexts.
 */

#pragma once
#include <cstring>

#include "main.h"
#include "movegen.h"
#include "position.h"

/*
 * piece_order
 * -----------
 * Rough relative piece values used in capture ordering (LVA factor).
 */

namespace movepick{
  constexpr int piece_order[num_pieces]={
  0,6,1,2,3,4,5,0,
  0,6,1,2,3,4,5,0
  };

/*
 * capture_sort_values
 * -------------------
 * MVV scores for captured piece type to bias capture ordering.
 */

  constexpr int capture_sort_values[num_pieces]={
  0,0,198,817,836,1270,2521,0,0,0,198,817,836,1270,2521,0
  };

  // Initialize staged picker for a regular node
  void init_search(const position&,uint32_t,int,bool);
  // Initialize staged picker for quiescence node
  void init_q_search(const position&,uint32_t,int,square);
  // Initialize staged picker for ProbCut node
  void init_prob_cut(const position&,uint32_t,int);

  // Return next move based on current stage, or no_move
  uint32_t pick_move(// Position being searched
    const position& pos);

  // Score moves for a move_gen class
  template<move_gen> void score(const position& pos);
}

/*
 * piece_square_table<T>
 * ---------------------
 * 2D array indexed by [piece][square] with helpers to read/update.
 * Used as a base for history-like statistics.
 */

template<typename tn> struct piece_square_table{
  const tn* operator[](ptype piece) const{
    return table_[piece];
  }

  tn* operator[](ptype piece){
    return table_[piece];
  }

  void clear(){
    std::memset(table_,0,sizeof(table_));
  }

  [[nodiscard]] tn get(ptype piece,square to) const{
    return table_[piece][to];
  }

  void update(ptype piece,square to,tn val){
    table_[piece][to]=val;
  }
protected:
  tn table_[num_pieces][num_squares];

  tn& operator[](const int offset){
    return table_[offset/num_squares][offset%num_squares];
  }

  const tn& operator[](const int offset) const{
    return table_[offset/num_squares][offset%num_squares];
  }
};

/*
 * piece_square_stats
 * ------------------
 * History update scheme with exponential decay via +/- update methods.
 * 'max_plus' and 'max_min' control saturation rate.
 */

template<int max_plus,int max_min> struct piece_square_stats:piece_square_table<int16_t>{
  static int calculate_offset(const ptype piece,const square to){
    return 64*static_cast<int>(piece)+static_cast<int>(to);
  }

  [[nodiscard]] int16_t value_at_offset(int offset) const{
    return (*this)[offset];
  }

  void fill(const int val){
    const int16_t v=static_cast<int16_t>(val);
    for(auto& p:this->table_) for(int s=0;s<num_squares;++s) p[s]=v;
  }

  void update_plus(int offset,const int val){
    int16_t& elem=(*this)[offset];
    elem-=elem*val/max_plus;
    elem+=val;
  }

  void update_minus(int offset,const int val){
    int16_t& elem=(*this)[offset];
    elem-=elem*val/max_min;
    elem-=val;
  }
};

/*
 * color_square_stats<T>
 * ---------------------
 * Per-side tables indexed by [color][square].
 */

template<typename T> struct color_square_stats{
  const T* operator[](side color) const{
    return table_[color];
  }

  T* operator[](side color){
    return table_[color];
  }

  void clear(){
    std::memset(table_,0,sizeof(table_));
  }
private:
  T table_[num_sides][num_squares];
};

/*
 * counter_move_full_stats
 * -----------------------
 * Map from [color][prev_move_low_12_bits] to a suggested reply move.
 */

struct counter_move_full_stats{
  [[nodiscard]] uint32_t get(const side color,const uint32_t move) const{
    return table_[color][move&0xfff];
  }

  void clear(){
    std::memset(table_,0,sizeof table_);
  }

  void update(const side color,const uint32_t move1,const uint32_t move){
    table_[color][move1&0xfff]=static_cast<uint16_t>(move);
  }
private:
  uint16_t table_[num_sides][64*64]={};
};

/*
 * counter_follow_up_move_stats
 * ----------------------------
 * Map from (piece1,to1,piece2,to2) to a suggested follow-up move,
 * used to find a plausible counter when the plain counter is missing.
 */

struct counter_follow_up_move_stats{
  [[nodiscard]] uint32_t get(const ptype piece1,const square to1,const ptype piece2,
    const square to2) const{
    return table_[piece1][to1][piece_type(piece2)][to2];
  }

  void clear(){
    std::memset(table_,0,sizeof table_);
  }

  void update(const ptype piece1,const square to1,const ptype piece2,
    const square to2,const uint32_t move){
    table_[piece1][to1][piece_type(piece2)][to2]=static_cast<uint16_t>(move);
  }
private:
  uint16_t table_[num_pieces][num_squares][num_piecetypes][num_squares]={};
};

/*
 * max_gain_stats
 * --------------
 * Tracks the maximum capture gain seen for a quiet move to bias ordering.
 */

struct max_gain_stats{
  [[nodiscard]] int get(const ptype piece,const uint32_t move) const{
    return table_[piece][move&0x0fff];
  }

  void clear(){
    std::memset(table_,0,sizeof table_);
  }

  void update(const ptype piece,const uint32_t move,const int gain){
    auto* const p_gain=&table_[piece][move&0x0fff];
    *p_gain+=(gain-*p_gain+8)>>4;
  }
private:
  int table_[num_pieces][64*64]={};
};

/*
 * killer_stats
 * ------------
 * Stores one quiet "killer" move per hashed position index for each side.
 * The index is derived from CRC16 of piece bitboards.
 */

struct killer_stats{
  static int index_my_pieces(const position& pos,side color);
  static int index_your_pieces(const position& pos,side color,square to);

  [[nodiscard]] uint32_t get(const side color,const int index) const{
    return table_[color][index];
  }

  void clear(){
    std::memset(table_,0,sizeof table_);
  }

  void update(const side color,const int index,const uint32_t move){
    table_[color][index]=move;
  }
private:
  uint32_t table_[num_sides][65536]={};
};

// Shorthand aliases for history tables
using counter_move_stats=piece_square_table<uint16_t>;
using move_value_stats=piece_square_stats<8192,8192>;
using counter_move_values=piece_square_stats<3*8192,3*8192>;
using counter_move_history=piece_square_table<counter_move_values>;

// Advance stage enum in place
inline stage& operator++(stage& d){
  return d=static_cast<stage>(static_cast<int>(d)+1);
}
