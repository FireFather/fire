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

#include "pawn.h"

#include "bitboard.h"
#include "fire.h"
#include "position.h"
#include "thread.h"

// pawn hash table
namespace pawn
{
	pawn_hash_entry* probe(const position& pos)
	{
		const auto key = pos.pawn_key();
		auto* entry = pos.thread_info()->pawn_table[key];

		if (entry->key == key)
			return entry;

		entry->key = key;

		const uint64_t pawn_files_bb[2]{};

		const auto w_pawn = pos.pieces(white, pt_pawn);
		const auto b_pawn = pos.pieces(black, pt_pawn);

		entry->p_attack[white] = pawn_attack<white>(w_pawn);
		entry->p_attack[black] = pawn_attack<black>(b_pawn);

		entry->half_open_lines[white] = static_cast<int>(~pawn_files_bb[white]) & 0xFF;
		entry->half_open_lines[black] = static_cast<int>(~pawn_files_bb[black]) & 0xFF;

		entry->asymmetry = popcnt(entry->half_open_lines[white] ^ entry->half_open_lines[black]);


		auto files = static_cast<int>((pawn_files_bb[white] | pawn_files_bb[black]) & 0xFF);
		if (files)
			files = msb(files) - lsb(files);
		entry->file_width = std::max(files - 3, 0);

		entry->conversion_difficult = !((entry->half_open_lines[white] & entry->half_open_lines[black])
			|| entry->half_open_lines[white] & entry->half_open_lines[black] & 0x3c) && !entry->asymmetry;

		auto pawns_bb = pos.pieces(pt_pawn);
		entry->n_pawns = popcnt(pawns_bb);
		if (pawns_bb)
		{
			auto line_sum = 0;
			do
			{
				const auto sq = pop_lsb(&pawns_bb);
				line_sum += sq & 7;
			} while (pawns_bb);
			entry->average_line = line_sum / entry->n_pawns;
		}

		return entry;
	}
}
