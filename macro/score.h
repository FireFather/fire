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

// these macros specify basic arithmetic operations on enumerated score values
inline score operator+(const score s1, const score s2)
{
	return static_cast<score>(static_cast<int>(s1) + static_cast<int>(s2));
}

inline score operator-(const score s1, const score s2)
{
	return static_cast<score>(static_cast<int>(s1) - static_cast<int>(s2));
}

inline score operator*(const int i, const score s)
{
	return static_cast<score>(i * static_cast<int>(s));
}

inline score operator*(const score s, const int i)
{
	return static_cast<score>(static_cast<int>(s) * i);
}

inline score& operator+=(score& s1, const score s2)
{
	return s1 = s1 + s2;
}

inline score& operator-=(score& s1, const score s2)
{
	return s1 = s1 - s2;
}
