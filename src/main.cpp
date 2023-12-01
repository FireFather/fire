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
#include "util.h"
#include "uci.h"
int main(const int argc, char* argv[])
{
	acout() << util::engine_info() << std::endl; // display engine name, version, platform, and bmis
	acout() << util::build_date(); // display build date/time
	acout() << util::compiler_info() << std::endl << std::endl; // display compiler info
	acout() << util::core_info(); // display logical cores
	init(default_hash); // initialize using default hash (64 MB)
	uci_loop(argc, argv); // enter infinite loop and parse for input
	return 0;
}
