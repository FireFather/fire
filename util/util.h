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
#include <ostream>
#include <iostream>
#include <mutex>

#include "../position.h"

static std::mutex mutex_cout;

// asynchronous output w/ mutex
struct acout
{
	std::unique_lock<std::mutex> lk;
	acout()
		:
		lk(std::unique_lock(mutex_cout))
	{}

	template<typename T>
	acout& operator<<(const T& _t)
	{
		std::cout << _t;
		return *this;
	}

	acout& operator<<(std::ostream& (*fp)(std::ostream&))
	{
		std::cout << fp;
		return *this;
	}
};

namespace util
{
	const std::string piece_to_char(" KPNBRQ  kpnbrq");

	std::string engine_info();
	std::string build_date();
	std::string engine_author();
	std::string core_info();
	std::string compiler_info();
	std::string move_to_string(uint32_t move, const position& pos);
	uint32_t move_from_string(const position& pos, std::string& str);

	// xorshift64star Pseudo-Random Number Generator
	// This class is based on original code written and dedicated
	// to the public domain by Sebastiano Vigna (2014).

	class random
	{
		uint64_t s_;

		uint64_t rand64()
		{
			s_ ^= s_ >> 12, s_ ^= s_ << 25, s_ ^= s_ >> 27;
			return s_ * 2685821657736338717LL;
		}

	public:
		explicit random(const uint64_t seed) : s_(seed)
		{
			assert(seed);
		}

		template <typename t>
		t rand()
		{
			return t(rand64());
		}
	};
}
std::ostream& operator<<(std::ostream& os, const position& pos);