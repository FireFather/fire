#pragma once
#include <cstdint>
#include "main.h"
#include "position.h"

namespace zobrist{
  extern uint64_t psq[num_pieces][num_squares];
  extern uint64_t enpassant[num_files];
  extern uint64_t castle[castle_possible_n];
  extern uint64_t on_move;
  extern uint64_t hash_50_move[32];
}
