/*
  Copyright (c) 2011-2013 Ronald de Man
  This file may be redistributed and/or modified without restrictions.

  tbcore.c contains engine-independent routines of the tablebase probing code.
  This file should not need too much adaptation to add tablebase probing to
  a particular engine, provided the engine is written in C or C++.
*/

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../position.h"
#include "../zobrist.h"
#include "../movegen.h"
#include "../bitboard.h"
#include "../pragma.h"

#include "tbprobe.h"
#include "tbcore.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/mman.h>
#endif

#if !defined(_MSC_VER)
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

constexpr auto tb_max_piece = 254;
constexpr auto tb_max_pawn = 256;
constexpr auto hash_max = 7;

#define SWAP(a,b) {int tmp=a;(a)=b;(b)=tmp;}

constexpr auto tb_pawn = 1;
constexpr auto tb_knight = 2;
constexpr auto tb_bishop = 3;
constexpr auto tb_rook = 4;
constexpr auto tb_queen = 5;
constexpr auto tb_king = 6;

#define TB_W_PAWN tb_pawn
#define TB_B_PAWN (tb_pawn | 8)

static LOCK_T tb_mutex;

static bool initialized = false;
static int num_paths = 0;
static char* path_string = nullptr;
static char** paths = nullptr;

int tb_num_piece, tb_num_pawn, tb_max_men;
static tb_entry_piece tb_piece[tb_max_piece];
static tb_entry_pawn tb_pawns[tb_max_pawn];

static tb_hash_entry tb_hash[1 << tb_hash_bits][hash_max];

constexpr auto dtz_entries = 64;

static dtz_table_entry dtz_table[dtz_entries];

static void init_indices();
static uint64 calc_key_from_pcs(const int* pcs, int mirror);
static void free_wdl_entry(tb_entry* entry);
static void free_dtz_entry(tb_entry* entry);

static FD open_tb(const char* str, const char* suffix)
{
	FD fd;

	for (auto i = 0; i < num_paths; i++) {
		char file[256];
		strcpy(file, paths[i]);
		strcat(file, "/");
		strcat(file, str);
		strcat(file, suffix);
#ifndef _WIN32
		fd = open(file, O_RDONLY);
#else
		fd = CreateFile(file, GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
#endif
		if (fd != FD_ERR) return fd;
	}
	return FD_ERR;
}

static void close_tb(FD const fd)
{
#ifndef _WIN32
	close(fd);
#else
	CloseHandle(fd);
#endif
}

static char* map_file(const char* name, const char* suffix, uint64* mapping)
{
	FD const fd = open_tb(name, suffix);
	if (fd == FD_ERR)
		return nullptr;
#ifndef _WIN32
	struct stat statbuf {};
	fstat(fd, &statbuf);
	*mapping = statbuf.st_size;
	char* data = (char*)mmap(NULL, statbuf.st_size, PROT_READ,
		MAP_SHARED, fd, 0);
	if (data == (char*)(-1)) {
		printf("Could not mmap() %s.\n", name);
		exit(1);
	}
#else
	DWORD size_high;
	auto const size_low = GetFileSize(fd, &size_high);
	//  *size = ((uint64)size_high) << 32 | ((uint64)size_low);
	auto* map = CreateFileMapping(fd, nullptr, PAGE_READONLY, size_high, size_low,
		nullptr);
	if (map == nullptr) {
		printf("CreateFileMapping() failed.\n");
		exit(1);
	}
	*mapping = reinterpret_cast<uint64>(map);
	auto* const data = static_cast<char*>(MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0));
	if (data == nullptr) {
		printf("MapViewOfFile() failed, name = %s%s, error = %lu.\n", name, suffix, GetLastError());
		exit(1);
	}
#endif
	close_tb(fd);
	return data;
}

#ifndef _WIN32
static void unmap_file(char* data, uint64 size)
{
	if (!data) return;
	munmap(data, size);
}
#else
static void unmap_file(const char* data, const uint64 mapping)
{
	if (!data) return;
	UnmapViewOfFile(data);
	CloseHandle(reinterpret_cast<HANDLE>(mapping));
}
#endif

static void add_to_hash(tb_entry* ptr, const uint64 key)
{
	const uint64 hshidx = key >> (64 - tb_hash_bits);
	auto i = 0;
	while (i < hash_max && tb_hash[hshidx][i].ptr)
		i++;
	if (i == hash_max) {
		printf("HSHMAX too low!\n");
		exit(1);
	}
	tb_hash[hshidx][i].key = key;
	tb_hash[hshidx][i].ptr = ptr;
}

static char pchr[] = { 'K', 'Q', 'R', 'B', 'N', 'P' };

static void init_tb(char* str)
{
	tb_entry* entry = nullptr;
	int i = 0, pcs[16]{};

	FD fd = open_tb(str, wdl_suffix);
	if (fd == FD_ERR) return;
	close_tb(fd);

	for (i = 0; i < 16; i++)
		pcs[i] = 0;
	auto color = 0;
	for (auto* s = str; *s; s++)
		switch (*s)
		{
		case 'P':
			pcs[tb_pawn | color]++;
			break;
		case 'N':
			pcs[tb_knight | color]++;
			break;
		case 'B':
			pcs[tb_bishop | color]++;
			break;
		case 'R':
			pcs[tb_rook | color]++;
			break;
		case 'Q':
			pcs[tb_queen | color]++;
			break;
		case 'K':
			pcs[tb_king | color]++;
			break;
		case 'v':
			color = 0x08;
			break;
		default:
			break;
		}
	for (i = 0; i < 8; i++)
		if (pcs[i] != pcs[i + 8])
			break;
	const auto key = calc_key_from_pcs(pcs, 0);
	const auto key2 = calc_key_from_pcs(pcs, 1);
	if (pcs[TB_W_PAWN] + pcs[TB_B_PAWN] == 0) {
		if (tb_num_piece == tb_max_piece) {
			printf("TBMAX_PIECE limit too low!\n");
			exit(1);
		}
		entry = reinterpret_cast<tb_entry*>(&tb_piece[tb_num_piece++]);
	}
	else {
		if (tb_num_pawn == tb_max_pawn) {
			printf("TBMAX_PAWN limit too low!\n");
			exit(1);
		}
		entry = reinterpret_cast<tb_entry*>(&tb_pawns[tb_num_pawn++]);
	}
	entry->key = key;
	entry->ready = 0;
	entry->num = 0;
	for (i = 0; i < 16; i++)
		entry->num += static_cast<ubyte>(pcs[i]);
	entry->symmetric = (key == key2);
	entry->has_pawns = (pcs[TB_W_PAWN] + pcs[TB_B_PAWN] > 0);
	if (entry->num > tb_max_men)
		tb_max_men = entry->num;

	if (entry->has_pawns) {
		auto* ptr = reinterpret_cast<struct tb_entry_pawn*>(entry);
		ptr->pawns[0] = static_cast<ubyte>(pcs[TB_W_PAWN]);
		ptr->pawns[1] = static_cast<ubyte>(pcs[TB_B_PAWN]);
		if (pcs[TB_B_PAWN] > 0
			&& (pcs[TB_W_PAWN] == 0 || pcs[TB_B_PAWN] < pcs[TB_W_PAWN])) {
			ptr->pawns[0] = static_cast<ubyte>(pcs[TB_B_PAWN]);
			ptr->pawns[1] = static_cast<ubyte>(pcs[TB_W_PAWN]);
		}
	}
	else {
		int j = 0;
		auto* const ptr = reinterpret_cast<struct tb_entry_piece*>(entry);
		for (i = 0, j = 0; i < 16; i++)
			if (pcs[i] == 1) j++;
		if (j >= 3) ptr->enc_type = 0;
		else if (j == 2) ptr->enc_type = 2;
		else { /* only for suicide */
			j = 16;
			for (i = 0; i < 16; i++) {
				if (pcs[i] < j && pcs[i] > 1) j = pcs[i];
				ptr->enc_type = static_cast<ubyte>(1 + j);
			}
		}
	}
	add_to_hash(entry, key);
	if (key2 != key) add_to_hash(entry, key2);
}

