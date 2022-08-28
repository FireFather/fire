/*
  Copyright (c) 2011-2013 Ronald de Man
*/

#pragma once

#ifndef _WIN32
#include <pthread.h>
#define SEP_CHAR ':'
#define FD int
#define FD_ERR -1
#else
#include <Windows.h>
constexpr auto SEP_CHAR = ';';
#define FD HANDLE
#define FD_ERR INVALID_HANDLE_VALUE
#endif

#ifndef _WIN32
#define LOCK_T pthread_mutex_t
#define LOCK_INIT(x) pthread_mutex_init(&(x), NULL)
#define LOCK(x) pthread_mutex_lock(&(x))
#define UNLOCK(x) pthread_mutex_unlock(&(x))
#else
#define LOCK_T HANDLE
#define LOCK_INIT(x) do { (x) = CreateMutex(NULL, FALSE, NULL); } while (0)

template<typename T>
constexpr auto LOCK(T x) { return WaitForSingleObject(x, INFINITE); }

template<typename T>
constexpr auto UNLOCK(T x) { return ReleaseMutex(x); }
#endif

#ifndef _MSC_VER
#define B_SWAP32(v) __builtin_bswap32(v)
#define B_SWAP64(v) __builtin_bswap64(v)
#else
template<typename T>
constexpr auto B_SWAP32(T v) { return _byteswap_ulong(v); }

template<typename T>
constexpr auto B_SWAP64(T v) { return _byteswap_uint64(v); }
#endif

constexpr auto wdl_suffix = ".rtbw";
constexpr auto dtz_suffix = ".rtbz";
constexpr auto wdl_dir = "RTBWDIR";
constexpr auto dtz_dir = "RTBZDIR";
constexpr auto tb_pieces = 6;

typedef unsigned long long uint64;
typedef unsigned int uint32;
typedef unsigned char ubyte;
typedef unsigned short ushort;

constexpr ubyte wdl_magic[4] =
{
	0x71, 0xe8, 0x23, 0x5d
};

constexpr ubyte dtz_magic[4] =
{
	0xd7, 0x66, 0x0c, 0xa5
};

constexpr auto tb_hash_bits = 10;

struct tb_hash_entry;

typedef uint64 base_t;

struct pairs_data
{
	char* indextable;
	ushort* sizetable;
	ubyte* data;
	ushort* offset;
	ubyte* symlen;
	ubyte* sympat;
	int blocksize;
	int idxbits;
	int min_len;
	base_t base[1]; // C++ complains about base[]...
};

struct tb_entry
{
	char* data;
	uint64 key;
	uint64 mapping;
	ubyte ready;
	ubyte num;
	ubyte symmetric;
	ubyte has_pawns;
}
#ifndef _WIN32
__attribute__((__may_alias__))
#endif
;

struct tb_entry_piece
{
	char* data;
	uint64 key;
	uint64 mapping;
	ubyte ready;
	ubyte num;
	ubyte symmetric;
	ubyte has_pawns;
	ubyte enc_type;
	pairs_data* precomp[2];
	int factor[2][tb_pieces];
	ubyte pieces[2][tb_pieces];
	ubyte norm[2][tb_pieces];
};

struct tb_entry_pawn
{
	char* data;
	uint64 key;
	uint64 mapping;
	ubyte ready;
	ubyte num;
	ubyte symmetric;
	ubyte has_pawns;
	ubyte pawns[2];
	struct
	{
		pairs_data* precomp[2];
		int factor[2][tb_pieces];
		ubyte pieces[2][tb_pieces];
		ubyte norm[2][tb_pieces];
	} file[4];
};

struct dtz_entry_piece
{
	char* data;
	uint64 key;
	uint64 mapping;
	ubyte ready;
	ubyte num;
	ubyte symmetric;
	ubyte has_pawns;
	ubyte enc_type;
	pairs_data* precomp;
	int factor[tb_pieces];
	ubyte pieces[tb_pieces];
	ubyte norm[tb_pieces];
	ubyte flags; // accurate, mapped, side
	ushort map_idx[4];
	ubyte* map;
};

struct dtz_entry_pawn
{
	char* data;
	uint64 key;
	uint64 mapping;
	ubyte ready;
	ubyte num;
	ubyte symmetric;
	ubyte has_pawns;
	ubyte pawns[2];
	struct
	{
		pairs_data* precomp;
		int factor[tb_pieces];
		ubyte pieces[tb_pieces];
		ubyte norm[tb_pieces];
	}
	file[4];
	ubyte flags[4];
	ushort map_idx[4][4];
	ubyte* map;
};

struct tb_hash_entry
{
	uint64 key;
	tb_entry* ptr;
};

struct dtz_table_entry
{
	uint64 key1;
	uint64 key2;
	tb_entry* entry;
};


