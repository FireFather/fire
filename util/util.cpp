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

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "../util/util.h"

#include <sstream> // std::stringstream

#include "../fire.h"
#include "../movegen.h"
#include "../position.h"
#include "../macro/file.h"
#include "../macro/rank.h"

namespace util
{
	const std::string months("Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec");
	std::string month, day, year;
	std::stringstream ei, bd, date(__DATE__);

	// return string with 'program' 'version' 'platform' and 'instruction set'

	std::string engine_info()
	{
		// specify correct bit manipulation instruction set constant, as this will be appended
		// to the fully distinguished engine name after platform
		#ifdef USE_PEXT
				static constexpr auto bmis = "bmi2";
		#else
		#ifdef USE_AVX2
				static constexpr auto bmis = "avx2";
		#else
				static constexpr auto bmis = "sse41";
		#endif
		#endif

		ei << program << " ";
		date >> month >> day >> year;
		ei << (1 + months.find(month) / 4) << day << year << " ";
		ei << platform << " " << bmis;
		version = ei.str();
		return ei.str();
	}

	// return string with 'build date'
	std::string build_date()
	{
		date >> month >> day >> year;
		bd << "\nBuild timestamp  : " << month << ' ' << std::setw(2) << std::setfill('0') << day << ' ' << year << ' ' << std::setw(2) << std::setfill('0') << __TIME__ << std::endl;
		return bd.str();
	}

	// return string with 'author'
	std::string engine_author()
	{
		std::stringstream ea;
		ea << author << std::endl;
		return ea.str();
	}

	// return # of cores
	std::string core_info()
	{
		std::stringstream ci;

#ifdef _WIN32
		// if windows
		SYSTEM_INFO sys_info;
		GetSystemInfo(&sys_info);
		ci << "info string " << sys_info.dwNumberOfProcessors << " available cores" << std::endl;
#else
		// if linux
		ci << "info string " << sysconf(_SC_NPROCESSORS_ONLN) << " available cores" << std::endl;
#endif

		return ci.str();
	}

	std::string compiler_info() {

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)
#define MAKE_VERSION_STRING(major, minor, patch) STRINGIFY(major) "." STRINGIFY(minor) "." STRINGIFY(patch)

		std::string compiler = "Compiled using   : ";

#ifdef __clang__
		compiler += "clang++ ";
		compiler += MAKE_VERSION_STRING(__clang_major__, __clang_minor__, __clang_patchlevel__);

#elif __INTEL_COMPILER
		compiler += "Intel compiler ";
		compiler += "(version ";
		compiler += STRINGIFY(__INTEL_COMPILER) " update " STRINGIFY(__INTEL_COMPILER_UPDATE);
		compiler += ")";

#elif _MSC_VER
		compiler += "MSVC ";
		compiler += "(version ";
		compiler += STRINGIFY(_MSC_FULL_VER) "." STRINGIFY(_MSC_BUILD);
		compiler += ")";

#elif defined(__e2k__) && defined(__LCC__)
#define dot_ver2(n) \
		compiler += (char)'.'; \
		compiler += (char)('0' + (n) / 10); \
		compiler += (char)('0' + (n) % 10);

		compiler += "MCST LCC ";
		compiler += "(version ";
		compiler += std::to_string(__LCC__ / 100);
		dot_ver2(__LCC__ % 100)
			dot_ver2(__LCC_MINOR__)
			compiler += ")";

#elif __GNUC__
		compiler += "g++ (GNUC) ";
		compiler += MAKE_VERSION_STRING(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
		compiler += "Unknown compiler ";
		compiler += "(unknown version)";
#endif

#if defined(__APPLE__)
		compiler += " on Apple";

#elif defined(__CYGWIN__)
		compiler += " on Cygwin";

#elif defined(__MINGW64__)
		compiler += " on MinGW64";

#elif defined(__MINGW32__)
		compiler += " on MinGW32";

#elif defined(__ANDROID__)
		compiler += " on Android";

#elif defined(__linux__)
		compiler += " on Linux";

#elif defined(_WIN64)
		compiler += " on Microsoft Windows 64-bit";

#elif defined(_WIN32)
		compiler += " on Microsoft Windows 32-bit";
#else
		compiler += " on unknown system";
#endif

		compiler += "\nCompile settings :";

#if defined(_WIN64)
		compiler += " 64-bit";
#else
		compiler += " 32-bit";
#endif

#if defined(USE_VNNI)
		compiler += " VNNI";
#endif

#if defined(USE_AVX512)
		compiler += " AVX512";
#endif

#if defined(USE_PEXT)
		compiler += " BMI2";
#endif

#if defined(USE_AVX2)
		compiler += " AVX2";
#endif

#if defined(USE_SSE41)
		compiler += " SSE41";
#endif

#if defined(USE_SSSE3)
		compiler += " SSSE3";
#endif

#if defined(USE_SSE2)
		compiler += " SSE2";
#endif

#if defined(USE_POPCNT)
		compiler += " POPCNT";
#endif

#if defined(USE_MMX)
		compiler += " MMX";
#endif

#if defined(USE_NEON)
		compiler += " NEON";
#endif

#if !defined(NDEBUG)
		compiler += " DEBUG";
#endif

		return compiler;
	}

	// convert from internal move format to ascii
	std::string move_to_string(const uint32_t move, const position& pos)
	{
		char s_move[6]{};

		const auto from = from_square(move);
		auto to = to_square(move);

		if (move == no_move || move == null_move)
			return "";

		if (move_type(move) == castle_move && pos.is_chess960())
			to = pos.castle_rook_square(to);

		s_move[0] = 'a' + static_cast<char>(file_of(from));
		s_move[1] = '1' + static_cast<char>(rank_of(from));
		s_move[2] = 'a' + static_cast<char>(file_of(to));
		s_move[3] = '1' + static_cast<char>(rank_of(to));

		if (move < static_cast<uint32_t>(promotion_p))
			return std::string(s_move, 4);

		s_move[4] = "   nbrq"[promotion_piece(move)];
		return std::string(s_move, 5);
	}

	// convert from ascii string to internal move format
	uint32_t move_from_string(const position& pos, std::string& str)
	{
		if (pos.is_chess960())
		{
			if (str == "O-O")
				str = move_to_string(make_move(castle_move, pos.king(pos.on_move()), relative_square(pos.on_move(), g1)), pos);
			else if (str == "O-O-O")
				str = move_to_string(make_move(castle_move, pos.king(pos.on_move()), relative_square(pos.on_move(), c1)), pos);
		}

		if (str.length() == 5)
			str[4] = static_cast<char>(tolower(str[4]));

		for (const auto& new_move : legal_move_list(pos))
			if (str == move_to_string(new_move, pos))
				return new_move;

		return no_move;
	}
}

// display ascii representation of position (for use in bench and perft)
std::ostream& operator<<(std::ostream& os, const position& pos)
{
	constexpr char p_chars[] =
	{
	'K','P','N','B','R','Q',
	'k','p','n','b','r','q',
	};

	auto found = false;

	for (auto r = rank_8; r >= rank_1; --r)
	{
		for (auto f = file_a; f <= file_h; ++f)
		{
			const auto pc = util::piece_to_char[pos.piece_on_square(make_square(f, r))];
			for (auto i = 0; i <= 11; ++i)
			{
				if (pc == p_chars[i])
				{
					found = true;
					break;
				}
			}
			if (found)
				os << " " << pc;
			else
				os << " " << ".";
			found = false;
		}
		os << "\n";
	}
	return os;
}