void syzygy_path_init(const std::string& path)
{
	char str[16];
	auto i = 0, j = 0, k = 0, l = 0;

	if (initialized) {
		free(path_string);
		free(paths);
		tb_entry* entry = nullptr;
		for (i = 0; i < tb_num_piece; i++) {
			entry = reinterpret_cast<tb_entry*>(&tb_piece[i]);
			free_wdl_entry(entry);
		}
		for (i = 0; i < tb_num_pawn; i++) {
			entry = reinterpret_cast<tb_entry*>(&tb_pawns[i]);
			free_wdl_entry(entry);
		}
		for (i = 0; i < dtz_entries; i++)
			if (dtz_table[i].entry)
				free_dtz_entry(dtz_table[i].entry);
	}
	else {
		init_indices();
		initialized = true;
	}

	const auto* const p = path.c_str();
	if (strlen(p) == 0 || !strcmp(p, "<empty>")) return;
	path_string = static_cast<char*>(malloc(strlen(p) + 1));
	if (path_string != nullptr)
		strcpy(path_string, p);
	num_paths = 0;
	for (i = 0; i <= static_cast<int>(strlen(p)); i++) {
		if (path_string[i] != sep_char)
			num_paths++;
		while (path_string[i] && path_string[i] != sep_char)
			i++;
		if (!path_string[i]) break;
		path_string[i] = 0;
	}
	paths = static_cast<char**>(malloc(num_paths * sizeof(char*)));
	if (paths != nullptr)
	{
		for (i = j = 0; i < num_paths; i++) {
			while (!path_string[j]) j++;
			paths[i] = &path_string[j];
			while (path_string[j]) j++;
		}
	}
	LOCK_INIT(tb_mutex);

	tb_num_piece = tb_num_pawn = 0;
	tb_max_men = 0;

	for (i = 0; i < (1 << tb_hash_bits); i++)
		for (j = 0; j < hash_max; j++) {
			tb_hash[i][j].key = 0ULL;
			tb_hash[i][j].ptr = nullptr;
		}

	for (i = 0; i < dtz_entries; i++)
		dtz_table[i].entry = nullptr;

	for (i = 1; i < 6; i++) {
		sprintf(str, "K%cvK", pchr[i]);
		init_tb(str);
	}

	for (i = 1; i < 6; i++)
		for (j = i + 1; j < 6; j++) {
			sprintf(str, "K%cvK%c", pchr[i], pchr[j]);
			init_tb(str);
		}

	for (i = 1; i < 6; i++)
		for (j = i + 1; j < 6; j++) {
			sprintf(str, "K%c%cvK", pchr[i], pchr[j]);
			init_tb(str);
		}

	for (i = 1; i < 6; i++)
		for (j = i + 1; j < 6; j++)
			for (k = 1; k < 6; k++) {
				sprintf(str, "K%c%cvK%c", pchr[i], pchr[j], pchr[k]);
				init_tb(str);
			}

	for (i = 1; i < 6; i++)
		for (j = i + 1; j < 6; j++)
			for (k = j + 1; k < 6; k++) {
				sprintf(str, "K%c%c%cvK", pchr[i], pchr[j], pchr[k]);
				init_tb(str);
			}

	for (i = 1; i < 6; i++)
		for (j = i + 1; j < 6; j++)
			for (k = i + 1; k < 6; k++)
				for (l = (i == k) ? j : k; l < 6; l++) {
					sprintf(str, "K%c%cvK%c%c", pchr[i], pchr[j], pchr[k], pchr[l]);
					init_tb(str);
				}

	for (i = 1; i < 6; i++)
		for (j = i + 1; j < 6; j++)
			for (k = j + 1; k < 6; k++)
				for (l = 1; l < 6; l++) {
					sprintf(str, "K%c%c%cvK%c", pchr[i], pchr[j], pchr[k], pchr[l]);
					init_tb(str);
				}

	for (i = 1; i < 6; i++)
		for (j = i + 1; j < 6; j++)
			for (k = j + 1; k < 6; k++)
				for (l = k + 1; l < 6; l++) {
					sprintf(str, "K%c%c%c%cvK", pchr[i], pchr[j], pchr[k], pchr[l]);
					init_tb(str);
				}
}

static constexpr signed char offdiag[] = {
  0,-1,-1,-1,-1,-1,-1,-1,
  1, 0,-1,-1,-1,-1,-1,-1,
  1, 1, 0,-1,-1,-1,-1,-1,
  1, 1, 1, 0,-1,-1,-1,-1,
  1, 1, 1, 1, 0,-1,-1,-1,
  1, 1, 1, 1, 1, 0,-1,-1,
  1, 1, 1, 1, 1, 1, 0,-1,
  1, 1, 1, 1, 1, 1, 1, 0
};

static constexpr ubyte triangle[] = {
  6, 0, 1, 2, 2, 1, 0, 6,
  0, 7, 3, 4, 4, 3, 7, 0,
  1, 3, 8, 5, 5, 8, 3, 1,
  2, 4, 5, 9, 9, 5, 4, 2,
  2, 4, 5, 9, 9, 5, 4, 2,
  1, 3, 8, 5, 5, 8, 3, 1,
  0, 7, 3, 4, 4, 3, 7, 0,
  6, 0, 1, 2, 2, 1, 0, 6
};

static constexpr ubyte flipdiag[] = {
   0,  8, 16, 24, 32, 40, 48, 56,
   1,  9, 17, 25, 33, 41, 49, 57,
   2, 10, 18, 26, 34, 42, 50, 58,
   3, 11, 19, 27, 35, 43, 51, 59,
   4, 12, 20, 28, 36, 44, 52, 60,
   5, 13, 21, 29, 37, 45, 53, 61,
   6, 14, 22, 30, 38, 46, 54, 62,
   7, 15, 23, 31, 39, 47, 55, 63
};

static constexpr ubyte lower[] = {
  28,  0,  1,  2,  3,  4,  5,  6,
   0, 29,  7,  8,  9, 10, 11, 12,
   1,  7, 30, 13, 14, 15, 16, 17,
   2,  8, 13, 31, 18, 19, 20, 21,
   3,  9, 14, 18, 32, 22, 23, 24,
   4, 10, 15, 19, 22, 33, 25, 26,
   5, 11, 16, 20, 23, 25, 34, 27,
   6, 12, 17, 21, 24, 26, 27, 35
};

static constexpr ubyte diag[] = {
   0,  0,  0,  0,  0,  0,  0,  8,
   0,  1,  0,  0,  0,  0,  9,  0,
   0,  0,  2,  0,  0, 10,  0,  0,
   0,  0,  0,  3, 11,  0,  0,  0,
   0,  0,  0, 12,  4,  0,  0,  0,
   0,  0, 13,  0,  0,  5,  0,  0,
   0, 14,  0,  0,  0,  0,  6,  0,
  15,  0,  0,  0,  0,  0,  0,  7
};

static constexpr ubyte flap[] = {
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 6, 12, 18, 18, 12, 6, 0,
  1, 7, 13, 19, 19, 13, 7, 1,
  2, 8, 14, 20, 20, 14, 8, 2,
  3, 9, 15, 21, 21, 15, 9, 3,
  4, 10, 16, 22, 22, 16, 10, 4,
  5, 11, 17, 23, 23, 17, 11, 5,
  0, 0, 0, 0, 0, 0, 0, 0
};

static constexpr ubyte ptwist[] = {
  0, 0, 0, 0, 0, 0, 0, 0,
  47, 35, 23, 11, 10, 22, 34, 46,
  45, 33, 21, 9, 8, 20, 32, 44,
  43, 31, 19, 7, 6, 18, 30, 42,
  41, 29, 17, 5, 4, 16, 28, 40,
  39, 27, 15, 3, 2, 14, 26, 38,
  37, 25, 13, 1, 0, 12, 24, 36,
  0, 0, 0, 0, 0, 0, 0, 0
};

static constexpr ubyte invflap[] = {
  8, 16, 24, 32, 40, 48,
  9, 17, 25, 33, 41, 49,
  10, 18, 26, 34, 42, 50,
  11, 19, 27, 35, 43, 51
};

static constexpr ubyte file_to_file[] = {
  0, 1, 2, 3, 3, 2, 1, 0
};

