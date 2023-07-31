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
#if defined(_MSC_VER)
#include <bit>
#endif
#include "fire.h"
// the C/C++ AVX intrinsic functions for x86 processors are in the header "immintrin.h".
// the corresponding Intel® AVX2 instruction is PEXT (Parallel Bits Extract)
#ifdef USE_PEXT
#include <immintrin.h>
inline uint64_t pext(const uint64_t occupied, const uint64_t mask) { return _pext_u64(occupied, mask); }
#endif
#if defined(_MSC_VER)
inline int popcnt(const uint64_t bb) { return std::popcount(bb); }
inline square lsb(const uint64_t bb) { return static_cast<square>(std::countr_zero(bb)); }
inline square msb(const uint64_t bb) { unsigned long idx; _BitScanReverse64(&idx, bb); return static_cast<square>(idx); }
inline void prefetch(void* address) { _mm_prefetch(static_cast<char*>(address), _MM_HINT_T0); }
#define CACHE_ALIGN __declspec(align(64))
#elif defined(__GNUC__)
inline int popcnt(uint64_t bb) { return __builtin_popcountll(bb); }
inline square lsb(uint64_t bb) { return static_cast<square>(__builtin_ctzll(bb)); }
inline square msb(uint64_t bb) { return static_cast<square>(63 ^ __builtin_clzll(bb)); }
inline void prefetch(void* address) { __builtin_prefetch(address); }
#define CACHE_ALIGN __attribute__ ((aligned(64)))
#endif
