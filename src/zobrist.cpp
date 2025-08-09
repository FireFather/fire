/*
 * Chess Engine - Zobrist Hashing (Implementation)
 * -----------------------------------------------
 * Initializes and applies Zobrist hashing:
 *   - Random keys for piece-square combinations
 *   - Keys for side to move, castling rights, en passant files
 *   - Incremental updates for move making and unmaking
 *
 * ASCII-only comments for compatibility.
 */

#include "zobrist.h"
#include "position.h"

/*
 * Global Zobrist key arrays
 * -------------------------
 * These are filled once at startup by position::init().
 */

namespace zobrist{
  uint64_t psq[num_pieces][num_squares];
  uint64_t enpassant[num_files];
  uint64_t castle[castle_possible_n];
  uint64_t on_move;
  uint64_t hash_50_move[32];
}

/*
 * position::exception_key(move)
 * -----------------------------
 * Special hash adjustment used for PV-reconstruction or repetition
 * detection when kings move across each other. Uses only king piece-square
 * keys to build a lightweight differential key for the move.
 */

uint64_t position::exception_key(const uint32_t move){
  return zobrist::psq[w_king][from_square(move)]^
    zobrist::psq[b_king][to_square(move)];
}

/*
 * position::draw50_key()
 * ----------------------
 * Return the Zobrist key contribution for the 50-move counter bucket.
 * The counter is divided by 4 (>> 2) to reduce table size.
 */

uint64_t position::draw50_key() const{
  return zobrist::hash_50_move[pos_info_->draw50_moves>>2];
}