static constexpr short kk_idx[10][64] = {
  { -1, -1, -1,  0,  1,  2,  3,  4,
	-1, -1, -1,  5,  6,  7,  8,  9,
	10, 11, 12, 13, 14, 15, 16, 17,
	18, 19, 20, 21, 22, 23, 24, 25,
	26, 27, 28, 29, 30, 31, 32, 33,
	34, 35, 36, 37, 38, 39, 40, 41,
	42, 43, 44, 45, 46, 47, 48, 49,
	50, 51, 52, 53, 54, 55, 56, 57 },
  { 58, -1, -1, -1, 59, 60, 61, 62,
	63, -1, -1, -1, 64, 65, 66, 67,
	68, 69, 70, 71, 72, 73, 74, 75,
	76, 77, 78, 79, 80, 81, 82, 83,
	84, 85, 86, 87, 88, 89, 90, 91,
	92, 93, 94, 95, 96, 97, 98, 99,
   100,101,102,103,104,105,106,107,
   108,109,110,111,112,113,114,115},
  {116,117, -1, -1, -1,118,119,120,
   121,122, -1, -1, -1,123,124,125,
   126,127,128,129,130,131,132,133,
   134,135,136,137,138,139,140,141,
   142,143,144,145,146,147,148,149,
   150,151,152,153,154,155,156,157,
   158,159,160,161,162,163,164,165,
   166,167,168,169,170,171,172,173 },
  {174, -1, -1, -1,175,176,177,178,
   179, -1, -1, -1,180,181,182,183,
   184, -1, -1, -1,185,186,187,188,
   189,190,191,192,193,194,195,196,
   197,198,199,200,201,202,203,204,
   205,206,207,208,209,210,211,212,
   213,214,215,216,217,218,219,220,
   221,222,223,224,225,226,227,228 },
  {229,230, -1, -1, -1,231,232,233,
   234,235, -1, -1, -1,236,237,238,
   239,240, -1, -1, -1,241,242,243,
   244,245,246,247,248,249,250,251,
   252,253,254,255,256,257,258,259,
   260,261,262,263,264,265,266,267,
   268,269,270,271,272,273,274,275,
   276,277,278,279,280,281,282,283 },
  {284,285,286,287,288,289,290,291,
   292,293, -1, -1, -1,294,295,296,
   297,298, -1, -1, -1,299,300,301,
   302,303, -1, -1, -1,304,305,306,
   307,308,309,310,311,312,313,314,
   315,316,317,318,319,320,321,322,
   323,324,325,326,327,328,329,330,
   331,332,333,334,335,336,337,338 },
  { -1, -1,339,340,341,342,343,344,
	-1, -1,345,346,347,348,349,350,
	-1, -1,441,351,352,353,354,355,
	-1, -1, -1,442,356,357,358,359,
	-1, -1, -1, -1,443,360,361,362,
	-1, -1, -1, -1, -1,444,363,364,
	-1, -1, -1, -1, -1, -1,445,365,
	-1, -1, -1, -1, -1, -1, -1,446 },
  { -1, -1, -1,366,367,368,369,370,
	-1, -1, -1,371,372,373,374,375,
	-1, -1, -1,376,377,378,379,380,
	-1, -1, -1,447,381,382,383,384,
	-1, -1, -1, -1,448,385,386,387,
	-1, -1, -1, -1, -1,449,388,389,
	-1, -1, -1, -1, -1, -1,450,390,
	-1, -1, -1, -1, -1, -1, -1,451 },
  {452,391,392,393,394,395,396,397,
	-1, -1, -1, -1,398,399,400,401,
	-1, -1, -1, -1,402,403,404,405,
	-1, -1, -1, -1,406,407,408,409,
	-1, -1, -1, -1,453,410,411,412,
	-1, -1, -1, -1, -1,454,413,414,
	-1, -1, -1, -1, -1, -1,455,415,
	-1, -1, -1, -1, -1, -1, -1,456 },
  {457,416,417,418,419,420,421,422,
	-1,458,423,424,425,426,427,428,
	-1, -1, -1, -1, -1,429,430,431,
	-1, -1, -1, -1, -1,432,433,434,
	-1, -1, -1, -1, -1,435,436,437,
	-1, -1, -1, -1, -1,459,438,439,
	-1, -1, -1, -1, -1, -1,460,440,
	-1, -1, -1, -1, -1, -1, -1,461 }
};

static int binomial[5][64];
static int pawnidx[5][24];
static int pfactor[5][4];

static void init_indices()
{
	auto i = 0, j = 0;

	// binomial[k-1][n] = Bin(n, k)
	for (i = 0; i < 5; i++)
		for (j = 0; j < 64; j++) {
			auto f = j;
			auto l = 1;
			for (auto k = 1; k <= i; k++) {
				f *= (j - k);
				l *= (k + 1);
			}
			binomial[i][j] = f / l;
		}

	for (i = 0; i < 5; i++) {
		auto s = 0;
		for (j = 0; j < 6; j++) {
			pawnidx[i][j] = s;
			s += (i == 0) ? 1 : binomial[i - 1][ptwist[invflap[j]]];
		}
		pfactor[i][0] = s;
		s = 0;
		for (; j < 12; j++) {
			pawnidx[i][j] = s;
			s += (i == 0) ? 1 : binomial[i - 1][ptwist[invflap[j]]];
		}
		pfactor[i][1] = s;
		s = 0;
		for (; j < 18; j++) {
			pawnidx[i][j] = s;
			s += (i == 0) ? 1 : binomial[i - 1][ptwist[invflap[j]]];
		}
		pfactor[i][2] = s;
		s = 0;
		for (; j < 24; j++) {
			pawnidx[i][j] = s;
			s += (i == 0) ? 1 : binomial[i - 1][ptwist[invflap[j]]];
		}
		pfactor[i][3] = s;
	}
}

static uint64 encode_piece(const tb_entry_piece* ptr, const ubyte* norm, int* pos, const int* factor)
{
	uint64 idx = 0;
	auto i = 0, j = 0, l = 0;
	const int n = ptr->num;

	if (pos[0] & 0x04) {
		for (i = 0; i < n; i++)
			pos[i] ^= 0x07;
	}
	if (pos[0] & 0x20) {
		for (i = 0; i < n; i++)
			pos[i] ^= 0x38;
	}

	for (i = 0; i < n; i++)
		if (offdiag[pos[i]]) break;
	if (i < (ptr->enc_type == 0 ? 3 : 2) && offdiag[pos[i]] > 0)
		for (i = 0; i < n; i++)
			pos[i] = flipdiag[pos[i]];

	switch (ptr->enc_type) {

	case 0: /* 111 */
		i = (pos[1] > pos[0]);
		j = (pos[2] > pos[0]) + (pos[2] > pos[1]);

		if (offdiag[pos[0]])
			idx = triangle[pos[0]] * static_cast<uint64_t>(63 * 62) + (static_cast<int64_t>(pos[1]) - static_cast<int64_t>(i)) * static_cast<uint64_t>(62) + (static_cast<uint64_t>(pos[2]) - static_cast<uint64_t>(j));
		else if (offdiag[pos[1]])
			idx = static_cast<uint64_t>(6 * 63 * 62) + static_cast<uint64_t>(diag[pos[0]]) * static_cast<uint64_t>(28 * 62) + static_cast<uint64_t>(lower[pos[1]]) * static_cast<uint64_t>(62) + static_cast<uint64_t>(pos[2]) - static_cast<uint64_t>(j);
		else if (offdiag[pos[2]])
			idx = static_cast<uint64_t>(6 * 63 * 62 + 4 * 28 * 62) + static_cast<uint64_t>(diag[pos[0]]) * static_cast<uint64_t>(7 * 28) + (static_cast<uint64_t>(diag[pos[1]]) - static_cast<uint64_t>(i)) * static_cast<uint64_t>(28) + lower[pos[2]];
		else
			idx = static_cast<uint64_t>(6 * 63 * 62 + 4 * 28 * 62 + 4 * 7 * 28) + (static_cast<uint64_t>(diag[pos[0]]) * static_cast<uint64_t>(7 * 6)) + (static_cast<uint64_t>(diag[pos[1]]) - static_cast<uint64_t>(i)) * static_cast<uint64_t>(6) + (static_cast<uint64_t>(diag[pos[2]]) - static_cast<uint64_t>(j));
		i = 3;
		break;

	case 1: /* K3 */
		j = (pos[2] > pos[0]) + (pos[2] > pos[1]);

		idx = kk_idx[triangle[pos[0]]][pos[1]];
		if (idx < 441)
			idx = idx + static_cast<uint64_t>(441) * (static_cast<uint64_t>(pos[2]) - static_cast<uint64_t>(j));
		else {
			idx = static_cast<uint64_t>(441) * static_cast<uint64_t>(62) + (idx - static_cast<uint64_t>(441)) + static_cast<uint64_t>(21) * static_cast<uint64_t>(lower[pos[2]]);
			if (!offdiag[pos[2]])
				idx -= static_cast<uint64_t>(j) * static_cast<uint64_t>(21);
		}
		i = 3;
		break;

	default: /* K2 */
		idx = kk_idx[triangle[pos[0]]][pos[1]];
		i = 2;
		break;
	}
	idx *= factor[0];

	while (i < n) {
		const int t = norm[i];
		for (j = i; j < i + t; j++)
			for (auto k = j + 1; k < i + t; k++)
				if (pos[j] > pos[k])
					SWAP(pos[j], pos[k])
		auto s = 0;
		for (auto m = i; m < i + t; m++) {
			const auto p = pos[m];
			for (l = 0, j = 0; l < i; l++)
				j += (p > pos[l]);
			s += binomial[m - i][p - j];
		}
		idx += static_cast<uint64>(s) * static_cast<uint64>(factor[i]);
		i += t;
	}

	return idx;
}

// determine file of leftmost pawn and sort pawns
static int pawn_file(const tb_entry_pawn* ptr, int* pos)
{
	for (auto i = 1; i < ptr->pawns[0]; i++)
		if (flap[pos[0]] > flap[pos[i]])
			SWAP(pos[0], pos[i])

	return file_to_file[pos[0] & 0x07];
}

