/*
 * Chess Engine - Utilities (Implementation)
 * ----------------------------------------
 * Misc helpers for I/O and formatting:
 *   - move_to_string / move_from_string: UCI-algebraic move text
 *   - stream operator<< for printing the board
 *   - engine_info() and build_info() printers
 * Notes: ASCII-only comments.
 */

/*
 * Chess Engine - Utility Functions (Implementation)
 * -------------------------------------------------
 * Provides miscellaneous utilities:
 *   - Thread-safe output stream wrapper (acout)
 *   - Time measurement helpers
 *   - String parsing for options and commands
 *   - Misc helpers for string formatting
 *
 * ASCII-only comments for compatibility.
 */

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
#include "util.h"

/*
 * move_to_string(move, pos)
 * -------------------------
 * Return UCI-style move string for a packed move:
 *   - Handles Chess960 castling by converting to rook destination file
 *   - Adds promotion suffix (q,r,b,n) when applicable
 */

std::string move_to_string(const uint32_t move,const position& pos){
  char s_move[6]{};
  const auto from=from_square(move);
  auto to=to_square(move);
  if(move==no_move||move==null_move) return "";
  if(move_type(move)==castle_move&&pos.is_chess960()) to=pos.castle_rook_square(to);
  s_move[0]=static_cast<char>('a'+file_of(from));
  s_move[1]=static_cast<char>('1'+rank_of(from));
  s_move[2]=static_cast<char>('a'+file_of(to));
  s_move[3]=static_cast<char>('1'+rank_of(to));
  if(move<static_cast<uint32_t>(promotion_p)) return {s_move,4};
  s_move[4]="   nbrq"[promotion_piece(move)];
  return {s_move,5};
}

/*
 * move_from_string(pos, str)
 * --------------------------
 * Parse a move string and return the encoded move id.
 * Supports:
 *   - Chess960 castling symbols O-O and O-O-O
 *   - Lower/upper case promotion piece letters
 * Tries all legal moves and returns the match or no_move.
 */

uint32_t move_from_string(const position& pos,std::string& str){
  if(pos.is_chess960()){
    if(str=="O-O")
      str=move_to_string(make_move(castle_move,pos.king(pos.on_move()),
          relative_square(pos.on_move(),g1)),
        pos);
    else if(str=="O-O-O")
      str=move_to_string(make_move(castle_move,pos.king(pos.on_move()),
          relative_square(pos.on_move(),c1)),
        pos);
  }
  if(str.length()==5) str[4]=static_cast<char>(tolower(str[4]));
  for(const auto& new_move:legal_move_list(pos)) if(str==move_to_string(new_move,pos)) return new_move;
  return no_move;
}

/*
 * operator<<(os, pos)
 * -------------------
 * Pretty-print the board as 8 ranks of 8 squares using piece_to_char.
 */

std::ostream& operator<<(std::ostream& os,const position& pos){
  auto found=false;
  for(auto r=rank_8;r>=rank_1;--r){
    for(auto f=file_a;f<=file_h;++f){
      const auto pc=piece_to_char[pos.piece_on_square(make_square(f,r))];
      for(auto i=0;i<=11;++i){
        constexpr char p_chars[]={
        'K','P','N','B','R','Q','k','p','n','b','r','q',
        };
        if(pc==p_chars[i]){
          found=true;
          break;
        }
      }
      if(found) os<<" "<<pc;
      else
        os<<" "
          <<".";
      found=false;
    }
    os<<"\n";
  }
  return os;
}

// Print engine name, version, platform, and BMI string
void engine_info(){
  std::stringstream ei;
  ei<<program<<" "<<version<<" "<<platform<<" "<<bmis<<'\n';
  acout()<<ei.str();
}

// Print build date and time using __DATE__ and __TIME__
// Example format: "Aug 09 2025 12:34:56"

void build_info(){
  const std::string months("Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec");
  std::string month,day,year;
  std::stringstream date(__DATE__);
  date>>month>>day>>year;
  std::stringstream bi;
  date>>month>>day>>year;
  bi<<month<<' '<<std::setw(2)<<std::setfill('0')<<day<<' '<<year
    <<' '<<std::setw(2)<<std::setfill('0')<<__TIME__<<'\n';
  acout()<<bi.str();
}
