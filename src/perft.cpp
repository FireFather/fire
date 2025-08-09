#include <algorithm>   // For std::max
#include <cinttypes>   // For fixed-width integer types like uint64_t
#include <iostream>    // For std::cout and std::cerr
#include <sstream>     // For std::ostringstream
#include <string>      // For std::string

#include "main.h"      // Project-specific main definitions
#include "position.h"  // Chess position representation and related methods
#include "thread.h"    // Thread pool handling
#include "uci.h"       // UCI (Universal Chess Interface) related functions
#include "util.h"      // Utility functions (timing, etc.)

namespace{
  /**
   * Recursive perft function.
   * @param pos   Reference to the current chess position.
   * @param depth Search depth remaining.
   * @return      Number of nodes (positions) searched from this position.
   *
   * This function generates all legal moves from the current position
   * and recursively explores each move until the search depth reaches 0.
   * The `leaf` optimization: when depth == 2, we can count moves at the next
   * ply directly instead of making another recursive call.
   */
  uint64_t perft(position& pos,const int depth){
    uint64_t cnt=0;// Node counter
    const auto leaf=depth==2;// Optimization flag for second-to-last ply
    // Loop over all legal moves from the current position
    for(const auto& m:legal_move_list(pos)){
      // Play the move (pos.give_check(m) determines if move gives check)
      pos.play_move(m,pos.give_check(m));
      // If at leaf depth, count moves directly; otherwise recurse
      cnt+=leaf?legal_move_list(pos).size()
        :perft(pos,depth-1);
      // Undo the move to restore the original position
      pos.take_move_back(m);
    }
    return cnt;
  }

  /**
   * Entry point for starting perft counting.
   * If depth > 1, calls the recursive perft; otherwise just returns move count.
   */
  uint64_t start_perft(position& pos,const int depth){
    return depth>1?perft(pos,depth)
      :legal_move_list(pos).size();
  }
}// anonymous namespace

/**
 * Runs a perft test from the given FEN string.
 * @param depth Search depth to run.
 * @param fen   FEN string representing the starting position.
 * @return      Result of fflush(stdout) (ensures output is flushed).
 *
 * This function:
 *  1. Loads the starting position from the given FEN (or default startpos if empty).
 *  2. Calls start_perft to count total nodes.
 *  3. Measures execution time and calculates nodes per second (NPS).
 *  4. Prints the results to the console.
 */
int perft(int depth,std::string& fen){
  uint64_t nodes=0;
  // Ensure depth is at least 1
  depth=std::max(depth,1);
  // If no FEN provided, use the default starting position
  if(fen.empty()) fen=startpos;
  // Reset search state (important for consistency between runs)
  search::reset();
  // Initialize position object from FEN
  position pos{};
  pos.set(fen,false,thread_pool.main());
  // Output position and depth
  acout()<<""<<fen.c_str()<<std::endl;
  acout()<<"depth "<<depth<<std::endl;
  // Start timing
  const auto start_time=now();
  // Perform perft calculation
  const auto cnt=start_perft(pos,depth);
  nodes+=cnt;
  // Measure elapsed time in seconds (add 1 ms to avoid division by zero)
  const auto elapsed_time=
    (std::chrono::duration_cast<std::chrono::milliseconds>(now()-start_time).count()+1)/1000.0;
  // Calculate nodes per second
  const auto nps=static_cast<double>(nodes)/elapsed_time;
  // Output results
  acout()<<"nodes "<<nodes<<std::endl;
  std::ostringstream ss;
  ss.precision(3);
  ss<<"time "<<std::fixed<<elapsed_time<<" secs"<<std::endl;
  acout()<<ss.str();
  ss.str(std::string());
  ss.precision(0);
  ss<<"nps "<<std::fixed<<nps<<std::endl;
  acout()<<ss.str();
  // Flush stdout so that output appears immediately
  return fflush(stdout);
}

/**
 * Runs a "divide" perft test.
 * @param depth Search depth to run.
 * @param fen   FEN string representing the starting position.
 * @return      Result of fflush(stdout).
 *
 * The divide command:
 *  - Lists each legal move from the root along with its perft count.
 *  - Useful for debugging and verifying move generation.
 */
int divide(int depth,const std::string& fen){
  uint64_t nodes=0;
  // Ensure depth is at least 1
  depth=std::max(depth,1);
  // Reset search state
  search::reset();
  // Initialize position from FEN
  position pos{};
  pos.set(fen,false,thread_pool.main());
  // Output position and depth
  acout()<<fen.c_str()<<std::endl;
  acout()<<"depth "<<depth<<std::endl;
  // Start timing
  const auto start_time=now();
  // Iterate over all root moves
  for(const auto& m:legal_move_list(pos)){
    // Play the move
    pos.play_move(m,pos.give_check(m));
    // Count nodes from this move
    const auto cnt=depth>1?start_perft(pos,depth-1):1;
    // Undo the move
    pos.take_move_back(m);
    // Print move and its node count
    std::cerr<<""<<move_to_string(m,pos)<<" "<<cnt<<std::endl;
    // Accumulate total node count
    nodes+=cnt;
  }
  // Measure elapsed time in seconds
  const auto elapsed_time=
    (std::chrono::duration_cast<std::chrono::milliseconds>(now()-start_time).count()+1)/1000.0;
  // Calculate nodes per second
  const auto nps=static_cast<double>(nodes)/elapsed_time;
  // Output summary statistics
  acout()<<"nodes "<<nodes<<std::endl;
  std::ostringstream ss;
  ss.precision(3);
  ss<<"time "<<std::fixed<<elapsed_time<<" secs"<<std::endl;
  acout()<<ss.str();
  ss.str(std::string());
  ss.precision(0);
  ss<<"nps "<<std::fixed<<nps<<std::endl;
  acout()<<ss.str();
  // Flush stdout
  return fflush(stdout);
}
