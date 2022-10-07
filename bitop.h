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

#include "fire.h"

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#include <bit>
#endif

// the C/C++ AVX intrinsic functions for x86 processors are in the header "immintrin.h".
// the corresponding Intel® AVX2 instruction is PEXT (Parallel Bits Extract)
#ifdef USE_PEXT
#include <immintrin.h>
inline uint64_t pext(const uint64_t occupied, const uint64_t mask)
{
	return _pext_u64(occupied, mask);
}
#endif

// calculate the number of bits set to 1
inline int popcnt(const uint64_t b)
{
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
	return std::popcount(b);
#elif defined(__GNUC__)
	return __builtin_popcountll(b);
#endif
}

// search the mask data from least significant bit (LSB) to the most significant bit (MSB) for a set bit (1)
inline square lsb(const uint64_t b)
{
#if defined(_WIN64) && defined(_MSC_VER)
	return static_cast<square>(std::countr_zero(b));
#elif defined(__GNUC__)
	return square(__builtin_ctzll(b));
#endif
}

// search the mask data from most significant bit (MSB) to least significant bit (LSB) for a set bit (1)
inline square msb(const uint64_t b)
{
#if defined(_WIN64) && defined(_MSC_VER)
	unsigned long idx;
	_BitScanReverse64(&idx, b);
	return static_cast<square>(idx);
#elif defined(__GNUC__)
	return square(63 ^ __builtin_clzll(b));
#endif
}

// many new instructions require data that's aligned to 16-byte boundaries, so 64-byte alignment improves performance
#ifdef _MSC_VER
#define CACHE_ALIGN __declspec(align(64))
#else
#define CACHE_ALIGN __attribute__ ((aligned(64)))
#endif

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#include <xmmintrin.h>
#endif

// increase speed by having the compiler prefetch data from memory
inline void prefetch(void* address)
{
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
	_mm_prefetch(static_cast<char*>(address), _MM_HINT_T0);
#elif defined(__GNUC__)
	__builtin_prefetch(address);
#endif
}

inline void prefetch2(void* address)
{
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
	_mm_prefetch(static_cast<char*>(address), _MM_HINT_T0);
	_mm_prefetch(static_cast<char*>(address) + 64, _MM_HINT_T0);
#elif defined(__GNUC__)
	__builtin_prefetch(address);
	__builtin_prefetch((char*)address + 64);
#endif
}