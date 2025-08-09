/*
 * Chess Engine - Zobrist Hashing (Interface)
 * ------------------------------------------
 * Declares global Zobrist key tables and the initialization function.
 */

#pragma once
#include <cstdint>
#include "main.h"
#include "position.h"

/*
 * zobrist namespace
 * -----------------
 * Holds Zobrist key arrays and init() declaration.
 */

/*
 * Zobrist key tables
 * ------------------
 * Extern declarations for hashing components used in position keys.
 */

namespace zobrist{
  // piece-square[pt][sq] keys
  extern uint64_t psq[num_pieces][num_squares];
  // en passant file keys
  extern uint64_t enpassant[num_files];
  // castling rights keys
  extern uint64_t castle[castle_possible_n];
  extern uint64_t on_move;
  extern uint64_t hash_50_move[32];
}
