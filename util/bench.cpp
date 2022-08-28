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

#include <sstream>
#include <fstream>
#include "bench.h"

#include "../thread.h"
#include "../uci.h"
#include "util.h"

// search 64 positions (startposition, 20 openings from ECO, 20 middlegame, 20 endgames from ECE, & 3 FRC start positions)
// to depth 16 and write a time-stamped results file to disk
// this is an extremely useful function during development and code optimization efforts...to measure speedups and/or slowdowns
// it can be started via command line 'bench', or via Bench button in your GUI's UCI dialog
void bench(const int depth)
{
	static char file_name[64]{};
	char buf[32]{};

	uint64_t nodes = 0;
	auto pos_num = 0;
	auto num_positions = 64;
	position pos{};

	// start bench
	const auto start_time = now();

	for (auto& bench_position : bench_positions)
	{
		pos_num++;
		search::reset();
		auto s_depth = "depth " + std::to_string(depth);
		std::istringstream is(s_depth);
		pos.set(bench_position, false, thread_pool.main());
		acout() << "position " << pos_num << '/' << num_positions << " " << bench_position << std::endl;
		acout() << pos << std::endl;
		go(pos, is);
		thread_pool.main()->wait_for_search_to_end();
		nodes += thread_pool.visited_nodes();
	}
	uci_chess960 = true;
	for (auto& bench_position : frc_bench_positions)
	{
		pos_num++;
		search::reset();
		auto s_depth = "depth " + std::to_string(depth);
		std::istringstream is(s_depth);
		pos.set(bench_position, true, thread_pool.main());
		acout() << "chess960 position " << pos_num << '/' << num_positions << " " << bench_position << std::endl;
		acout() << pos << std::endl;
		go(pos, is);
		thread_pool.main()->wait_for_search_to_end();
		nodes += thread_pool.visited_nodes();
	}
	uci_chess960 = false;
	const auto elapsed_time = static_cast<double>(now() + 1 - start_time) / 1000;
	const auto nps = static_cast<double>(nodes) / elapsed_time;
	const auto ttd = elapsed_time / num_positions;

	// end bench

	// output results
	acout() << "depth " << depth << std::endl;
	acout() << "nodes " << nodes << std::endl;

	std::ostringstream ss;

	ss.precision(2);
	ss << "time " << std::fixed << elapsed_time << " secs" << std::endl;
	acout() << ss.str();
	ss.str(std::string());

	ss.precision(0);
	ss << "nps " << std::fixed << nps << std::endl;
	acout() << ss.str();
	ss.str(std::string());

	ss.precision(2);
	ss << "ttd " << std::fixed << ttd << " secs" << std::endl;
	acout() << ss.str();
	ss.str(std::string());

	// calculate time stamp for file name
	auto now = time(nullptr);
	strftime(buf, 32, "%b-%d_%H-%M", localtime(&now));

	// copy time stamp string to file name
	sprintf(file_name, "bench_%s.txt", buf);
	acout() << "\nsaved " << file_name << std::endl << std::endl;

	// create stream & open log file for writing
	std::ofstream bench_log;
	bench_log.open(file_name);

	// write formatted system info and bench results to log file
	bench_log << version;
	bench_log << "depth " << depth << std::endl;
	bench_log << "nodes " << nodes << std::endl;
	bench_log << "time " << std::fixed << std::setprecision(2) << elapsed_time << " secs" << std::endl;
	bench_log << "nps " << std::fixed << std::setprecision(0) << nps << std::endl;
	bench_log << "ttd " << std::fixed << std::setprecision(2) << ttd << " secs" << std::endl;

	bench_log.close();
	new_game();
}
