#include <cinttypes>
#include <iostream>
#include <sstream>
#include <string>

#include "main.h"
#include "position.h"
#include "thread.h"
#include "uci.h"
#include "util.h"

uint64_t perft(position& pos, const int depth) {
  uint64_t cnt = 0;
  const auto leaf = depth == 2;

  for (const auto& m : legal_move_list(pos)) {
    pos.play_move(m, pos.give_check(m));
    cnt += leaf ? legal_move_list(pos).size() : perft(pos, depth - 1);
    pos.take_move_back(m);
  }
  return cnt;
}

uint64_t start_perft(position& pos, const int depth) {
  return depth > 1 ? perft(pos, depth) : legal_move_list(pos).size();
}

void perft(int depth, std::string& fen) {
  uint64_t nodes = 0;
  if (depth < 1) depth = 1;
  if (fen.empty()) fen = startpos;

  search::reset();
  position pos{};
  pos.set(fen, false, thread_pool.main());

  acout() << "" << fen.c_str() << '\n';
  acout() << "depth " << depth << '\n';

  const auto start_time = now();
  const auto cnt = start_perft(pos, depth);
  nodes += cnt;
  const auto elapsed_time = static_cast<double>(now() + 1 - start_time) / 1000;
  const auto nps = static_cast<double>(nodes) / elapsed_time;
  acout() << "nodes " << nodes << '\n';

  std::ostringstream ss;

  ss.precision(3);
  ss << "time " << std::fixed << elapsed_time << " secs" << '\n';
  acout() << ss.str();
  ss.str(std::string());

  ss.precision(0);
  ss << "nps " << std::fixed << nps << '\n';
  acout() << ss.str() << std::flush;
}

void divide(int depth, const std::string& fen) {
  uint64_t nodes = 0;
  if (depth < 1) depth = 1;

  search::reset();
  position pos{};
  pos.set(fen, false, thread_pool.main());
  acout() << fen.c_str() << '\n';
  acout() << "depth " << depth << '\n';
  const auto start_time = now();
  for (const auto& m : legal_move_list(pos)) {
    pos.play_move(m, pos.give_check(m));
    const auto cnt = depth > 1 ? start_perft(pos, depth - 1) : 1;
    pos.take_move_back(m);
    std::cerr << "" << move_to_string(m, pos) << " " << cnt << '\n';
    nodes += cnt;
  }
  const auto elapsed_time = static_cast<double>(now() + 1 - start_time) / 1000;
  const auto nps = static_cast<double>(nodes) / elapsed_time;

  acout() << "nodes " << nodes << '\n';
  std::ostringstream ss;
  ss.precision(3);
  ss << "time " << std::fixed << elapsed_time << " secs" << '\n';
  acout() << ss.str();
  ss.str(std::string());
  ss.precision(0);
  ss << "nps " << std::fixed << nps << '\n';
  acout() << ss.str() << std::flush;
}
