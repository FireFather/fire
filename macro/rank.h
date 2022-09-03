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

#pragma once

// these macros specify basic arithmetic operations: + (addition), - (subtraction), ++ (post-increment) and -- (post-decrement) on enumerated rank values
inline rank operator+(const rank r1, const rank r2)
{
	return static_cast<rank>(static_cast<int>(r1) + static_cast<int>(r2));
}

inline rank operator-(const rank r1, const rank r2)
{
	return static_cast<rank>(static_cast<int>(r1) - static_cast<int>(r2));
}

inline rank& operator++(rank& r)
{
	return r = static_cast<rank>(static_cast<int>(r) + 1);
}

inline rank& operator--(rank& r)
{
	return r = static_cast<rank>(static_cast<int>(r) - 1);
}
