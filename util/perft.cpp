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

#include <cinttypes>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "util.h"
#include "../fire.h"
#include "../position.h"
#include "../thread.h"
#include "../uci.h"

// count leaf nodes for all legal moves at a specific depth
static uint64_t perft(position& pos, const int depth) {

	uint64_t cnt = 0;
	const auto leaf = depth == 2;

	for (const auto& m : legal_move_list(pos))
	{
		pos.play_move(m, pos.give_check(m));
		cnt += leaf ? legal_move_list(pos).size() : perft(pos, depth - 1);
		pos.take_move_back(m);
	}
	return cnt;
}

uint64_t start_perft(position& pos, const int depth) {
	return depth > 1 ? perft(pos, depth) : legal_move_list(pos).size();
}

void perft(int depth, std::string& fen)
{
	char buf[32];

	uint64_t nodes = 0;

	if (depth < 1) depth = 1;

	if (fen.empty())
		fen = startpos;

	search::reset();

	// if 'perft.epd' is specified as 4th function parameter
	// read positions from that file
	if (static char file_name[64]; fen == "perft.epd")
	{
		std::fstream file;
		file.open("perft.epd", std::ios::in);

		if (file.is_open())
		{
			// calculate time stamp for file name
			auto now_ = time(nullptr);
			strftime(buf, 32, "%b-%d_%H-%M", localtime(&now_));

			// copy time stamp string to file name
			sprintf(file_name, "perft_%s.txt", buf);

			// create stream & open log file for writing
			std::ofstream perft_log;
			perft_log.open(file_name);

			// write system info to log file
			perft_log << version << std::endl;

			std::string fen_pos;

			while (getline(file, fen_pos))
			{
				position pos{};
				pos.set(fen_pos, false, thread_pool.main());
				acout() << pos;

				// start perft
				const auto start_time = now();
				const auto cnt = start_perft(pos, depth);
				nodes += cnt;
				const auto elapsed_time = static_cast<double>(now() + 1 - start_time) / 1000;
				const auto nps = static_cast<double>(nodes) / elapsed_time;
				// end perft

				// output data
				acout() << fen_pos.c_str() << std::endl;
				acout() << "depth " << depth << std::endl;
				acout() << "nodes " << nodes << std::endl;

				std::ostringstream ss;

				ss.precision(3);
				ss << "time " << std::fixed << elapsed_time << " secs" << std::endl;
				acout() << ss.str();
				ss.str(std::string());

				ss.precision(0);
				ss << "nps " << std::fixed << nps << std::endl << std::endl;
				acout() << ss.str();
				ss.str(std::string());

				// write formatted results to time stamped log file
				perft_log << "perft " << depth << std::endl;
				perft_log << fen_pos.c_str() << std::endl;
				perft_log << "nodes " << std::fixed << std::setprecision(0) << nodes << std::endl;
				perft_log << "time " << std::fixed << std::setprecision(2) << elapsed_time << " secs" << std::endl;
				perft_log << "nps " << std::fixed << std::setprecision(0) << nps << std::endl;
				perft_log << std::endl;
			}
			perft_log.close();
			acout() << "saved " << file_name << std::endl;
		}
	}
	else
	{
		// otherwise use the fen specified 
		position pos{};
		pos.set(fen, false, thread_pool.main());
		acout() << pos;
		acout() << "" << fen.c_str() << std::endl;
		acout() << "depth " << depth << std::endl;

		// start perft
		const auto start_time = now();
		const auto cnt = start_perft(pos, depth);
		nodes += cnt;
		const auto elapsed_time = static_cast<double>(now() + 1 - start_time) / 1000;
		const auto nps = static_cast<double>(nodes) / elapsed_time;
		// end perft

		// output data
		acout() << "nodes " << nodes << std::endl;

		std::ostringstream ss;

		ss.precision(3);
		ss << "time " << std::fixed << elapsed_time << " secs" << std::endl;
		acout() << ss.str();
		ss.str(std::string());

		ss.precision(0);
		ss << "nps " << std::fixed << nps << std::endl;
		acout() << ss.str();
		ss.str(std::string());

		// calculate time stamp for file name
		auto now = time(nullptr);
		strftime(buf, 32, "%b-%d_%H-%M", localtime(&now));

		// copy time stamp string to file name
		sprintf(file_name, "perft_%s.txt", buf);

		// create stream & open log file for writing
		std::ofstream perft_log;
		perft_log.open(file_name);

		// write formatted results to time stamped log file
		perft_log << version << std::endl;
		perft_log << "perft " << depth << std::endl;
		perft_log << fen.c_str() << std::endl;
		perft_log << "nodes " << std::fixed << std::setprecision(0) << nodes << std::endl;
		perft_log << "time " << std::fixed << std::setprecision(2) << elapsed_time << " secs" << std::endl;
		perft_log << "nps " << std::fixed << std::setprecision(0) << nps << std::endl;
		perft_log.close();
		acout() << "saved " << file_name << std::endl << std::endl;
	}
}

