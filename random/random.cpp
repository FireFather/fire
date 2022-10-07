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

  Thanks to Yu Nasu, Hisayori Noda, this implementation adapted from R. De Man
  and Daniel Shaw's Cfish nnue probe code https://github.com/dshawul/nnue-probe
*/

#include <random>

#include "../movepick.h"
#include "../position.h"
#include "../pragma.h"
#include "../util/util.h"

// use uniform_real_distribution to generate all possible value and avoid statistical bias
void random(position& pos) {

	int num_moves = 0;

	// find & count legal moves
	for (const auto& m : legal_move_list(pos))
		num_moves++;

	// seed the generator	
	std::random_device rd;

	// use standard mersenne_twister_engine
	std::mt19937 gen(rd());

	// calculate a uniform distribution between 1 and num_moves + 1
	std::uniform_real_distribution<> distribution(1, num_moves + 1);

	// generate a random number using the distribution from above
	const int r = static_cast<int>(distribution(gen));

	// find the move
	num_moves = 0;
	for (const auto& m : legal_move_list(pos))
	{
		num_moves++;
		if (num_moves == r)
		{
			// play and output the move
			pos.play_move(m, pos.give_check(m));
			acout() << "bestmove " << util::move_to_string(m, pos) << std::endl;
		}
	}
}