static uint64 encode_pawn(const tb_entry_pawn* ptr, const ubyte* norm, int* pos, const int* factor)
{
	auto i = 0, j = 0, k = 0, m = 0, s = 0;
	const int n = ptr->num;

	if (pos[0] & 0x04)
		for (i = 0; i < n; i++)
			pos[i] ^= 0x07;

	for (i = 1; i < ptr->pawns[0]; i++)
		for (j = i + 1; j < ptr->pawns[0]; j++)
			if (ptwist[pos[i]] < ptwist[pos[j]])
				SWAP(pos[i], pos[j])

	auto t = ptr->pawns[0] - 1;
	uint64 idx = pawnidx[t][flap[pos[0]]];
	for (i = t; i > 0; i--)
		idx += binomial[t - i][ptwist[pos[i]]];
	idx *= factor[0];

	// remaining pawns
	i = ptr->pawns[0];
	t = i + ptr->pawns[1];
	if (t > i) {
		for (j = i; j < t; j++)
			for (k = j + 1; k < t; k++)
				if (pos[j] > pos[k])
					SWAP(pos[j], pos[k])
		s = 0;
		for (m = i; m < t; m++) {
			const auto p = pos[m];
			for (k = 0, j = 0; k < i; k++)
				j += (p > pos[k]);
			s += binomial[m - i][p - j - 8];
		}
		idx += static_cast<uint64>(s) * static_cast<uint64>(factor[i]);
		i = t;
	}

	while (i < n) {
		t = norm[i];
		for (j = i; j < i + t; j++)
			for (k = j + 1; k < i + t; k++)
				if (pos[j] > pos[k])
					SWAP(pos[j], pos[k])
		s = 0;
		for (m = i; m < i + t; m++) {
			const auto p = pos[m];
			for (k = 0, j = 0; k < i; k++)
				j += (p > pos[k]);
			s += binomial[m - i][p - j];
		}
		idx += static_cast<uint64>(s) * static_cast<uint64>(factor[i]);
		i += t;
	}

	return idx;
}

// place k like pieces on n squares
static int subfactor(const int k, const int n)
{
	auto f = n;
	auto l = 1;
	for (auto i = 1; i < k; i++) {
		f *= n - i;
		l *= i + 1;
	}

	return f / l;
}

static uint64 calc_factors_piece(int* factor, const int num, const int order, const ubyte* norm, const ubyte enc_type)
{
	auto i = 0, k = 0;
	static int pivfac[] = { 31332, 28056, 462 };

	auto n = 64 - norm[0];

	uint64 f = 1;
	for (i = norm[0], k = 0; i < num || k == order; k++) {
		if (k == order) {
			factor[0] = static_cast<int>(f);
			f *= pivfac[enc_type];
		}
		else {
			factor[i] = static_cast<int>(f);
			f *= subfactor(norm[i], n);
			n -= norm[i];
			i += norm[i];
		}
	}

	return f;
}

static uint64 calc_factors_pawn(int* factor, const int num, const int order, const int order2, const ubyte* norm, const int file)
{
	int i = norm[0];
	if (order2 < 0x0f) i += norm[i];
	auto n = 64 - i;

	uint64 f = 1;
	for (auto k = 0; i < num || k == order || k == order2; k++) {
		if (k == order) {
			factor[0] = static_cast<int>(f);
			f *= pfactor[norm[0] - 1][file];
		}
		else if (k == order2) {
			factor[norm[0]] = static_cast<int>(f);
			f *= subfactor(norm[norm[0]], 48 - norm[0]);
		}
		else {
			factor[i] = static_cast<int>(f);
			f *= subfactor(norm[i], n);
			n -= norm[i];
			i += norm[i];
		}
	}

	return f;
}

static void set_norm_piece(const tb_entry_piece* ptr, ubyte* norm, const ubyte* pieces)
{
	auto i = 0;

	for (i = 0; i < ptr->num; i++)
		norm[i] = 0;

	switch (ptr->enc_type) {
	case 0:
		norm[0] = 3;
		break;
	case 2:
		norm[0] = 2;
		break;
	default:
		norm[0] = static_cast<ubyte>(ptr->enc_type - 1);
		break;
	}

	for (i = norm[0]; i < ptr->num; i += norm[i])
		for (auto j = i; j < ptr->num && pieces[j] == pieces[i]; j++)
			norm[i]++;
}

static void set_norm_pawn(const tb_entry_pawn* ptr, ubyte* norm, const ubyte* pieces)
{
	auto i = 0;

	for (i = 0; i < ptr->num; i++)
		norm[i] = 0;

	norm[0] = ptr->pawns[0];
	if (ptr->pawns[1]) norm[ptr->pawns[0]] = ptr->pawns[1];

	for (i = ptr->pawns[0] + ptr->pawns[1]; i < ptr->num; i += norm[i])
		for (auto j = i; j < ptr->num && pieces[j] == pieces[i]; j++)
			norm[i]++;
}

static void setup_pieces_piece(tb_entry_piece* ptr, const unsigned char* data, uint64* tb_size)
{
	auto i = 0;

	for (i = 0; i < ptr->num; i++)
		ptr->pieces[0][i] = static_cast<ubyte>(data[i + 1] & 0x0f);
	auto order = data[0] & 0x0f;
	set_norm_piece(ptr, ptr->norm[0], ptr->pieces[0]);
	tb_size[0] = calc_factors_piece(ptr->factor[0], ptr->num, order, ptr->norm[0], ptr->enc_type);

	for (i = 0; i < ptr->num; i++)
		ptr->pieces[1][i] = static_cast<ubyte>(data[i + 1] >> 4);
	order = data[0] >> 4;
	set_norm_piece(ptr, ptr->norm[1], ptr->pieces[1]);
	tb_size[1] = calc_factors_piece(ptr->factor[1], ptr->num, order, ptr->norm[1], ptr->enc_type);
}

static void setup_pieces_piece_dtz(dtz_entry_piece* ptr, const unsigned char* data, uint64* tb_size)
{
	for (auto i = 0; i < ptr->num; i++)
		ptr->pieces[i] = static_cast<ubyte>(data[i + 1] & 0x0f);
	const auto order = data[0] & 0x0f;
	set_norm_piece(reinterpret_cast<tb_entry_piece*>(ptr), ptr->norm, ptr->pieces);
	tb_size[0] = calc_factors_piece(ptr->factor, ptr->num, order, ptr->norm, ptr->enc_type);
}

static void setup_pieces_pawn(tb_entry_pawn* ptr, const unsigned char* data, uint64* tb_size, const int f)
{
	auto i = 0;

	const auto j = 1 + (ptr->pawns[1] > 0);
	auto order = data[0] & 0x0f;
	auto order2 = ptr->pawns[1] ? (data[1] & 0x0f) : 0x0f;
	for (i = 0; i < ptr->num; i++)
		ptr->file[f].pieces[0][i] = static_cast<ubyte>(data[i + j] & 0x0f);
	set_norm_pawn(ptr, ptr->file[f].norm[0], ptr->file[f].pieces[0]);
	tb_size[0] = calc_factors_pawn(ptr->file[f].factor[0], ptr->num, order, order2, ptr->file[f].norm[0], f);

	order = data[0] >> 4;
	order2 = ptr->pawns[1] ? (data[1] >> 4) : 0x0f;
	for (i = 0; i < ptr->num; i++)
		ptr->file[f].pieces[1][i] = static_cast<ubyte>(data[i + j] >> 4);
	set_norm_pawn(ptr, ptr->file[f].norm[1], ptr->file[f].pieces[1]);
	tb_size[1] = calc_factors_pawn(ptr->file[f].factor[1], ptr->num, order, order2, ptr->file[f].norm[1], f);
}

static void setup_pieces_pawn_dtz(dtz_entry_pawn* ptr, const unsigned char* data, uint64* tb_size, const int f)
{
	const auto j = 1 + (ptr->pawns[1] > 0);
	const auto order = data[0] & 0x0f;
	const auto order2 = ptr->pawns[1] ? (data[1] & 0x0f) : 0x0f;
	for (auto i = 0; i < ptr->num; i++)
		ptr->file[f].pieces[i] = static_cast<ubyte>(data[i + j] & 0x0f);
	set_norm_pawn(reinterpret_cast<tb_entry_pawn*>(ptr), ptr->file[f].norm, ptr->file[f].pieces);
	tb_size[0] = calc_factors_pawn(ptr->file[f].factor, ptr->num, order, order2, ptr->file[f].norm, f);
}

static void calc_symlen(pairs_data* d, const int s, char* tmp)
{
	if (d != nullptr)
	{
		const auto* const w = d->sympat + 3 * static_cast<int>(s);
		if (const auto s2 = (w[2] << 4) | (w[1] >> 4); s2 == 0x0fff)
			d->symlen[s] = 0;
		else {
			const auto s1 = ((w[1] & 0xf) << 8) | w[0];
			if (!tmp[s1]) calc_symlen(d, s1, tmp);
			if (!tmp[s2]) calc_symlen(d, s2, tmp);
			d->symlen[s] = static_cast<ubyte>(d->symlen[s1] + d->symlen[s2] + 1);
		}
		tmp[s] = 1;
	}
}

