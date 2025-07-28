#pragma once
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <intrin.h>

#ifdef _MSC_VER
#pragma warning(disable : 4127)
#pragma warning(disable : 4244)
#endif

#ifdef _MSC_VER
#define CACHE_ALIGN __declspec(align(64))
#else
#define CACHE_ALIGN __attribute__((aligned(64)))
#endif

#define C64(x) x##ULL
enum square : int8_t;
enum side : int;

constexpr uint64_t k1=0x5555555555555555;
constexpr uint64_t k2=0x3333333333333333;
constexpr uint64_t k4=0x0f0f0f0f0f0f0f0f;
constexpr uint64_t kf=0x0101010101010101;

inline int popcnt(uint64_t b){
  b=b-(b>>1&k1);
  b=(b&k2)+(b>>2&k2);
  b=(b+(b>>4))&k4;
  b=b*kf>>56;
  return static_cast<int>(b);
}

inline int debruin[64]={
0,47,1,56,48,27,2,60,
57,49,41,37,28,16,3,61,
54,58,35,52,50,42,21,44,
38,32,29,23,17,11,4,62,
46,55,26,59,40,36,15,53,
34,51,20,43,31,22,10,45,
25,39,14,33,19,30,9,24,
13,18,8,12,7,6,5,63
};

inline square lsb(const uint64_t b){
  return static_cast<square>(debruin[0x03f79d71b4cb0a89*(b^(b-1))>>58]);
}

inline square msb(uint64_t bb){
  constexpr uint64_t debruijn64=C64(0x03f79d71b4cb0a89);
  bb|=bb>>1;
  bb|=bb>>2;
  bb|=bb>>4;
  bb|=bb>>8;
  bb|=bb>>16;
  bb|=bb>>32;
  return static_cast<square>(debruin[(bb*debruijn64)>>58]);
}

#if defined(_MSC_VER)
inline void prefetch(void* address){
  _mm_prefetch(static_cast<char*>(address), _MM_HINT_T0);
}
#elif defined(__GNUC__)
inline void prefetch(void* address) { __builtin_prefetch(address); }
#endif

constexpr auto program="Fire";
constexpr auto version="10";
constexpr auto bmis="avx2";
constexpr auto author="Norman Schmidt";
constexpr auto platform="x64";

constexpr int default_hash=64;
constexpr int max_hash=1048576;
constexpr int max_threads=256;
constexpr int max_moves=256;
constexpr int max_ply=128;
constexpr int max_pv=63;

constexpr int value_pawn=200;
constexpr int value_knight=800;
constexpr int value_bishop=850;
constexpr int value_rook=1250;
constexpr int value_queen=2500;

constexpr double score_factor=1.6;

constexpr int sort_zero=0;
constexpr int sort_max=999999;

constexpr int eval_0=0;
constexpr int draw_eval=0;
constexpr int no_eval=199999;
constexpr int pawn_eval=100*16;
constexpr int bishop_eval=360*16;

constexpr int mat_0=0;
constexpr int mat_knight=41;
constexpr int mat_bishop=42;
constexpr int mat_rook=64;
constexpr int mat_queen=127;

constexpr int see_0=0;
constexpr int see_pawn=100;
constexpr int see_knight=328;
constexpr int see_bishop=336;
constexpr int see_rook=512;
constexpr int see_queen=1016;

constexpr int score_0=0;
constexpr int score_1=1;
constexpr int draw_score=0;
constexpr int win_score=10000;

constexpr int mate_score=30256;
constexpr int longest_mate_score=mate_score-2*max_ply;
constexpr int longest_mated_score=-mate_score+2*max_ply;
constexpr int egtb_win_score=mate_score-max_ply;

constexpr int max_score=30257;
constexpr int no_score=30258;

constexpr uint8_t no_castle=0;
constexpr uint8_t white_short=1;
constexpr uint8_t white_long=2;
constexpr uint8_t black_short=4;
constexpr uint8_t black_long=8;
constexpr uint8_t all=white_short|white_long|black_short|black_long;
constexpr uint8_t castle_possible_n=16;

constexpr uint8_t no_piecetype=0;
constexpr uint8_t pt_king=1;
constexpr uint8_t pt_pawn=2;
constexpr uint8_t pt_knight=3;
constexpr uint8_t pt_bishop=4;
constexpr uint8_t pt_rook=5;
constexpr uint8_t pt_queen=6;
constexpr uint8_t pieces_without_king=7;
constexpr uint8_t num_piecetypes=8;
constexpr uint8_t all_pieces=0;

constexpr int middlegame_phase=26;

constexpr uint32_t no_move=0;
constexpr uint32_t null_move=65;

constexpr int castle_move=9<<12;
constexpr int enpassant=10<<12;
constexpr int promotion_p=11<<12;
constexpr int promotion_b=12<<12;
constexpr int promotion_r=13<<12;
constexpr int promotion_q=14<<12;

constexpr int plies=8;
constexpr int main_thread_inc=9;
constexpr int other_thread_inc=9;

constexpr int depth_0=0;
constexpr int no_depth=-6*plies;
constexpr int max_depth=max_ply*plies;

constexpr auto white=static_cast<side>(0);
constexpr auto black=static_cast<side>(1);
constexpr auto num_sides=static_cast<side>(2);

constexpr auto north=static_cast<square>(8);
constexpr auto south=static_cast<square>(-8);
constexpr auto east=static_cast<square>(1);
constexpr auto west=static_cast<square>(-1);

