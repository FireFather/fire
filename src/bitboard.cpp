/*
 * Chess Engine - Bitboard Module (Implementation)
 * -----------------------------------------------
 * Initializes and provides fast bitboard utilities:
 *   - Precomputed single-square masks, ranks/files, distances
 *   - Pawn attack spans and passed-pawn masks
 *   - Non-sliding attacks for king and knight
 *   - Magic bitboard tables for bishop and rook sliding attacks
 *   - Helper functions for between/connection bitboards and king zone
 *
 * This is a performance-critical component used by move generation,
 * evaluation, and search. All comments are ASCII-only to satisfy
 * toolchains that warn on UTF-8 in comments.
 */

#include "bitboard.h"
#include "macro.h"
#include "main.h"

uint64_t between_bb[num_squares][num_squares];
uint64_t square_bb[num_squares];
uint64_t adjacent_files_bb[num_files];
uint64_t ranks_in_front_bb[num_sides][num_ranks];
uint64_t in_front_bb[num_sides][num_squares];
uint64_t passed_pawn_mask[num_sides][num_squares];
uint64_t pawn_attack_span[num_sides][num_squares];
uint64_t pawnattack[num_sides][num_squares];
uint64_t empty_attack[num_piecetypes][num_squares];
uint64_t king_zone[num_squares];
uint64_t connection_bb[num_squares][num_squares];
int8_t square_distance[num_squares][num_squares];
uint64_t rook_mask[num_squares];
uint64_t bishop_mask[num_squares];
uint64_t* bishop_attack_table[64];
uint64_t* rook_attack_table[64];

// next_subset(subset, mask)
// -------------------------
// Iterate through all submasks of 'mask' in decreasing order.
// Given the current 'subset', returns the next subset;
// when it returns 0 after 0 input, iteration is complete.

namespace{
  uint64_t next_subset(const uint64_t subset,const uint64_t mask){
    return (subset-mask)&mask;
  }
}

/*
 * bitboard::init()
 * ----------------
 * Precompute bitboard tables:
 *   - Square masks, adjacent files, ranks forward for both colors
 *   - Pawn attack tables and spans
 *   - King zone masks (8-neighborhood with edge padding)
 *   - Non-sliding attacks for king and knight
 *   - Initialize magic slider tables and fill between/connection masks
 */

void bitboard::init(){
  for(auto sq=a1;sq<=h8;++sq) square_bb[sq]=1ULL<<sq;
  for(auto f=file_a;f<=file_h;++f)
    adjacent_files_bb[f]=
      (f>file_a?file_bb[f-1]:0)|
      (f<file_h?file_bb[f+1]:0);
  for(auto r=rank_1;r<rank_8;++r){
    ranks_in_front_bb[white][r]=
      ~(ranks_in_front_bb[black][r+1]=
        ranks_in_front_bb[black][r]|rank_bb[r]);
  }
  for(auto color=white;color<=black;++color){
    for(auto sq=a1;sq<=h8;++sq){
      in_front_bb[color][sq]=
        ranks_in_front_bb[color][rank_of(sq)]&file_bb[file_of(sq)];
      pawn_attack_span[color][sq]=
        ranks_in_front_bb[color][rank_of(sq)]&adjacent_files_bb[file_of(sq)];
      passed_pawn_mask[color][sq]=
        in_front_bb[color][sq]|pawn_attack_span[color][sq];
    }
  }
  for(auto square1=a1;square1<=h8;++square1)
    for(auto square2=a1;square2<=h8;++square2)
      square_distance[square1][square2]=static_cast<int8_t>(
        std::max(file_distance(square1,square2),rank_distance(square1,square2)));
  for(auto sq=a1;sq<=h8;++sq){
    pawnattack[white][sq]=pawn_attack<white>(square_bb[sq]);
    pawnattack[black][sq]=pawn_attack<black>(square_bb[sq]);
  }
  for(auto piece=pt_king;piece<=pt_knight;++++piece){
    for(auto sq=a1;sq<=h8;++sq){
      uint64_t b=0;
      for(int i=0;i<8;++i){
        if(const auto to=sq+static_cast<square>(kp_delta[piece][i]);to>=a1&&to<=h8&&distance(sq,to)<3) b|=square_bb[to];
      }
      empty_attack[piece][sq]=b;
    }
  }
  for(auto sq=a1;sq<=h8;++sq){
    // Start from king moves, then pad on board edges
    uint64_t b=empty_attack[pt_king][sq];
    if(file_of(sq)==file_a) b|=b<<1;
    else if(file_of(sq)==file_h) b|=b>>1;
    if(rank_of(sq)==rank_1) b|=b<<8;
    else if(rank_of(sq)==rank_8) b|=b>>8;
    king_zone[sq]=b;
  }
  init_magic_sliders();
  for(auto square1=a1;square1<=h8;++square1){
    empty_attack[pt_bishop][square1]=attack_bishop_bb(square1,0);
    empty_attack[pt_rook][square1]=attack_rook_bb(square1,0);
    empty_attack[pt_queen][square1]=
      empty_attack[pt_bishop][square1]|empty_attack[pt_rook][square1];
    for(auto piece=pt_bishop;piece<=pt_rook;++piece){
      for(auto square2=a1;square2<=h8;++square2){
        if(!(empty_attack[piece][square1]&square_bb[square2])) continue;
        connection_bb[square1][square2]=
          (attack_bb(piece,square1,0)&attack_bb(piece,square2,0))|
          square_bb[square1]|square_bb[square2];
        between_bb[square1][square2]=
          attack_bb(piece,square1,square_bb[square2])&
          attack_bb(piece,square2,square_bb[square1]);
      }
    }
  }
}

