/*
  Fire is a freeware UCI chess playing engine authored by Norman Schmidt.

  Fire utilizes many state-of-the-art chess programming ideas and techniques
  which have been documented in detail at https://www.chessprogramming.org/
  and demonstrated via the very strong open-source chess engine Stockfish...
  https://github.com/official-stockfish/Stockfish.
  
  Fire is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or any later version.

  You should have received a copy of the GNU General Public License with
  this program: copying.txt.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "evaluate.h"

#include "endgame.h"
#include "fire.h"
#include "nnue/nnue.h"
#include "pragma.h"

namespace evaluate
{
	/*
	* Piece codes are
	*     wking=1, wqueen=2, wrook=3, wbishop= 4, wknight= 5, wpawn= 6,
	*     bking=7, bqueen=8, brook=9, bbishop=10, bknight=11, bpawn=12,
	*     
	* Squares are
	*     A1=0, B1=1 ... H8=63
	*     
	* Input format:
	*     piece[0] is white king, square[0] is its location
	*     piece[1] is black king, square[1] is its location
	*     ..
	*     piece[x], square[x] can be in any order
	*     ..
	*     piece[n+1] is set to 0 to represent end of array
	*     
	* Returns
	*   Score relative to side to move in approximate centipawns
	*/

	// builds pieces & squares arrays as required by nnue specs above
	int eval_nnue(const position& pos)
	{
		int pieces[33]{};
		int squares[33]{};
		int index = 2;
		for (uint8_t i = 0; i < 64; i++)
		{
			if (pos.piece_on_square(static_cast<square>(i)) == 1)
			{
				pieces[0] = 1;
				squares[0] = i;
			}
			else if (pos.piece_on_square(static_cast<square>(i)) == 9)
			{
				pieces[1] = 7;
				squares[1] = i;
			}
			else if (pos.piece_on_square(static_cast<square>(i)) == 2)
			{
				pieces[index] = 6;
				squares[index] = i;
				index++;
			}
			else if (pos.piece_on_square(static_cast<square>(i)) == 3)
			{
				pieces[index] = 5;
				squares[index] = i;
				index++;
			}
			else if (pos.piece_on_square(static_cast<square>(i)) == 4)
			{
				pieces[index] = 4;
				squares[index] = i;
				index++;
			}
			else if (pos.piece_on_square(static_cast<square>(i)) == 5)
			{
				pieces[index] = 3;
				squares[index] = i;
				index++;
			}
			else if (pos.piece_on_square(static_cast<square>(i)) == 6)
			{
				pieces[index] = 2;
				squares[index] = i;
				index++;
			}
			else if (pos.piece_on_square(static_cast<square>(i)) == 10)
			{
				pieces[index] = 12;
				squares[index] = i;
				index++;
			}
			else if (pos.piece_on_square(static_cast<square>(i)) == 11)
			{
				pieces[index] = 11;
				squares[index] = i;
				index++;
			}
			else if (pos.piece_on_square(static_cast<square>(i)) == 12)
			{
				pieces[index] = 10;
				squares[index] = i;
				index++;
			}
			else if (pos.piece_on_square(static_cast<square>(i)) == 13)
			{
				pieces[index] = 9;
				squares[index] = i;
				index++;
			}
			else if (pos.piece_on_square(static_cast<square>(i)) == 14)
			{
				pieces[index] = 8;
				squares[index] = i;
				index++;
			}
		}

		const int nnue_score = nnue_evaluate(pos.on_move(), pieces, squares);
		return nnue_score;
	}

	int eval(const position& pos)
	{
		const int nnue_score = eval_nnue(pos);
		return nnue_score;
	}

	int eval_after_null_move(const int eval)
	{
		const auto result = -eval + 2 * value_tempo;
		return result;
	}
}
