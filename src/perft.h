#pragma once
// perft.h
//
// Public interface for perft helpers.
//
// "perft" (performance test) is a standard chess-engine diagnostic that
// counts the number of legal positions reachable from a given position
// up to a specified depth. It is commonly used to validate move generation
// and make/unmake logic. The "divide" variant also prints per-move subtotals.
//
// Notes:
//  - These functions expect that the engine's position, threading, and
//    timing utilities are available via project headers (see perft.cpp).
//  - I/O is performed to the engine's logging/output streams.
//  - Time measurements are printed along with total nodes and NPS.
//
// Thread-safety:
//  - The provided implementations reset global search state before running.
//  - They operate on a single position instance and do not spawn threads
//    themselves. If your engine uses a thread pool globally, make sure it
//    is initialized before calling these functions.

#include <string>

/**
 * Run a perft count from the given starting position.
 *
 * @param depth
 *   Search depth in plies (half-moves). Values less than 1 are clamped to 1.
 *
 * @param fen
 *   FEN string for the starting position. If empty, the function will
 *   substitute the engine's default "startpos" FEN. This parameter is
 *   non-const because the function may assign a default value back into it.
 *
 * Output:
 *   - Prints the FEN, depth, total node count, elapsed time in seconds,
 *     and nodes-per-second (NPS) to the engine's output stream.
 *
 * Return value:
 *   - Returns the result of fflush(stdout) after printing (0 on success,
 *     EOF on error).
 */
int perft(int depth,std::string& fen);

/**
 * Run a "divide" perft from the given starting position.
 *
 * Behavior:
 *   - For each legal root move, prints the move and the perft count that
 *     originates from that move. After listing all moves, prints the total
 *     node count, elapsed time in seconds, and NPS.
 *
 * @param depth
 *   Search depth in plies (half-moves). Values less than 1 are clamped to 1.
 *
 * @param fen
 *   FEN string for the starting position. This function does not modify
 *   the string and will use it as provided.
 *
 * Return value:
 *   - Returns the result of fflush(stdout) after printing (0 on success,
 *     EOF on error).
 */
int divide(int depth,const std::string& fen);
