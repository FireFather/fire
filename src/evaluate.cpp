/*
 * Chess Engine - Evaluation Module (Implementation)
 * -------------------------------------------------
 * This file implements the evaluation entry points, currently delegating
 * entirely to an NNUE (Efficiently Updatable Neural Network) backend.
 *
 * The flow:
 * - eval_nnue(): Prepares an array of piece types and square indices
 * in NNUE's expected format, then calls nnue_evaluate().
 * - eval(): Public entry point used by search; wraps eval_nnue().
 * - eval_after_null_move(): Utility to quickly adjust eval after a null move.
 *
 * The NNUE interface expects:
 * pieces[] : numeric piece codes (engine-defined mapping to NNUE IDs)
 * squares[] : 0..63 board indices
 *
 * These functions are performance-critical but relatively straightforward.
 */

#include "main.h"
#include "nnue.h"
#include "position.h"

namespace evaluate{namespace{
/*
 * eval_nnue(pos)
 * --------------
 * Internal helper to evaluate a position via NNUE.
 *
 * Steps:
 * 1) Initialize empty piece and square arrays.
 * 2) Iterate over all 64 squares:
 * - Identify the piece code from position::piece_on_square().
 * - Map engine's piece codes (1=white king, 9=black king, etc.)
 * to NNUE's internal IDs for each piece type and color.
 * - Store piece ID and square index in arrays.
 * - Index 0: white king, Index 1: black king.
 * - Remaining indices: all other pieces in any order.
 * 3) Pass arrays and side-to-move flag to nnue_evaluate().
 * 4) Return NNUE's score (centipawns relative to side to move).
 */
// eval_nnue: Extracts piece types and square indices from the position,
// prepares arrays for the NNUE input, and calls the neural network evaluator.
    int eval_nnue(const position& pos){
      // Arrays to hold piece type codes and corresponding square indices.
      // Max 33 pieces: index 0-1 reserved for kings, others start at index 2.
      int pieces[33]{};
      int squares[33]{};
      int index=2;// start filling from index 2
      // Iterate over all 64 squares
      for(uint8_t i=0;i<64;i++){
        const auto sq=static_cast<square>(i);
        // White king
        if(const int pc=pos.piece_on_square(sq);pc==1){
          pieces[0]=1;// code for white king
          squares[0]=i;
        }
        // Black king
        else if(pc==9){
          pieces[1]=7;// code for black king
          squares[1]=i;
        }
        // Other piece types: mapping pos code to NNUE code
        else if(pc==2){
          // white queen
          pieces[index]=6;
          squares[index]=i;
          index++;
        } else if(pc==3){
          // white rook
          pieces[index]=5;
          squares[index]=i;
          index++;
        } else if(pc==4){
          // white bishop
          pieces[index]=4;
          squares[index]=i;
          index++;
        } else if(pc==5){
          // white knight
          pieces[index]=3;
          squares[index]=i;
          index++;
        } else if(pc==6){
          // white pawn
          pieces[index]=2;
          squares[index]=i;
          index++;
        } else if(pc==10){
          // black queen
          pieces[index]=12;
          squares[index]=i;
          index++;
        } else if(pc==11){
          // black rook
          pieces[index]=11;
          squares[index]=i;
          index++;
        } else if(pc==12){
          // black bishop
          pieces[index]=10;
          squares[index]=i;
          index++;
        } else if(pc==13){
          // black knight
          pieces[index]=9;
          squares[index]=i;
          index++;
        } else if(pc==14){
          // black pawn
          pieces[index]=8;
          squares[index]=i;
          index++;
        }
      }
      // Call NNUE evaluator: on_move determines which side to evaluate for
      const int nnue_score=nnue_evaluate(pos.on_move(),pieces,squares);
      return nnue_score;
    }
  }// anonymous namespace

// Public API: Evaluate a position by computing the NNUE score.
  int eval(const position& pos){
    return eval_nnue(pos);
  }

// Evaluate after a null move: simply negate the evaluation.
  int eval_after_null_move(const int eval){
    return -eval;
  }
}// namespace evaluate