ushort read_ushort(const ubyte* d) {
	return static_cast<ushort>(d[0] | (d[1] << 8));
}

uint32 read_uint32(const ubyte* d) {
	return d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
}

static pairs_data* setup_pairs(unsigned char* data, const uint64 tb_size, uint64* size, unsigned char** next, ubyte* flags, const int wdl)
{
	pairs_data* d = nullptr;
	auto i = 0;

	*flags = data[0];
	if (data[0] & 0x80) {
		d = static_cast<pairs_data*>(malloc(sizeof(pairs_data)));
		if (d != nullptr)
		{
			d->idxbits = 0;
			if (wdl)
				d->min_len = data[1];
			else
				d->min_len = 0;
		}

		*next = data + 2;
		size[0] = size[1] = size[2] = 0;
		return d;
	}

	const int blocksize = data[1];
	const int idxbits = data[2];
	const int real_num_blocks = static_cast<int>(read_uint32(&data[4]));
	const auto num_blocks = real_num_blocks + *&data[3];
	const int max_len = data[8];
	const int min_len = data[9];
	const auto h = max_len - min_len + 1;
	const int num_syms = read_ushort(&data[10 + 2 * h]);
	d = static_cast<pairs_data*>(malloc(sizeof(pairs_data) + (h - 1) * sizeof(base_t) + num_syms));
	if (d != nullptr)
	{
		d->blocksize = blocksize;
		d->idxbits = idxbits;
		d->offset = reinterpret_cast<ushort*>(&data[10]);
		d->symlen = reinterpret_cast<ubyte*>(d) + sizeof(pairs_data) + (h - 1) * sizeof(base_t);
		d->sympat = &data[12 + 2 * h];
		d->min_len = min_len;
	}
	*next = &data[12 + 2 * h + 3 * num_syms + (num_syms & 1)];

	const auto num_indices = (tb_size + (1ULL << idxbits) - 1) >> idxbits;
	size[0] = 6ULL * num_indices;
	size[1] = 2ULL * num_blocks;
	size[2] = (1ULL << blocksize) * real_num_blocks;

	// char tmp[num_syms];
	char tmp[4096]{};
	for (i = 0; i < num_syms; i++)
		tmp[i] = 0;
	for (i = 0; i < num_syms; i++)
		if (!tmp[i])
			calc_symlen(d, i, tmp);
	if (d != nullptr)
	{
		d->base[h - 1] = 0;
		for (i = h - 2; i >= 0; i--)
			d->base[i] = (d->base[i + 1] + read_ushort(reinterpret_cast<ubyte*>(d->offset + i)) - read_ushort(reinterpret_cast<ubyte*>(d->offset + i + 1))) / 2;
		for (i = 0; i < h; i++)
			d->base[i] <<= 64 - (min_len + i);

		d->offset -= d->min_len;
	}
	return d;
}

static int init_table_wdl(tb_entry* entry, char* str)
{
	ubyte* next = nullptr;
	uint64 tb_size[8]{};
	uint64 size[8 * 3]{};
	ubyte flags = 0;

	// first mmap the table into memory

	entry->data = map_file(str, wdl_suffix, &entry->mapping);
	if (!entry->data) {
		printf("Could not find %s" "wdl_suffix", str);
		return 0;
	}

	auto* data = reinterpret_cast<ubyte*>(entry->data);
	if (data[0] != wdl_magic[0] ||
		data[1] != wdl_magic[1] ||
		data[2] != wdl_magic[2] ||
		data[3] != wdl_magic[3]) {
		printf("Corrupted table.\n");
		unmap_file(entry->data, entry->mapping);
		entry->data = nullptr;
		return 0;
	}

	const auto split = data[4] & 0x01;
	const auto files = data[4] & 0x02 ? 4 : 1;

	data += 5;

	if (!entry->has_pawns) {
		auto* ptr = reinterpret_cast<struct tb_entry_piece*>(entry);
		setup_pieces_piece(ptr, data, &tb_size[0]);
		data += ptr->num + 1;
		data += reinterpret_cast<uintptr_t>(data) & 0x01;

		ptr->precomp[0] = setup_pairs(data, tb_size[0], &size[0], &next, &flags, 1);
		data = next;
		if (split) {
			ptr->precomp[1] = setup_pairs(data, tb_size[1], &size[3], &next, &flags, 1);
			data = next;
		}
		else
			ptr->precomp[1] = nullptr;

		if (ptr->precomp[0] != nullptr)
			ptr->precomp[0]->indextable = reinterpret_cast<char*>(data);
		data += size[0];
		if (split && ptr->precomp[0] != nullptr) {
			ptr->precomp[1]->indextable = reinterpret_cast<char*>(data);
			data += size[3];
		}
		if (ptr->precomp[0] != nullptr)
			ptr->precomp[0]->sizetable = reinterpret_cast<ushort*>(data);
		data += size[1];
		if (split && ptr->precomp[0] != nullptr) {
			ptr->precomp[1]->sizetable = reinterpret_cast<ushort*>(data);
			data += size[4];
		}

		data = reinterpret_cast<ubyte*>((reinterpret_cast<uintptr_t>(data) + 0x3f) & ~0x3f);
		if (ptr->precomp[0] != nullptr)
			ptr->precomp[0]->data = data;
		data += size[2];
		if (split && ptr->precomp[0] != nullptr) {
			data = reinterpret_cast<ubyte*>((reinterpret_cast<uintptr_t>(data) + 0x3f) & ~0x3f);
			ptr->precomp[1]->data = data;
		}
	}
	else {
		auto f = 0;
		auto* ptr = reinterpret_cast<struct tb_entry_pawn*>(entry);
		const auto s = 1 + (ptr->pawns[1] > 0);
		for (f = 0; f < 4; f++) {
			setup_pieces_pawn(ptr, data, &tb_size[2 * f], f);
			data += ptr->num + static_cast<int>(s);
		}
		data += reinterpret_cast<uintptr_t>(data) & 0x01;

		for (f = 0; f < files; f++) {
			ptr->file[f].precomp[0] = setup_pairs(data, tb_size[2 * f], &size[6 * f], &next, &flags, 1);
			data = next;
			if (split) {
				ptr->file[f].precomp[1] = setup_pairs(data, tb_size[2 * f + 1], &size[6 * f + 3], &next, &flags, 1);
				data = next;
			}
			else
				ptr->file[f].precomp[1] = nullptr;
		}

		for (f = 0; f < files; f++) {
			ptr->file[f].precomp[0]->indextable = reinterpret_cast<char*>(data);
			data += size[6 * f];
			if (split) {
				ptr->file[f].precomp[1]->indextable = reinterpret_cast<char*>(data);
				data += size[6 * f + 3];
			}
		}

		for (f = 0; f < files; f++) {
			ptr->file[f].precomp[0]->sizetable = reinterpret_cast<ushort*>(data);
			data += size[6 * f + 1];
			if (split) {
				ptr->file[f].precomp[1]->sizetable = reinterpret_cast<ushort*>(data);
				data += size[6 * f + 4];
			}
		}

		for (f = 0; f < files; f++) {
			data = reinterpret_cast<ubyte*>((reinterpret_cast<uintptr_t>(data) + 0x3f) & ~0x3f);
			ptr->file[f].precomp[0]->data = data;
			data += size[6 * f + 2];
			if (split) {
				data = reinterpret_cast<ubyte*>((reinterpret_cast<uintptr_t>(data) + 0x3f) & ~0x3f);
				ptr->file[f].precomp[1]->data = data;
				data += size[6 * f + 5];
			}
		}
	}

	return 1;
}