// divide is similar to perft but lists all moves possible and calculates the perft of the decremented depth for each
void divide(int depth, std::string& fen)
{
	char buf[32];
	uint64_t nodes = 0;

	if (depth < 1) depth = 1;

	search::reset();

	if (static char file_name[256]; fen == "perft.epd")
	{
		std::fstream file;
		file.open("perft.epd", std::ios::in);

		if (file.is_open())
		{
			// calculate time stamp for file name
			auto now_ = time(nullptr);
			strftime(buf, 32, "%b-%d_%H-%M", localtime(&now_));

			// copy time stamp string to file name
			sprintf(file_name, "divide_%s.txt", buf);

			// create stream & open log file for writing
			std::ofstream divide_log;
			divide_log.open(file_name);

			std::string fen_pos;

			while (getline(file, fen_pos))
			{
				position pos{};
				pos.set(fen_pos, false, thread_pool.main());
				acout() << pos;
				acout() << fen_pos.c_str() << std::endl;
				acout() << "depth " << depth << std::endl;

				// start perft/divide
				const auto start_time = now();
				for (const auto& m : legal_move_list(pos))
				{
					pos.play_move(m, pos.give_check(m));
					const auto cnt = depth > 1 ? start_perft(pos, depth - 1) : 1;
					pos.take_move_back(m);
					std::cerr << "" << util::move_to_string(m, pos) << " " << cnt << std::endl;
					nodes += cnt;
				}
				const auto elapsed_time = static_cast<double>(now() + 1 - start_time) / 1000;
				const auto nps = static_cast<double>(nodes) / elapsed_time;
				// end perft/divide

				// output data
				acout() << "nodes " << nodes << std::endl;

				std::ostringstream ss;

				ss.precision(3);
				ss << "time " << std::fixed << elapsed_time << " secs" << std::endl;
				acout() << ss.str();
				ss.str(std::string());

				ss.precision(0);
				ss << "nps " << std::fixed << nps << std::endl;
				acout() << ss.str();
				ss.str(std::string());
				acout() << std::endl;

				// write formatted results to time stamped log file
				divide_log << version << std::endl;
				divide_log << "divide " << depth << std::endl;
				divide_log << fen.c_str() << std::endl;
				divide_log << "nodes " << std::fixed << std::setprecision(0) << nodes << std::endl;
				divide_log << "time " << std::fixed << std::setprecision(2) << elapsed_time << " secs" << std::endl;
				divide_log << "nps " << std::fixed << std::setprecision(0) << nps << std::endl;

				// return for next position
			}
			divide_log.close();
			acout() << "saved " << file_name << std::endl;
		}
	}
	else
	{
		position pos{};
		pos.set(fen, false, thread_pool.main());
		acout() << pos;
		acout() << fen.c_str() << std::endl;
		acout() << "depth " << depth << std::endl;

		// start perft/divide
		const auto start_time = now();
		for (const auto& m : legal_move_list(pos))
		{
			pos.play_move(m, pos.give_check(m));
			const auto cnt = depth > 1 ? start_perft(pos, depth - 1) : 1;
			pos.take_move_back(m);
			std::cerr << "" << util::move_to_string(m, pos) << " " << cnt << std::endl;
			nodes += cnt;
		}
		const auto elapsed_time = static_cast<double>(now() + 1 - start_time) / 1000;
		const auto nps = static_cast<double>(nodes) / elapsed_time;
		// end perft/divide

		// output data
		acout() << "nodes " << nodes << std::endl;

		std::ostringstream ss;

		ss.precision(3);
		ss << "time " << std::fixed << elapsed_time << " secs" << std::endl;
		acout() << ss.str();
		ss.str(std::string());

		ss.precision(0);
		ss << "nps " << std::fixed << nps << std::endl;
		acout() << ss.str();
		ss.str(std::string());

		// calculate time stamp for file name
		auto now = time(nullptr);
		strftime(buf, 32, "%b-%d_%H-%M", localtime(&now));

		// copy time stamp string to file name
		sprintf(file_name, "divide_%s.txt", buf);

		// create stream & open log file for writing
		std::ofstream divide_log;
		divide_log.open(file_name);

		// write formatted results to time stamped log file
		divide_log << version << std::endl;
		divide_log << "divide " << depth << std::endl;
		divide_log << fen.c_str() << std::endl;
		divide_log << "nodes " << std::fixed << std::setprecision(0) << nodes << std::endl;
		divide_log << "time " << std::fixed << std::setprecision(2) << elapsed_time << " secs" << std::endl;
		divide_log << "nps " << std::fixed << std::setprecision(0) << nps << std::endl;
		divide_log.close();
		acout() << "saved " << file_name << std::endl << std::endl;
	}
}
