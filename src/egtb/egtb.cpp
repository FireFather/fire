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

#include "tbprobe.h"
#include "../search.h"
#include "../util/util.h"

// syzygy initialization and probe functions
namespace egtb
{
	int max_pieces_wdl = 0;
	int max_pieces_dtz = 0;
	int max_pieces_dtm = 0;

	egtb_probe egtb_probe_wdl = nullptr;
	egtb_probe egtb_probe_dtm = nullptr;
	egtb_probe egtb_probe_dtz = nullptr;

	bool use_rule50;

	static bool syzygy_in_use = false;
	static std::string current_syzygy_path;

	//probe distance-to-zero
	int tb_probe_dtz(position& pos)
	{
		auto success = 0;

		const auto val = syzygy_probe_dtz(pos, &success);

		if (success == 0)
			return no_score;

		pos.increase_tb_hits();

		const auto move50 = pos.fifty_move_counter();

		if (val > 0 && (!use_rule50 || move50 + val <= 100))
			return std::max(longest_mate_score - (pos.info()->ply + val), egtb_win_score + score_1);
		if (val < 0 && (!use_rule50 || move50 - val <= 100))
			return std::min(-longest_mate_score + (pos.info()->ply - val), -egtb_win_score - score_1);
		return draw_score;
	}

	// probe win-loss-draw
	int tb_probe_wdl(position& pos)
	{
		auto success = 0;

		const auto val = syzygy_probe_wdl(pos, &success);

		if (success == 0)
			return no_score;

		pos.increase_tb_hits();

		const auto draw_value = use_rule50 ? 1 : 0;
		if (val > draw_value)
			return longest_mate_score - pos.info()->ply;
		if (val < -draw_value)
			return -longest_mate_score + pos.info()->ply;
		return draw_value;
	}

	void syzygy_init(const std::string& path)
	{
		const auto path_specified = !path.empty() && path != "<empty>";

		if (path_specified)
		{
			if (path != current_syzygy_path)
			{
				syzygy_path_init(path);
				current_syzygy_path = path;
			}

			max_pieces_wdl = max_pieces_dtz = tb_max_men;

			if (max_pieces_wdl)
			{
				syzygy_in_use = true;
				egtb_probe_wdl = &tb_probe_wdl;
				egtb_probe_dtz = &tb_probe_dtz;

				// notify the GUI via UCI if TBs are found, and how many
				acout() << "info string Found " << tb_num_piece + tb_num_pawn << " tablebases" << std::endl;
				return;
			}
		}
		if (path_specified || syzygy_in_use)
		{
			syzygy_in_use = false;
			max_pieces_wdl = max_pieces_dtz = 0;
			egtb_probe_wdl = nullptr;
			egtb_probe_dtz = nullptr;
		}
	}
}