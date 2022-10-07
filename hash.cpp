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

#include "hash.h"

#include <iostream>

#include "bitboard.h"
#include "fire.h"

hash main_hash;

// set hash size in MB
void hash::init(const size_t mb_size)
{
	const auto new_size = static_cast<size_t>(1) << msb(mb_size * 1024 * 1024 / sizeof(bucket));

	if (new_size == buckets_)
		return;

	buckets_ = new_size;

	if (hash_mem_)
		free(hash_mem_);

	hash_mem_ = static_cast<bucket*>(calloc(buckets_ * sizeof(bucket) + 63, 1));

	if (!hash_mem_)
	{
		std::cerr << "Failed to allocate " << mb_size
			<< "MB for transposition table." << std::endl;
		exit(EXIT_FAILURE);
	}

	buckets_ = new_size;
	bucket_mask_ = (buckets_ - 1) * sizeof(bucket);
}

// reset allocated memory to 0
void hash::clear() const
{
	std::memset(hash_mem_, 0, buckets_ * sizeof(bucket));
}

// probe exiting entries transposition table
main_hash_entry* hash::probe(const uint64_t key) const
{
	auto* const hash_entry = entry(key);
	const uint16_t key16 = key >> 48;

	for (auto i = 0; i < bucket_size; ++i)
		if (hash_entry[i].key_ == key16)
		{
			if ((hash_entry[i].flags_ & age_mask) != age_)
				hash_entry[i].flags_ = static_cast<uint8_t>(age_ + (hash_entry[i].flags_ & flags_mask));

			return &hash_entry[i];
		}

	return nullptr;
}

// overwrite existing entry if aged
main_hash_entry* hash::replace(const uint64_t key) const
{
	auto* const hash_entry = entry(key);
	const uint16_t key16 = key >> 48;

	for (auto i = 0; i < bucket_size; ++i)
		if (hash_entry[i].key_ == 0 || hash_entry[i].key_ == key16)
			return &hash_entry[i];

	auto* replacement = hash_entry;
	for (auto i = 1; i < bucket_size; ++i)
		if (replacement->depth_ - (age_ - (replacement->flags_ & age_mask) & age_mask)
			> hash_entry[i].depth_ - (age_ - (hash_entry[i].flags_ & age_mask) & age_mask))
			replacement = &hash_entry[i];

	return replacement;
}

// send hash memory usage info to UCI GUI
int hash::hash_full() const
{
	constexpr auto i_max = 999 / bucket_size + 1;
	auto cnt = 0;
	for (auto i = 0; i < i_max; i++)
	{
		const main_hash_entry* hash_entry = &hash_mem_[i].entry[0];
		for (auto j = 0; j < bucket_size; j++)
			if (hash_entry[j].key_ && (hash_entry[j].flags_ & age_mask) == age_)
				cnt++;
	}
	return cnt * 1000 / (i_max * bucket_size);
}
