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

  Thanks to Yu Nasu, Hisayori Noda. This implementation adapted from R. De Man
  and Daniel Shaw's Cfish nnue probe code https://github.com/dshawul/nnue-probe
*/

#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdio>
#include <cctype>

#include "misc.h"

FD open_file(const char* name)
{
#ifndef _WIN32
	return open(name, O_RDONLY);
#else
	return CreateFile(name, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
	                  FILE_FLAG_RANDOM_ACCESS, nullptr);
#endif
}

void close_file(const FD fd)
{
#ifndef _WIN32
	close(fd);
#else
	CloseHandle(fd);
#endif
}

size_t file_size(const FD fd)
{
#ifndef _WIN32
	struct stat statbuf;
	fstat(fd, &statbuf);
	return statbuf.st_size;
#else
	DWORD size_high;
	const DWORD size_low = GetFileSize(fd, &size_high);
	return (static_cast<uint64_t>(size_high) << 32) | size_low;
#endif
}

const void* map_file(const FD fd, map_t* map)
{
#ifndef _WIN32
	* map = file_size(fd);
	void* data = mmap(NULL, *map, PROT_READ, MAP_SHARED, fd, 0);
#ifdef MADV_RANDOM
	madvise(data, *map, MADV_RANDOM);
#endif
	return data == MAP_FAILED ? NULL : data;

#else
	DWORD size_high;
	const DWORD size_low = GetFileSize(fd, &size_high);
	*map = CreateFileMapping(fd, nullptr, PAGE_READONLY, size_high, size_low, nullptr);
	if (*map == nullptr)
		return nullptr;
	return MapViewOfFile(*map, FILE_MAP_READ, 0, 0, 0);
#endif
}

void unmap_file(const void* data, map_t map)
{
	if (!data) return;

#ifndef _WIN32
	munmap((void*)data, map);
#else
	UnmapViewOfFile(data);
	CloseHandle(map);
#endif
}
