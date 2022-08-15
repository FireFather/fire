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
#include "define.h"
#include "fire.h"

enum hashflags : uint8_t
{
	no_limit,
	threat_white = 1,
	threat_black = 2,
	in_use = 4,
	south_border = 64,
	north_border = 128,
	exact_value = north_border | south_border
};

constexpr uint8_t age_mask = 0x38;
constexpr uint8_t flags_mask = 0xc7;
constexpr uint8_t threat_mask = 0x03;
constexpr uint8_t use_mask = 0xfb;

struct main_hash_entry
{
	[[nodiscard]] uint32_t move() const
	{
		return move_;
	}

	[[nodiscard]] int value() const
	{
		return value_;
	}

	[[nodiscard]] int eval() const
	{
		return eval_;
	}

	[[nodiscard]] int depth() const
	{
		return depth_ * static_cast<int>(plies) + plies - 1;
	}

	[[nodiscard]] hashflags bounds() const
	{
		return static_cast<hashflags>(flags_ & exact_value);
	}

	[[nodiscard]] hashflags threat() const
	{
		return static_cast<hashflags>(flags_ & threat_mask);
	}

	void save(const uint64_t k, const int val, const uint8_t flags, const int d, const uint32_t z, const int eval, const uint8_t gen)
	{
		const auto dd = d / plies;
		const uint16_t k16 = k >> 48;
		if (z || k16 != key_)
			move_ = static_cast<uint16_t>(z);

		if (k16 != key_
			|| dd > depth_ - 4
			|| (flags & exact_value) == exact_value)
		{
			key_ = k16;
			value_ = static_cast<int16_t>(val);
			eval_ = static_cast<int16_t>(eval);
			flags_ = static_cast<uint8_t>(gen + flags);
			depth_ = static_cast<int8_t>(dd);
		}
	}

private:
	friend class hash;

	uint16_t key_;
	int8_t depth_;
	uint8_t flags_;
	int16_t value_;
	int16_t eval_;
	uint16_t move_;
};

class hash
{
	static constexpr int cache_line = 64;
	static constexpr int bucket_size = 3;

	struct bucket
	{
		main_hash_entry entry[bucket_size];
		char padding[2];
	};

	static_assert(cache_line % sizeof(bucket) == 0, "Cluster size incorrect");

public:
	~hash()
	{
		free(hash_mem_);
	}

	void new_age()
	{
		age_ = age_ + 8 & age_mask;
	}

	[[nodiscard]] uint8_t age() const
	{
		return age_;
	}
	[[nodiscard]] main_hash_entry* probe(uint64_t key) const;
	[[nodiscard]] main_hash_entry* replace(uint64_t key) const;
	[[nodiscard]] int hash_full() const;
	void init(size_t mb_size);
	void clear() const;

	[[nodiscard]] inline main_hash_entry* entry(const uint64_t key) const
	{
		return reinterpret_cast<main_hash_entry*>(reinterpret_cast<char*>(hash_mem_) + (key & bucket_mask_));
	}

	inline void prefetch_entry(const uint64_t key) const
	{
		prefetch(entry(key));
	}

private:
	size_t buckets_ = 0;
	size_t bucket_mask_ = 0;
	bucket* hash_mem_ = nullptr;
	uint8_t age_ = 0;
};

extern hash main_hash;