constexpr auto north_east=static_cast<square>(9);
constexpr auto south_east=static_cast<square>(-7);
constexpr auto south_west=static_cast<square>(-9);
constexpr auto north_west=static_cast<square>(7);
constexpr auto no_square=static_cast<square>(127);
constexpr auto num_squares=static_cast<square>(64);

constexpr auto num_files=static_cast<square>(8);
constexpr auto num_ranks=static_cast<square>(8);

enum square : int8_t{
  a1,b1,c1,d1,e1,f1,g1,h1,
  a2,b2,c2,d2,e2,f2,g2,h2,
  a3,b3,c3,d3,e3,f3,g3,h3,
  a4,b4,c4,d4,e4,f4,g4,h4,
  a5,b5,c5,d5,e5,f5,g5,h5,
  a6,b6,c6,d6,e6,f6,g6,h6,
  a7,b7,c7,d7,e7,f7,g7,h7,
  a8,b8,c8,d8,e8,f8,g8,h8
};

enum file{
  file_a,file_b,file_c,file_d,file_e,file_f,file_g,file_h
};

enum rank{
  rank_1,rank_2,rank_3,rank_4,rank_5,rank_6,rank_7,rank_8
};

enum score : int;

enum stage : uint8_t{
  normal_search,gen_good_captures,good_captures,killers_1,killers_2,gen_bxp_captures,bxp_captures,quietmoves,
  bad_captures,delayed_moves,check_evasions,gen_check_evasions,check_evasion_loop,q_search_with_checks,q_search_1,q_search_captures_1,
  q_search_check_moves,q_search_no_checks,q_search_2,q_search_captures_2,probcut,gen_probcut,probcut_captures,gen_recaptures,
  recapture_moves
};

constexpr score make_score(const int mg,const int eg){
  return static_cast<score>((static_cast<int>(mg*score_factor)<<16)+
    static_cast<int>(eg*score_factor));
}

constexpr int remake_score(const int mg,const int eg){
  return (static_cast<int>(mg)<<16)+static_cast<int>(eg);
}

inline int mg_value(const int score){
  const union{
    uint16_t u;
    int16_t s;
  } mg={static_cast<uint16_t>(static_cast<unsigned>(score+0x8000)>>16)};
  return mg.s;
}

inline int eg_value(const int score){
  const union{
    uint16_t u;
    int16_t s;
  } eg={static_cast<uint16_t>(static_cast<unsigned>(score))};
  return eg.s;
}

inline int operator/(const score score,const int i){
  return remake_score(mg_value(score)/i,eg_value(score)/i);
}

constexpr side operator~(const side color){
  return static_cast<side>(color^1);
}

constexpr square operator~(const square sq){
  return static_cast<square>(sq^56);
}

inline int mul_div(const int score,const int mul,const int div){
  return remake_score(mg_value(score)*mul/div,eg_value(score)*mul/div);
}

constexpr int gives_mate(const int ply){
  return mate_score-ply;
}

constexpr int gets_mated(const int ply){
  return -mate_score+ply;
}

constexpr square make_square(const file f,const rank r){
  return static_cast<square>((r<<3)+f);
}

constexpr file file_of(const square sq){
  return static_cast<file>(sq&7);
}

inline rank rank_of(const square sq){
  return static_cast<rank>(sq>>3);
}

constexpr square relative_square(const side color,const square sq){
  return static_cast<square>(sq^color*56);
}

constexpr rank relative_rank(const side color,const rank r){
  return static_cast<rank>(r^color*7);
}

inline rank relative_rank(const side color,const square sq){
  return relative_rank(color,rank_of(sq));
}

constexpr bool different_color(const square v1,const square v2){
  const auto val=static_cast<int>(v1)^static_cast<int>(v2);
  return (val>>3^val)&1;
}

constexpr square pawn_ahead(const side color){
  return color==white?north:south;
}

constexpr square from_square(const uint32_t move){
  return static_cast<square>(move>>6&0x3F);
}

constexpr square to_square(const uint32_t move){
  return static_cast<square>(move&0x3F);
}

constexpr int move_type(const uint32_t move){
  return static_cast<int>(move&15<<12);
}

constexpr uint8_t promotion_piece(const uint32_t move){
  return static_cast<uint8_t>(pt_knight+
    ((move>>12&15)-(promotion_p>>12)));
}

constexpr uint8_t piece_moved(const uint32_t move){
  return static_cast<uint8_t>(move>>12&7);
}

constexpr uint32_t make_move(const square from,const square to){
  return static_cast<uint32_t>(to+(from<<6));
}

constexpr uint32_t make_move(const int type,const square from,
  const square to){
  return static_cast<uint32_t>(to+(from<<6)+type);
}

constexpr bool is_ok(const uint32_t move){
  return move!=no_move&&move!=null_move;
}

template<int capacity> struct movelist{
  int move_number;
  uint32_t moves[capacity]={};

  movelist() : move_number(0){}

  void add(uint32_t move){
    if(move_number<capacity) moves[move_number++]=move;
  }

  uint32_t& operator[](int index){
    return moves[index];
  }

  const uint32_t& operator[](int index) const{
    return moves[index];
  }

  [[nodiscard]] int size() const{
    return move_number;
  }

  void resize(const int new_size){
    move_number=new_size;
  }

  void clear(){
    move_number=0;
  }

  [[nodiscard]] bool empty() const{
    return move_number==0;
  }

  int find(uint32_t move){
    for(auto i=0;i<move_number;i++) if(moves[i]==move) return i;
    return -1;
  }
};
