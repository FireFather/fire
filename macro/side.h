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

// this macro specifies the basic arithmetic operator + (addition) on enumerated side values
inline side operator+(const side c1, const side c2)
{
	return static_cast<side>(static_cast<int>(c1) + static_cast<int>(c2));
}

// this macro specifies the basic arithmetic operator - (subtraction) on enumerated side values
inline side operator-(const side c1, const side c2)
{
	return static_cast<side>(static_cast<int>(c1) - static_cast<int>(c2));
}

// this macro specifies the basic arithmetic operator * (multiplication) on enumerated side values
inline side operator*(const int i, const side c)
{
	return static_cast<side>(i * static_cast<int>(c));
}

// this macro specifies the basic arithmetic operator ++ (post-increment) on enumerated side values
inline side& operator++(side& c)
{
	return c = static_cast<side>(static_cast<int>(c) + 1);
}
