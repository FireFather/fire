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

// these macros specify basic arithmetic operations + (addition), - (subtraction), and ++ (post-increment) on enumerated file values
inline file operator+(const file f1, const file f2)
{
	return static_cast<file>(static_cast<int>(f1) + static_cast<int>(f2));
}

inline file operator-(const file f1, const file f2)
{
	return static_cast<file>(static_cast<int>(f1) - static_cast<int>(f2));
}

inline file& operator++(file& f)
{
	return f = static_cast<file>(static_cast<int>(f) + 1);
}
