#include "zobrist.h"
#include "position.h"

namespace zobrist {
  uint64_t psq[num_pieces][num_squares];
  uint64_t enpassant[num_files];
  uint64_t castle[castle_possible_n];
  uint64_t on_move;
  uint64_t hash_50_move[32];
}

uint64_t position::exception_key(const uint32_t move){
  return zobrist::psq[w_king][from_square(move)]^
    zobrist::psq[b_king][to_square(move)];
}

uint64_t position::draw50_key() const{
  return zobrist::hash_50_move[pos_info_->draw50_moves>>2];
}
