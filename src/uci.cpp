#include "uci.h"

#include <iostream>
#include <sstream>
#include <string>

#include "bitboard.h"
#include "evaluate.h"
#include "hash.h"
#include "main.h"
#include "nnue.h"
#include "perft.h"
#include "search.h"
#include "thread.h"
#include "util.h"

void new_game() {
  search::signals.stop_analyzing = true;
  thread_pool.main()->wake(false);
  thread_pool.main()->wait_for_search_to_end();
  search::reset();
}

void init_engine() {
  thread_pool.start = now();
  bitboard::init();
  position::init();
  search::init();
  thread_pool.init();
  search::reset();
  main_hash.init(64);
  const char* filename = uci_nnue_evalfile.c_str();
  nnue_init(filename);
}

void uci_loop(const int argc, char* argv[]) {
  position pos{};
  std::string token, cmd;

  pos.set(startpos, uci_chess960, thread_pool.main());
  new_game();

  for (auto i = 1; i < argc; ++i) cmd += std::string(argv[i]) + " ";

  do {
    if (argc == 1 && !getline(std::cin, cmd)) cmd = "quit";
    cmd = trim(cmd);
    if (cmd.empty()) continue;

    std::istringstream is(cmd);

    token.clear();
    is >> std::skipws >> token;

    if (token == "uci") {
      acout() << "id name " << program << " " << version << " " << platform << " " << bmis << '\n';
      acout() << "id author " << author << '\n';
      acout() << "option name Hash type spin default 64 min 16 max 1048576" << '\n';
      acout() << "option name Threads type spin default 1 min 1 max 128" << '\n';
      acout() << "option name MultiPV type spin default 1 min 1 max 64" << '\n';
      acout() << "option name Contempt type spin default 0 min -100 max 100" << '\n';
      acout() << "option name MoveOverhead type spin default 50 min 0 max 1000" << '\n';
      acout() << "option name Ponder type check default false" << '\n';
      acout() << "option name UCI_Chess960 type check default false" << '\n';
      acout() << "uciok" << '\n';
    } else if (token == "isready") {
      acout() << "readyok" << '\n';
    } else if (token == "ucinewgame") {
      new_game();
    } else if (token == "setoption") {
      set_option(is);
    } else if (token == "position") {
      set_position(pos, is);
    } else if (token == "go") {
      go(pos, is);
    } else if (token == "stop"
        || (token == "ponderhit" && search::signals.stop_if_ponder_hit)) {
      search::signals.stop_analyzing = true;
      thread_pool.main()->wake(false);
    } else if (token == "quit") {
      break;
    } else if (token == "pos") {
      acout() << pos;
    } else if (token == "perft") {
      auto depth = 7;
      auto& fen = startpos;
      is >> depth;
      is >> fen;
      perft(depth, fen);
    } else if (token == "divide") {
      auto depth = 7;
      auto& fen = startpos;
      is >> depth;
      is >> fen;
      divide(depth, fen);
    } else if (token == "bench") {
      auto bench_depth = is >> token ? token : "14";
      bench_active = true;
      bench(stoi(bench_depth));
      bench_active = false;
    } else {
    }
  } while (token != "quit" && argc == 1);
  thread_pool.exit();
}

void set_option(std::istringstream& is) {
  std::string token;
  is >> token;

  if (token == "name") {
    while (is >> token) {
      if (token == "Hash") {
        is >> token;
        is >> token;
        uci_hash = stoi(token);
        main_hash.init(uci_hash);
        acout() << "info string Hash " << uci_hash << " MB" << '\n';
        break;
      }
      if (token == "Threads") {
        is >> token;
        is >> token;
        uci_threads = stoi(token);
        thread_pool.change_thread_count(uci_threads);
        if (uci_threads == 1)
          acout() << "info string Threads " << uci_threads << " thread"
                  << '\n';
        else
          acout() << "info string Threads " << uci_threads << " threads"
                  << '\n';
        break;
      }
      if (token == "MultiPV") {
        is >> token;
        is >> token;
        uci_multipv = stoi(token);
        acout() << "info string MultiPV " << uci_multipv << '\n';
        break;
      }
      if (token == "Contempt") {
        is >> token;
        is >> token;
        uci_contempt = stoi(token);
        acout() << "info string Contempt " << uci_contempt << '\n';
        break;
      }
      if (token == "MoveOverhead") {
        is >> token;
        is >> token;
        time_control.move_overhead = stoi(token);
        acout() << "info string MoveOverhead " << time_control.move_overhead
                << " ms" << '\n';
        break;
      }
      if (token == "Ponder") {
        is >> token;
        is >> token;
        if (token == "true")
          uci_ponder = true;
        else
          uci_ponder = false;
        acout() << "info string Ponder " << uci_ponder << '\n';
        break;
      }
      if (token == "UCI_Chess960") {
        is >> token;
        is >> token;
        if (token == "true")
          uci_chess960 = true;
        else
          uci_chess960 = false;
        acout() << "info string UCI_Chess960 " << uci_chess960 << '\n';
        break;
      }
    }
  }
}

void go(position& pos, std::istringstream& is) {
  search_param param;
  std::string token;
  param.infinite = 1;

  while (is >> token) {
    if (token == "wtime") {
      is >> param.time[white];
      param.infinite = 0;
    } else if (token == "btime") {
      is >> param.time[black];
      param.infinite = 0;
    } else if (token == "winc") {
      is >> param.inc[white];
      param.infinite = 0;
    } else if (token == "binc") {
      is >> param.inc[black];
      param.infinite = 0;
    } else if (token == "movestogo") {
      is >> param.moves_to_go;
      param.infinite = 0;
    } else if (token == "depth") {
      is >> param.depth;
      param.infinite = 0;
    } else if (token == "nodes") {
      is >> param.nodes;
      param.infinite = 0;
    } else if (token == "movetime") {
      is >> param.move_time;
      param.infinite = 0;
    } else if (token == "infinite")
      param.infinite = 1;
  }
  thread_pool.begin_search(pos, param);
}

void set_position(position& pos, std::istringstream& is) {
  uint32_t move;
  std::string token, fen;

  is >> token;

  if (token == "startpos") {
    fen = startpos;
    is >> token;
  } else if (token == "fen")
    while (is >> token && token != "moves") fen += token + " ";
  else
    return;

  pos.set(fen, false, thread_pool.main());

  while (is >> token && (move = move_from_string(pos, token)) != no_move) {
    pos.play_move(move);
    pos.increase_game_ply();
  }
}

std::string trim(const std::string& str, const std::string& whitespace) {
  const auto str_begin = str.find_first_not_of(whitespace);
  if (str_begin == std::string::npos) return "";

  const auto str_end = str.find_last_not_of(whitespace);
  const auto str_range = str_end - str_begin + 1;

  return str.substr(str_begin, str_range);
}

std::string sq(const square sq) {
  return std::string{static_cast<char>('a' + file_of(sq)),
                     static_cast<char>('1' + rank_of(sq))};
}
