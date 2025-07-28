#pragma once
#ifdef _WIN32
#include <Windows.h>
#include <string>
#include <cstdint>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <immintrin.h>

#ifdef _WIN32
using FD=HANDLE;
#define FD_ERR INVALID_HANDLE_VALUE
using map_t=HANDLE;
#else
typedef int FD;
#define FD_ERR -1
typedef size_t map_t;
#endif

#ifdef _MSC_VER
#else
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

#define KING(c) ((c) ? bking : wking)
#define IS_KING(p) (((p) == wking) || ((p) == bking))

#ifdef _MSC_VER
enum:uint8_t{
  use_avx2=1,use_sse41=1,use_sse3=1,use_sse2=1,use_sse=1,is_64_bit=1
};
#endif

#define TILE_HEIGHT (num_regs * simd_width / 16)
#define CLAMP(a, b, c) ((a) < (b) ? (b) : (a) > (c) ? (c) : (a))

#if !defined(_MSC_VER)
#define NNUE_EMBEDDED
#define NNUE_EVAL_FILE "fire-10.nnue"
#endif

static std::string nnue_evalfile="fire-10.nnue";
inline const char* nnue_file=nnue_evalfile.c_str();
static constexpr uint32_t nnue_version=0x7AF32F16u;

enum pieces : uint8_t{
  blank=0,wking,wqueen,wrook,wbishop,wknight,wpawn,bking,
  bqueen,brook,bbishop,bknight,bpawn
};

enum : uint16_t{
  ps_w_pawn=1,ps_b_pawn=1*64+1,ps_w_knight=2*64+1,ps_b_knight=3*64+1,ps_w_bishop=4*64+1,ps_b_bishop=5*64+1,ps_w_rook=6*64+1,ps_b_rook=7*64+1,
  ps_w_queen=8*64+1,ps_b_queen=9*64+1,ps_end=10*64+1
};

enum : uint8_t{
  fv_scale=16,shift_=6
};

enum : uint16_t{
  k_half_dimensions=256,ft_in_dims=64*ps_end,ft_out_dims=k_half_dimensions*2
};

enum : uint16_t{
  num_regs=16,simd_width=256
};

enum{
  transformer_start=3*4+177,network_start=transformer_start+4+2*256+2*256*64*641
};

using dirty_piece=struct dirty_piece{
  int dirty_num;
  int pc[3];
  int from[3];
  int to[3];
};

using accumulator=struct accumulator{
  alignas(64) int16_t accumulation[2][256];
  int computed_accumulation;
};

using nnue_data=struct nnue_data{
  accumulator accu;
  dirty_piece dirty_piece;
};

using board=struct board{
  int player;
  int* pieces;
  int* squares;
  nnue_data* nnue[3];
};

using vec16_t=__m256i;
using vec8_t=__m256i;
using mask_t=uint32_t;
using mask2_t=uint64_t;
using clipped_t=int8_t;
using weight_t=int8_t;

using index_list=struct{
  size_t size;
  unsigned values[30];
};

#define VEC_ADD_16(a, b) _mm256_add_epi16(a, b)
#define VEC_SUB_16(a, b) _mm256_sub_epi16(a, b)
#define VEC_PACKS(a, b) _mm256_packs_epi16(a, b)
#define VEC_MASK_POS(a) \
  _mm256_movemask_epi8(_mm256_cmpgt_epi8(a, _mm256_setzero_si256()))

inline weight_t output_weights alignas(64)[1*32];
inline int32_t hidden1_biases alignas(64)[32];
inline int32_t hidden2_biases alignas(64)[32];
inline int32_t output_biases[1];
inline weight_t hidden1_weights alignas(64)[64*512];
inline weight_t hidden2_weights alignas(64)[64*32];

inline uint32_t piece_to_index[2][14]={
{
0,0,ps_w_queen,ps_w_rook,ps_w_bishop,ps_w_knight,ps_w_pawn,0,
ps_b_queen,ps_b_rook,ps_b_bishop,ps_b_knight,ps_b_pawn,0
},
{
0,0,ps_b_queen,ps_b_rook,ps_b_bishop,ps_b_knight,ps_b_pawn,0,
ps_w_queen,ps_w_rook,ps_w_bishop,ps_w_knight,ps_w_pawn,0
}
};

struct net_data{
  alignas(64) clipped_t input[ft_out_dims];
  clipped_t hidden1_out[32];
  int8_t hidden2_out[32];
};

int nnue_evaluate_pos(const board* pos);
int nnue_init(const char* eval_file);
int nnue_evaluate(int player,int* pieces,int* squares);
FD open_file(const char* name);
void close_file(FD fd);
size_t file_size(FD fd);
const void* map_file(FD fd,map_t* map);
void unmap_file(const void* data,map_t map);