/*
 * init_magic_bb(attack, attack_index, square_index, mask, shift, mult, deltas)
 * ----------------------------------------------------------------------------
 * Build magic move tables for a sliding piece:
 *   - 'attack' is the global attack array to fill
 *   - 'attack_index' is the per-square base index into 'attack'
 *   - 'square_index[sq]' will point inside 'attack' for fast lookup
 *   - 'mask[sq]' stores the blocker mask for that square
 *   - 'shift' and 'mult[sq]' define the magic hash
 *   - 'deltas' encodes sliding directions (rook or bishop)
 */

void init_magic_bb(uint64_t* attack,const int attack_index[],
  uint64_t* square_index[],uint64_t* mask,const int shift,
  const uint64_t mult[],const int deltas[4][2]){
  for(int sq=0;sq<64;++sq){
    const auto index=attack_index[sq];
    square_index[sq]=attack+index;
    mask[sq]=sliding_attacks(sq,0,deltas,1,6,1,6);
    uint64_t b=0;
    do{
      const int offset=static_cast<int>((b*mult[sq])>>shift);
      square_index[sq][offset]=sliding_attacks(sq,b,deltas,0,7,0,7);
      b=next_subset(b,mask[sq]);
    } while(b);
  }
}

/*
 * init_magic_sliders()
 * --------------------
 * Initialize magic tables for rook and bishop by calling init_magic_bb
 * with their respective parameters and direction deltas.
 */

void init_magic_sliders(){
  init_magic_bb(bitboard::magic_attack_r,bitboard::rook_magic_index,
    rook_attack_table,rook_mask,52,bitboard::rook_magics,
    rook_deltas);
  init_magic_bb(bitboard::magic_attack_r,bitboard::bishop_magic_index,
    bishop_attack_table,bishop_mask,55,bitboard::bishop_magics,
    bishop_deltas);
}

/*
 * sliding_attacks(sq, block, deltas, f_min, f_max, r_min, r_max)
 * --------------------------------------------------------------
 * Compute sliding attacks from 'sq' given blocking bitboard 'block'.
 * 'deltas' contains up to 4 (dx, dy) directions. The min/max bounds
 * restrict traversal on files and ranks for board-edge safe loops.
 */

uint64_t sliding_attacks(const int sq,const uint64_t block,
  const int deltas[4][2],const int f_min,const int f_max,
  const int r_min,const int r_max){
  uint64_t result=0;
  const int rk=sq/8;
  const int fl=sq%8;
  for(int direction=0;direction<4;++direction){
    const int dx=deltas[direction][0];
    const int dy=deltas[direction][1];
    for(int f=fl+dx,r=rk+dy;
        (dx==0||(f>=f_min&&f<=f_max))&&
        (dy==0||(r>=r_min&&r<=r_max));
        f+=dx,r+=dy){
      result|=square_bb[f+r*8];
      if(block&square_bb[f+r*8]) break;
    }
  }
  return result;
}