static int init_table_dtz(tb_entry* entry)
{
	auto* data = reinterpret_cast<ubyte*>(entry->data);
	ubyte* next = nullptr;
	uint64 tb_size[4]{};
	uint64 size[4 * 3]{};

	if (!data)
		return 0;

	if (data[0] != dtz_magic[0] ||
		data[1] != dtz_magic[1] ||
		data[2] != dtz_magic[2] ||
		data[3] != dtz_magic[3]) {
		printf("Corrupted table.\n");
		return 0;
	}

	const auto files = data[4] & 0x02 ? 4 : 1;

	data += 5;

	if (!entry->has_pawns) {
		auto* ptr = reinterpret_cast<struct dtz_entry_piece*>(entry);
		setup_pieces_piece_dtz(ptr, data, &tb_size[0]);
		data += ptr->num + 1;
		data += reinterpret_cast<uintptr_t>(data) & 0x01;

		ptr->precomp = setup_pairs(data, tb_size[0], &size[0], &next, &(ptr->flags), 0);
		data = next;

		ptr->map = data;
		if (ptr->flags & 2) {
			for (auto& i : ptr->map_idx)
			{
				i = static_cast<ushort>(data + 1 - ptr->map);
				data += 1 + static_cast<int>(data[0]);
			}
			data += reinterpret_cast<uintptr_t>(data) & 0x01;
		}
		if (ptr->precomp != nullptr)
			ptr->precomp->indextable = reinterpret_cast<char*>(data);
		data += size[0];

		if (ptr->precomp != nullptr)
			ptr->precomp->sizetable = reinterpret_cast<ushort*>(data);
		data += size[1];

		data = reinterpret_cast<ubyte*>((reinterpret_cast<uintptr_t>(data) + 0x3f) & ~0x3f);
		if (ptr->precomp != nullptr)
			ptr->precomp->data = data;
		data += size[2];
	}
	else {
		auto f = 0;
		auto* ptr = reinterpret_cast<struct dtz_entry_pawn*>(entry);
		const auto s = 1 + (ptr->pawns[1] > 0);
		for (f = 0; f < 4; f++) {
			setup_pieces_pawn_dtz(ptr, data, &tb_size[f], f);
			data += ptr->num + static_cast<int>(s);
		}
		data += reinterpret_cast<uintptr_t>(data) & 0x01;

		for (f = 0; f < files; f++) {
			ptr->file[f].precomp = setup_pairs(data, tb_size[f], &size[3 * f], &next, &(ptr->flags[f]), 0);
			data = next;
		}

		ptr->map = data;
		for (f = 0; f < files; f++) {
			if (ptr->flags[f] & 2) {
				for (auto i = 0; i < 4; i++) {
					ptr->map_idx[f][i] = static_cast<ushort>(data + 1 - ptr->map);
					data += 1 + static_cast<int>(data[0]);
				}
			}
		}
		data += reinterpret_cast<uintptr_t>(data) & 0x01;

		for (f = 0; f < files; f++) {
			ptr->file[f].precomp->indextable = reinterpret_cast<char*>(data);
			data += size[3 * f];
		}

		for (f = 0; f < files; f++) {
			ptr->file[f].precomp->sizetable = reinterpret_cast<ushort*>(data);
			data += size[3 * f + 1];
		}

		for (f = 0; f < files; f++) {
			data = reinterpret_cast<ubyte*>((reinterpret_cast<uintptr_t>(data) + 0x3f) & ~0x3f);
			ptr->file[f].precomp->data = data;
			data += size[3 * f + 2];
		}
	}

	return 1;
}

template<bool LittleEndian>
static ubyte decompress_pairs(pairs_data* d, const uint64 idx)
{
	if (!d->idxbits)
		return static_cast<ubyte>(d->min_len);

	const auto mainidx = static_cast<uint32>(idx >> d->idxbits);
	int litidx = static_cast<int>((idx & ((1ULL << d->idxbits) - 1)) - (1ULL << (d->idxbits - 1)));
	auto block = *reinterpret_cast<uint32*>(d->indextable + 6 * static_cast<int>(mainidx));
	if (!LittleEndian)
		block = B_SWAP32(block);

	auto idx_offset = *reinterpret_cast<ushort*>(d->indextable + 6 * static_cast<int>(mainidx) + 4);
	if (!LittleEndian)
		idx_offset = static_cast<ushort>((idx_offset << 8) | (idx_offset >> 8));
	litidx += idx_offset;

	if (litidx < 0) {
		do {
			litidx += d->sizetable[--block] + 1;
		} while (litidx < 0);
	}
	else {
		while (litidx > d->sizetable[block])
			litidx -= d->sizetable[block++] + 1;
	}

	auto* ptr = reinterpret_cast<uint32*>(d->data + (block << static_cast<unsigned>(d->blocksize)));

	const auto m = d->min_len;
	const auto* const offset = d->offset;
	const auto* const base = d->base - m;
	const auto* const symlen = d->symlen;
	auto sym = 0;

	auto code = *reinterpret_cast<uint64*>(ptr);
	if (LittleEndian)
		code = B_SWAP64(code);

	ptr += 2;
	auto bitcnt = 0; // number of "empty bits" in code
	for (;;) {
		auto l = m;
		while (code < base[l]) l++;
		sym = offset[l];
		if (!LittleEndian)
			sym = ((sym & 0xff) << 8) | (sym >> 8);
		sym += static_cast<int>((code - base[l]) >> (64 - l));
		if (litidx < static_cast<int>(symlen[sym]) + 1) break;
		litidx -= static_cast<int>(symlen[sym]) + 1;
		code <<= l;
		bitcnt += l;
		if (bitcnt >= 32) {
			bitcnt -= 32;
			auto tmp = *ptr++;
			if (LittleEndian)
				tmp = B_SWAP32(tmp);
			code |= static_cast<uint64>(tmp) << bitcnt;
		}
	}

	const auto* const sympat = d->sympat;
	while (symlen[sym] != 0) {
		const auto* const w = sympat + (3 * sym);
		if (const auto s1 = ((w[1] & 0xf) << 8) | w[0]; litidx < static_cast<int>(symlen[s1]) + 1)
			sym = s1;
		else {
			litidx -= static_cast<int>(symlen[s1]) + 1;
			sym = (w[2] << 4) | (w[1] >> 4);
		}
	}

	return sympat[3 * sym];
}

void load_dtz_table(const char* str, const uint64 key1, const uint64 key2)
{
	auto i = 0;

	dtz_table[0].key1 = key1;
	dtz_table[0].key2 = key2;
	dtz_table[0].entry = nullptr;

	// find corresponding WDL entry
	const auto* const ptr2 = tb_hash[key1 >> (64 - tb_hash_bits)];
	for (i = 0; i < hash_max; i++)
		if (ptr2[i].key == key1) break;
	if (i == hash_max) return;
	auto* const ptr = ptr2[i].ptr;

	auto* ptr3 = static_cast<struct tb_entry*>(malloc(ptr->has_pawns
		? sizeof(struct dtz_entry_pawn)
		: sizeof(struct dtz_entry_piece)));
	if (ptr3 != nullptr)
	{
		ptr3->data = map_file(str, dtz_suffix, &ptr3->mapping);
		ptr3->key = ptr->key;
		ptr3->num = ptr->num;
		ptr3->symmetric = ptr->symmetric;
		ptr3->has_pawns = ptr->has_pawns;
		if (ptr3->has_pawns) {
			auto* entry = reinterpret_cast<struct dtz_entry_pawn*>(ptr3);
			entry->pawns[0] = reinterpret_cast<tb_entry_pawn*>(ptr)->pawns[0];
			entry->pawns[1] = reinterpret_cast<tb_entry_pawn*>(ptr)->pawns[1];
		}
		else {
			auto* const entry = reinterpret_cast<struct dtz_entry_piece*>(ptr3);
			entry->enc_type = reinterpret_cast<tb_entry_piece*>(ptr)->enc_type;
		}
		if (!init_table_dtz(ptr3))
			free(ptr3);
		else
			dtz_table[0].entry = ptr3;
	}
}

static void free_wdl_entry(tb_entry* entry)
{
	unmap_file(entry->data, entry->mapping);
	if (!entry->has_pawns) {
		const auto* ptr = reinterpret_cast<struct tb_entry_piece*>(entry);
		free(ptr->precomp[0]);
		if (ptr->precomp[1])
			free(ptr->precomp[1]);
	}
	else {
		auto* ptr = reinterpret_cast<struct tb_entry_pawn*>(entry);
		for (const auto& [precomp, factor, pieces, norm] : ptr->file)
		{
			free(precomp[0]);
			if (precomp[1])
				free(precomp[1]);
		}
	}
}

static void free_dtz_entry(tb_entry* entry)
{
	unmap_file(entry->data, entry->mapping);
	if (!entry->has_pawns) {
		const auto* const ptr = reinterpret_cast<struct dtz_entry_piece*>(entry);
		free(ptr->precomp);
	}
	else {
		auto* ptr = reinterpret_cast<struct dtz_entry_pawn*>(entry);
		for (const auto& [precomp, factor, pieces, norm] : ptr->file)
			free(precomp);
	}
	free(entry);
}

static int wdl_to_map[5] = { 1, 3, 0, 2, 0 };
static ubyte pa_flags[5] = { 8, 0, 0, 0, 4 };


enum syzygy_piece_type
{
	syz_none,
	syz_pawn,
	syz_knight,
	syz_bishop,
	syz_rook,
	syz_queen,
	syz_king,
	syz_nb
};

inline syzygy_piece_type& operator++(syzygy_piece_type& d)
{
	return d = static_cast<syzygy_piece_type>(static_cast<int>(d) + 1);
}

inline syzygy_piece_type& operator--(syzygy_piece_type& d)
{
	return d = static_cast<syzygy_piece_type>(static_cast<int>(d) - 1);
}

constexpr uint8_t pt_from_syz[syz_nb] = {
	no_piecetype, pt_pawn, pt_knight, pt_bishop, pt_rook, pt_queen, pt_king
};

constexpr syzygy_piece_type syz_from_pt[num_piecetypes] = {
	syz_none, syz_king, syz_pawn, syz_knight, syz_bishop, syz_rook, syz_queen
};


