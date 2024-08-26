#include "bench.h"
#include <sstream>
#include "thread.h"
#include "uci.h"
#include "util.h"

int bench(const int depth) {
  int ret = 0;
  uint64_t nodes = 0;
  auto pos_num = 0;
  position pos{};

  const auto start_time = now();

  for (auto& bench_position : bench_positions) {
    auto num_positions = 32;
    const auto start_time_pos = now();
    uint64_t nodes_pos = 0;

    pos_num++;
    search::reset();
    auto s_depth = "depth " + std::to_string(depth);
    std::istringstream is(s_depth);
    pos.set(bench_position, false, thread_pool.main());
    acout() << "position " << pos_num << '/' << num_positions << " "
      << bench_position << " ";
    go(pos, is);
    thread_pool.main()->wait_for_search_to_end();

    nodes_pos += thread_pool.visited_nodes();
    nodes += thread_pool.visited_nodes();
    const auto elapsed_time_pos =
      static_cast<double>(now() + 1 - start_time_pos) / 1000;
    const auto nps_pos = static_cast<double>(nodes_pos) / elapsed_time_pos;

    std::ostringstream ss;
    ss.precision(0);
    ss << "[" << std::fixed << nodes_pos << " nodes ";
    acout() << ss.str();

    ss.str(std::string());
    ss.precision(2);
    ss << std::fixed << elapsed_time_pos << " secs ";
    acout() << ss.str();
    ss.str(std::string());

    ss.precision(0);
    ss << std::fixed << nps_pos << " nps]" << std::endl;
    acout() << ss.str();
    ss.str(std::string());
    ret = fflush(stdout);
  }
  const auto elapsed_time = static_cast<double>(now() + 1 - start_time) / 1000;
  const auto nps = static_cast<double>(nodes) / elapsed_time;

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
  new_game();
  ret = fflush(stdout);
  return ret;
}
