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
#include <string>

constexpr auto program = "Fire";
inline std::string version = "";
constexpr auto author = "N. Schmidt";
constexpr auto platform = "x64";

// specify correct bit manipulation instruction set constant, as this will be appended
// to the fully distinguished engine name after platform
#ifdef USE_PEXT
constexpr auto bmis = "bmi2";
constexpr bool use_pext = true;
#else
#ifdef USE_AVX2
constexpr auto bmis = "avx2";
constexpr bool use_pext = false;
#else
constexpr auto bmis = "sse41";
constexpr bool use_pext = false;
#endif
#endif

// many new instructions require data that's aligned to 16-byte boundaries, so 64-byte alignment improves performance
#ifdef _MSC_VER
#define CACHE_ALIGN __declspec(align(64))
#else
#define CACHE_ALIGN __attribute__ ((aligned(64)))
#endif

#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
#include <xmmintrin.h>
#endif

// increase speed by having the compiler prefetch data from memory
inline void prefetch(void* address)
{
#ifdef __INTEL_COMPILER
	__asm__("");
#endif

#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
	_mm_prefetch(static_cast<char*>(address), _MM_HINT_T0);
#else
	__builtin_prefetch(address);
#endif
}

inline void prefetch2(void* address)
{
#ifdef __INTEL_COMPILER
	__asm__("");
#endif

#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
	_mm_prefetch(static_cast<char*>(address), _MM_HINT_T0);
	_mm_prefetch(static_cast<char*>(address) + 64, _MM_HINT_T0);
#else
	__builtin_prefetch(address);
	__builtin_prefetch((char*)address + 64);
#endif
}


