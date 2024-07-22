#include "main.h"
#include "nnue.h"
#include "position.h"

namespace evaluate {
  int eval_nnue(const position& pos) {
    int pieces[33]{};
    int squares[33]{};
    int index = 2;
    for (uint8_t i = 0; i < 64; i++) {
      if (pos.piece_on_square(static_cast<square>(i)) == 1) {
        pieces[0] = 1;
        squares[0] = i;
      }
      else if (pos.piece_on_square(static_cast<square>(i)) == 9) {
        pieces[1] = 7;
        squares[1] = i;
      }
      else if (pos.piece_on_square(static_cast<square>(i)) == 2) {
        pieces[index] = 6;
        squares[index] = i;
        index++;
      }
      else if (pos.piece_on_square(static_cast<square>(i)) == 3) {
        pieces[index] = 5;
        squares[index] = i;
        index++;
      }
      else if (pos.piece_on_square(static_cast<square>(i)) == 4) {
        pieces[index] = 4;
        squares[index] = i;
        index++;
      }
      else if (pos.piece_on_square(static_cast<square>(i)) == 5) {
        pieces[index] = 3;
        squares[index] = i;
        index++;
      }
      else if (pos.piece_on_square(static_cast<square>(i)) == 6) {
        pieces[index] = 2;
        squares[index] = i;
        index++;
      }
      else if (pos.piece_on_square(static_cast<square>(i)) == 10) {
        pieces[index] = 12;
        squares[index] = i;
        index++;
      }
      else if (pos.piece_on_square(static_cast<square>(i)) == 11) {
        pieces[index] = 11;
        squares[index] = i;
        index++;
      }
      else if (pos.piece_on_square(static_cast<square>(i)) == 12) {
        pieces[index] = 10;
        squares[index] = i;
        index++;
      }
      else if (pos.piece_on_square(static_cast<square>(i)) == 13) {
        pieces[index] = 9;
        squares[index] = i;
        index++;
      }
      else if (pos.piece_on_square(static_cast<square>(i)) == 14) {
        pieces[index] = 8;
        squares[index] = i;
        index++;
      }
    }
    const int nnue_score = nnue_evaluate(pos.on_move(), pieces, squares);
    return nnue_score;
  }

  int eval(const position& pos) {
    const int nnue_score = eval_nnue(pos);
    return nnue_score;
  }

  int eval_after_null_move(const int eval) {
    const auto result = -eval;
    return result;
  }
} // namespace evaluate