static void prt_str(const position& pos, char* str, const int mirror)
{
	auto pt = syz_none;
	auto i = 0;

	auto color = !mirror ? white : black;
	for (pt = syz_king; pt >= syz_pawn; --pt)
		for (i = pos.number(color, pt_from_syz[pt]); i > 0; i--)
			*str++ = pchr[6 - pt];
	*str++ = 'v';
	color = ~color;
	for (pt = syz_king; pt >= syz_pawn; --pt)
		for (i = pos.number(color, pt_from_syz[pt]); i > 0; i--)
			*str++ = pchr[6 - pt];
	*str++ = 0;
}

static uint64 calc_key(const position& pos, const int mirror)
{
	auto pt = syz_none;
	auto i = 0;
	uint64 key = 0;

	auto color = !mirror ? white : black;
	for (pt = syz_pawn; pt <= syz_king; ++pt)
		for (i = pos.number(color, pt_from_syz[pt]); i > 0; i--)
			key ^= zobrist::psq[make_piece(white, pt_from_syz[pt])][i - 1];
	color = ~color;
	for (pt = syz_pawn; pt <= syz_king; ++pt)
		for (i = pos.number(color, pt_from_syz[pt]); i > 0; i--)
			key ^= zobrist::psq[make_piece(black, pt_from_syz[pt])][i - 1];

	return key;
}

static uint64 calc_key_from_pcs(const int* pcs, const int mirror)
{
	auto pt = syz_none;
	auto i = 0;
	uint64 key = 0;

	auto color = !mirror ? 0 : 8;
	for (pt = syz_pawn; pt <= syz_king; ++pt)
		for (i = 0; i < pcs[color + pt]; i++)
			key ^= zobrist::psq[make_piece(white, pt_from_syz[pt])][i];
	color ^= 8;
	for (pt = syz_pawn; pt <= syz_king; ++pt)
		for (i = 0; i < pcs[color + pt]; i++)
			key ^= zobrist::psq[make_piece(black, pt_from_syz[pt])][i];

	return key;
}

bool is_little_endian()
{
	union
	{
		int i;
		char c[sizeof(int)];
	} x{};
	x.i = 1;
	return x.c[0] == 1;
}

static ubyte decompress_pairs(pairs_data* d, const uint64 idx)
{
	static const auto islittleendian = is_little_endian();
	return islittleendian
		? decompress_pairs<true>(d, idx)
		: decompress_pairs<false>(d, idx);
}

static int probe_wdl_table(const position& pos, int* success)
{
	auto i = 0;
	ubyte res = 0;
	int p[tb_pieces];

	const auto key = pos.material_key();

	if (key == (zobrist::psq[w_king][0] ^ zobrist::psq[b_king][0]))
		return 0;

	auto* ptr2 = tb_hash[key >> (64 - tb_hash_bits)];
	for (i = 0; i < hash_max; i++)
		if (ptr2[i].key == key) break;
	if (i == hash_max)
	{
		*success = 0;
		return 0;
	}

	auto* ptr = ptr2[i].ptr;
	if (!ptr->ready)
	{
		lock(tb_mutex);
		char str[16];
		prt_str(pos, str, ptr->key != key);
		if (!init_table_wdl(ptr, str))
		{
			ptr2[i].key = 0ULL;
			*success = 0;
			unlock(tb_mutex);
			return 0;
		}
#ifdef _MSC_VER
		_ReadWriteBarrier();
#else
		__asm__ __volatile__("" ::: "memory");
#endif
		ptr->ready = 1;
		unlock(tb_mutex);
	}

	auto b_side = 0, mirror = 0, c_mirror = 0;
	if (!ptr->symmetric)
	{
		if (key != ptr->key)
		{
			c_mirror = 8;
			mirror = 0x38;
			b_side = pos.on_move() == white;
		}
		else
		{
			c_mirror = mirror = 0;
			b_side = !(pos.on_move() == white);
		}
	}
	else
	{
		c_mirror = pos.on_move() == white ? 0 : 8;
		mirror = pos.on_move() == white ? 0 : 0x38;
		b_side = 0;
	}

	if (uint64 idx = 0; !ptr->has_pawns)
	{
		const auto* entry = reinterpret_cast<struct tb_entry_piece*>(ptr);
		const auto* pc = entry->pieces[b_side];
		for (i = 0; i < entry->num;)
		{
			auto bb = pos.pieces(static_cast<side>((pc[i] ^ c_mirror) >> 3),
				pt_from_syz[pc[i] & 0x07]);
			do
			{
				p[i++] = pop_lsb(&bb);
			} while (bb);
		}
		idx = encode_piece(entry, entry->norm[b_side], p, entry->factor[b_side]);
		res = decompress_pairs(entry->precomp[b_side], idx);
	}
	else
	{
		const auto* entry = reinterpret_cast<struct tb_entry_pawn*>(ptr);
		const auto k = entry->file[0].pieces[0][0] ^ c_mirror;
		auto bb = pos.pieces(static_cast<side>(k >> 3), pt_from_syz[k & 0x07]);
		i = 0;
		do
		{
			p[i++] = pop_lsb(&bb) ^ mirror;
		} while (bb);
		const auto f = pawn_file(entry, p);
		const auto* pc = entry->file[f].pieces[b_side];
		while (i < entry->num)
		{
			bb = pos.pieces(static_cast<side>((pc[i] ^ c_mirror) >> 3),
				pt_from_syz[pc[i] & 0x07]);
			do
			{
				p[i++] = pop_lsb(&bb) ^ mirror;
			} while (bb);
		}
		idx = encode_pawn(entry, entry->file[f].norm[b_side], p, entry->file[f].factor[b_side]);
		res = decompress_pairs(entry->file[f].precomp[b_side], idx);
	}

	return static_cast<int>(res) - 2;
}

static int probe_dtz_table(const position& pos, const int wdl, int* success)
{
	tb_entry* ptr = nullptr;
	auto i = 0, res = 0;
	int p[tb_pieces];

	const auto key = pos.material_key();

	if (dtz_table[0].key1 != key && dtz_table[0].key2 != key)
	{
		for (i = 1; i < dtz_entries; i++)
			if (dtz_table[i].key1 == key) break;
		if (i < dtz_entries)
		{
			const auto table_entry = dtz_table[i];
			for (; i > 0; i--)
				dtz_table[i] = dtz_table[i - 1];
			dtz_table[0] = table_entry;
		}
		else
		{
			const auto* ptr2 = tb_hash[key >> (64 - tb_hash_bits)];
			for (i = 0; i < hash_max; i++)
				if (ptr2[i].key == key) break;
			if (i == hash_max)
			{
				*success = 0;
				return 0;
			}
			ptr = ptr2[i].ptr;
			char str[16];
			const int mirror = ptr->key != key;
			prt_str(pos, str, mirror);
			if (dtz_table[dtz_entries - 1].entry)
				free_dtz_entry(dtz_table[dtz_entries - 1].entry);
			for (i = dtz_entries - 1; i > 0; i--)
				dtz_table[i] = dtz_table[i - 1];
			load_dtz_table(str, calc_key(pos, mirror), calc_key(pos, ~mirror));
		}
	}

	ptr = dtz_table[0].entry;
	if (!ptr)
	{
		*success = 0;
		return 0;
	}

	auto b_side = 0, mirror = 0, c_mirror = 0;
	if (!ptr->symmetric)
	{
		if (key != ptr->key)
		{
			c_mirror = 8;
			mirror = 0x38;
			b_side = pos.on_move() == white;
		}
		else
		{
			c_mirror = mirror = 0;
			b_side = !(pos.on_move() == white);
		}
	}
	else
	{
		c_mirror = pos.on_move() == white ? 0 : 8;
		mirror = pos.on_move() == white ? 0 : 0x38;
		b_side = 0;
	}

	if (uint64 idx = 0; !ptr->has_pawns)
	{
		auto* entry = reinterpret_cast<struct dtz_entry_piece*>(ptr);
		if ((entry->flags & 1) != b_side && !entry->symmetric)
		{
			*success = -1;
			return 0;
		}
		const auto* pc = entry->pieces;
		for (i = 0; i < entry->num;)
		{
			auto bb = pos.pieces(static_cast<side>((pc[i] ^ c_mirror) >> 3),
				pt_from_syz[pc[i] & 0x07]);
			do
			{
				p[i++] = pop_lsb(&bb);
			} while (bb);
		}
		idx = encode_piece(reinterpret_cast<tb_entry_piece*>(entry), entry->norm, p, entry->factor);
		res = decompress_pairs(entry->precomp, idx);

		if (entry->flags & 2)
			res = entry->map[entry->map_idx[wdl_to_map[wdl + 2]] + res];

		if (!(entry->flags & pa_flags[wdl + 2]) || wdl & 1)
			res *= 2;
	}
	else
	{
		auto* entry = reinterpret_cast<struct dtz_entry_pawn*>(ptr);
		const auto k = entry->file[0].pieces[0] ^ c_mirror;
		auto bb = pos.pieces(static_cast<side>(k >> 3), pt_from_syz[k & 0x07]);
		i = 0;
		do
		{
			p[i++] = pop_lsb(&bb) ^ mirror;
		} while (bb);
		const auto f = pawn_file(reinterpret_cast<struct tb_entry_pawn*>(entry), p);
		if ((entry->flags[f] & 1) != b_side)
		{
			*success = -1;
			return 0;
		}
		const auto* pc = entry->file[f].pieces;
		while (i < entry->num)
		{
			bb = pos.pieces(static_cast<side>((pc[i] ^ c_mirror) >> 3),
				pt_from_syz[pc[i] & 0x07]);
			do
			{
				p[i++] = pop_lsb(&bb) ^ mirror;
			} while (bb);
		}
		idx = encode_pawn(reinterpret_cast<tb_entry_pawn*>(entry), entry->file[f].norm, p, entry->file[f].factor);
		res = decompress_pairs(entry->file[f].precomp, idx);

		if (entry->flags[f] & 2)
			res = entry->map[entry->map_idx[f][wdl_to_map[wdl + 2]] + res];

		if (!(entry->flags[f] & pa_flags[wdl + 2]) || wdl & 1)
			res *= 2;
	}

	return res;
}

