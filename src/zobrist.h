#pragma once
#include <cstdint>

#include "main.h"
#include "position.h"

namespace zobrist {
  inline uint64_t psq[num_pieces][num_squares];
  inline uint64_t enpassant[num_files];
  inline uint64_t castle[castle_possible_n];
  inline uint64_t on_move;
  inline uint64_t hash_50_move[32];
}  // namespace zobrist
