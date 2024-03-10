#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include <iomanip>
#include <sstream>

#include "macro.h"
#include "main.h"
#include "movegen.h"
#include "position.h"
#include "util.h"

std::string move_to_string(const uint32_t move, const position& pos) {
  char s_move[6]{};

  const auto from = from_square(move);
  auto to = to_square(move);

  if (move == no_move || move == null_move) return "";

  if (move_type(move) == castle_move && pos.is_chess960())
    to = pos.castle_rook_square(to);

  s_move[0] = 'a' + static_cast<char>(file_of(from));
  s_move[1] = '1' + static_cast<char>(rank_of(from));
  s_move[2] = 'a' + static_cast<char>(file_of(to));
  s_move[3] = '1' + static_cast<char>(rank_of(to));

  if (move < static_cast<uint32_t>(promotion_p)) return std::string(s_move, 4);

  s_move[4] = "   nbrq"[promotion_piece(move)];
  return std::string(s_move, 5);
}

uint32_t move_from_string(const position& pos, std::string& str) {
  if (pos.is_chess960()) {
    if (str == "O-O")
      str = move_to_string(make_move(castle_move, pos.king(pos.on_move()),
        relative_square(pos.on_move(), g1)),
        pos);
    else if (str == "O-O-O")
      str = move_to_string(make_move(castle_move, pos.king(pos.on_move()),
        relative_square(pos.on_move(), c1)),
        pos);
  }

  if (str.length() == 5) str[4] = static_cast<char>(tolower(str[4]));

  for (const auto& new_move : legal_move_list(pos))
    if (str == move_to_string(new_move, pos)) return new_move;

  return no_move;
}

std::ostream& operator<<(std::ostream& os, const position& pos) {
  constexpr char p_chars[] = {
      'K', 'P', 'N', 'B', 'R', 'Q', 'k', 'p', 'n', 'b', 'r', 'q',
  };

  auto found = false;

  for (auto r = rank_8; r >= rank_1; --r) {
    for (auto f = file_a; f <= file_h; ++f) {
      const auto pc = piece_to_char[pos.piece_on_square(make_square(f, r))];
      for (auto i = 0; i <= 11; ++i) {
        if (pc == p_chars[i]) {
          found = true;
          break;
        }
      }
      if (found)
        os << " " << pc;
      else
        os << " "
        << ".";
      found = false;
    }
    os << "\n";
  }
  return os;
}

void engine_info() {
  std::stringstream ei;
  ei << program << " " << version << " " << platform << " " << bmis << '\n';
  acout() << ei.str();
}

void build_info() {
  const std::string months("Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec");
  std::string month, day, year;
  std::stringstream date(__DATE__);
  date >> month >> day >> year;
  std::stringstream bi;
  date >> month >> day >> year;
  bi << month << ' ' << std::setw(2) << std::setfill('0') << day << ' ' << year
    << ' ' << std::setw(2) << std::setfill('0') << __TIME__ << '\n';
  acout() << bi.str();
}