static s_move* add_underprom_caps(const position& pos, s_move* stack, s_move* end)
{
	auto* extra = end;

	for (auto* moves = stack; moves < end; moves++)
	{
		if (const auto move = moves->move; move_type(move) >= promotion_p && !pos.empty_square(to_square(move)))
		{
			(*extra++).move = move - (1 << 12);
			(*extra++).move = move - (2 << 12);
			(*extra++).move = move - (3 << 12);
		}
	}

	return extra;
}

static int probe_ab(position& pos, int alpha, const int beta, int* success)
{
	auto val = 0;
	s_move stack[64];
	s_move* end = nullptr;

	if (!pos.is_in_check())
	{
		end = generate_moves<captures_promotions>(pos, stack);
		end = add_underprom_caps(pos, stack, end);
	}
	else
		end = generate_moves<evade_check>(pos, stack);

	for (auto* moves = stack; moves < end; moves++)
	{
		const auto capture = moves->move;
		if (!pos.is_capture_move(capture) || move_type(capture) == enpassant
			|| !pos.legal_move(capture))
			continue;
		pos.play_move(capture);
		val = -probe_ab(pos, -beta, -alpha, success);
		pos.take_move_back(capture);
		if (*success == 0) return 0;
		if (val > alpha)
		{
			if (val >= beta)
			{
				*success = 2;
				return val;
			}
			alpha = val;
		}
	}

	val = probe_wdl_table(pos, success);
	if (*success == 0) return 0;
	if (alpha >= val)
	{
		*success = 1 + (alpha > 0);
		return alpha;
	}
	*success = 1;
	return val;
}

int syzygy_probe_wdl(position& pos, int* success)
{
	*success = 1;
	auto val = probe_ab(pos, -2, 2, success);

	if (pos.enpassant_square() == no_square)
		return val;
	if (!*success) return 0;

	auto v1 = -3;
	s_move stack[192];
	s_move* moves = nullptr, * end = nullptr;

	if (!pos.is_in_check())
		end = generate_moves<captures_promotions>(pos, stack);
	else
		end = generate_moves<evade_check>(pos, stack);

	for (moves = stack; moves < end; moves++)
	{
		const auto capture = moves->move;
		if (move_type(capture) != enpassant
			|| !pos.legal_move(capture))
			continue;
		pos.play_move(capture);
		const auto v0 = -probe_ab(pos, -2, 2, success);
		pos.take_move_back(capture);
		if (*success == 0) return 0;
		if (v0 > v1) v1 = v0;
	}
	if (v1 > -3)
	{
		if (v1 >= val) val = v1;
		else if (val == 0)
		{
			for (moves = stack; moves < end; moves++)
			{
				const auto capture = moves->move;
				if (move_type(capture) == enpassant) continue;
				if (pos.legal_move(capture)) break;
			}
			if (moves == end && !pos.is_in_check())
			{
				end = generate_moves<quiet_moves>(pos, end);
				for (; moves < end; moves++)
				{
					if (const auto move = moves->move; pos.legal_move(move))
						break;
				}
			}
			if (moves == end)
				val = v1;
		}
	}

	return val;
}

static int probe_dtz_no_ep(position& pos, int* success)
{
	const auto wdl = probe_ab(pos, -2, 2, success);
	if (*success == 0) return 0;

	if (wdl == 0) return 0;

	if (*success == 2)
		return wdl == 2 ? 1 : 101;

	s_move stack[192];
	s_move* moves = nullptr;
	const s_move* end = nullptr;

	if (wdl > 0)
	{
		if (!pos.is_in_check())
			end = generate_moves<all_moves>(pos, stack);
		else
			end = generate_moves<evade_check>(pos, stack);

		for (moves = stack; moves < end; moves++)
		{
			const auto move = moves->move;
			if (piece_type(pos.moved_piece(move)) != pt_pawn || pos.is_capture_move(move)
				|| !pos.legal_move(move))
				continue;
			pos.play_move(move);
			const auto val = pos.enpassant_square() != no_square ? -syzygy_probe_wdl(pos, success) : -probe_ab(pos, -2, -wdl + 1, success);
			pos.take_move_back(move);
			if (*success == 0) return 0;
			if (val == wdl)
				return val == 2 ? 1 : 101;
		}
	}

	auto dtz = 1 + probe_dtz_table(pos, wdl, success);
	if (*success >= 0)
	{
		if (wdl & 1) dtz += 100;
		return wdl >= 0 ? dtz : -dtz;
	}

	if (wdl > 0)
	{
		auto best = 0xffff;
		for (moves = stack; moves < end; moves++)
		{
			const auto move = moves->move;
			if (pos.is_capture_move(move) || piece_type(pos.moved_piece(move)) == pt_pawn
				|| !pos.legal_move(move))
				continue;
			pos.play_move(move);
			const auto val = -syzygy_probe_dtz(pos, success);
			pos.take_move_back(move);
			if (*success == 0) return 0;
			if (val > 0 && val + 1 < best)
				best = val + 1;
		}
		return best;
	}
	auto best = -1;
	if (!pos.is_in_check())
		end = generate_moves<all_moves>(pos, stack);
	else
		end = generate_moves<evade_check>(pos, stack);
	for (moves = stack; moves < end; moves++)
	{
		auto val = 0;
		const auto move = moves->move;
		if (!pos.legal_move(move))
			continue;
		pos.play_move(move);
		if (pos.fifty_move_counter() == 0)
		{
			if (wdl == -2) val = -1;
			else
			{
				val = probe_ab(pos, 1, 2, success);
				val = val == 2 ? 0 : -101;
			}
		}
		else
		{
			val = -syzygy_probe_dtz(pos, success) - 1;
		}
		pos.take_move_back(move);
		if (*success == 0) return 0;
		if (val < best)
			best = val;
	}
	return best;
}

constexpr int wdl_to_dtz[] = {
	-1, -101, 0, 101, 1
};

int syzygy_probe_dtz(position& pos, int* success)
{
	*success = 1;
	auto val = probe_dtz_no_ep(pos, success);

	if (pos.enpassant_square() == no_square)
		return val;
	if (*success == 0) return 0;

	auto v1 = -3;

	s_move stack[192];
	s_move* moves = nullptr, * end = nullptr;

	if (!pos.is_in_check())
		end = generate_moves<captures_promotions>(pos, stack);
	else
		end = generate_moves<evade_check>(pos, stack);

	for (moves = stack; moves < end; moves++)
	{
		const auto capture = moves->move;
		if (move_type(capture) != enpassant
			|| !pos.legal_move(capture))
			continue;
		pos.play_move(capture);
		const auto v0 = -probe_ab(pos, -2, 2, success);
		pos.take_move_back(capture);
		if (*success == 0) return 0;
		if (v0 > v1) v1 = v0;
	}
	if (v1 > -3)
	{
		v1 = wdl_to_dtz[v1 + 2];
		if (val < -100)
		{
			if (v1 >= 0)
				val = v1;
		}
		else if (val < 0)
		{
			if (v1 >= 0 || v1 < -100)
				val = v1;
		}
		else if (val > 100)
		{
			if (v1 > 0)
				val = v1;
		}
		else if (val > 0)
		{
			if (v1 == 1)
				val = v1;
		}
		else if (v1 >= 0)
		{
			val = v1;
		}
		else
		{
			for (moves = stack; moves < end; moves++)
			{
				const auto move = moves->move;
				if (move_type(move) == enpassant) continue;
				if (pos.legal_move(move)) break;
			}
			if (moves == end && !pos.is_in_check())
			{
				end = generate_moves<quiet_moves>(pos, end);
				for (; moves < end; moves++)
				{
					if (const auto move = moves->move; pos.legal_move(move))
						break;
				}
			}
			if (moves == end)
				val = v1;
		}
	}

	return val;
}
