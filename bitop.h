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

// the C/C++ AVX intrinsic functions for x86 processors are in the header "immintrin.h".
// the corresponding Intel® AVX2 instruction is PEXT (Parallel Bits Extract)
#ifdef USE_PEXT
#include <immintrin.h>
inline uint64_t pext(const uint64_t occupied, const uint64_t mask)
{
	return _pext_u64(occupied, mask);
}
#endif

#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
#include <nmmintrin.h>
#endif
// calculate the number of bits set to 1
inline int popcnt(const uint64_t b)
{
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
	return static_cast<int>(_mm_popcnt_u64(b));
#else
	return __builtin_popcountll(b);
#endif
}

#if defined(_WIN64) && defined(_MSC_VER)
#include <intrin.h>
// search the mask data from least significant bit (LSB) to the most significant bit (MSB) for a set bit (1)
// this is a 64-bit Microsoft specific intrinsic
inline square lsb(const uint64_t b)
{
	unsigned long idx;
	_BitScanForward64(&idx, b);
	return static_cast<square>(idx);
}
// search the mask data from most significant bit (MSB) to least significant bit (LSB) for a set bit (1)
// this is a 64-bit Microsoft specific intrinsic
inline square msb(const uint64_t b)
{
	unsigned long idx;
	_BitScanReverse64(&idx, b);
	return static_cast<square>(idx);
}
#elif defined(__GNUC__)
// corresponding intrinsic to _BitScanForward64 for linux
inline square lsb(uint64_t b)
{
	return square(__builtin_ctzll(b));
}
// corresponding intrinsic to _BitScanReverse64 for linux
// __builtin_ctzll calculated XOR (bitwise exclusive OR)
inline square msb(uint64_t b)
{
	return square(63 ^ __builtin_clzll(b));
}
#endif