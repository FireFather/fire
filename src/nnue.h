/*
 * Chess Engine - NNUE Interface (Header)
 * --------------------------------------
 * Declarations and constants for the NNUE evaluation subsystem.
 *
 * Overview:
 *   - HalfKP feature transformer (oriented by side-to-move)
 *   - Incremental accumulator with dirty-piece tracking
 *   - AVX2-packed fully connected layers (two hidden, one output)
 *
 * Notes:
 *   - This header exposes a minimal "board" struct used by the NNUE entry points.
 *   - We keep types and arrays aligned for SIMD efficiency.
 *   - Comments are ASCII-only to avoid UTF-8 warnings.
 */

#pragma once

 /* Platform headers for file mapping */
#ifdef _WIN32
#include <Windows.h>
#include <string>
#include <cstdint>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <immintrin.h> /* SIMD intrinsics (AVX2, SSE, etc.) */

/* OS-specific file descriptor and mapping types */
#ifdef _WIN32
using FD=HANDLE;
#define FD_ERR INVALID_HANDLE_VALUE
using map_t=HANDLE;
#else
typedef int FD;
#define FD_ERR -1
typedef size_t map_t;
#endif

/* Silence cast-qual warnings in GCC/Clang for packed weight readers */
#ifdef _MSC_VER
#else
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

/* Piece utility macros */
#define KING(c) ((c) ? bking : wking)
#define IS_KING(p) (((p) == wking) || ((p) == bking))

/* Capability flags (used on MSVC builds; not required elsewhere) */
#ifdef _MSC_VER
enum : uint8_t {
  use_avx2=1, use_sse41=1, use_sse3=1, use_sse2=1, use_sse=1, is_64_bit=1
};
#endif

/* Feature-transformer tiling and utility macros */
#define TILE_HEIGHT (num_regs * simd_width / 16)
#define CLAMP(a, b, c) ((a) < (b) ? (b) : (a) > (c) ? (c) : (a))

/* Optional embedded network file for non-MSVC builds */
#if !defined(_MSC_VER)
#define NNUE_EMBEDDED
#define NNUE_EVAL_FILE "fire-10.nnue"
#endif

/* Default evaluation file name and version */
static std::string nnue_evalfile="fire-10.nnue";
inline const char* nnue_file=nnue_evalfile.c_str();
static constexpr uint32_t nnue_version=0x7AF32F16u;

/* Piece identifiers (engine-wide) */
enum pieces : uint8_t {
  blank=0, wking, wqueen, wrook, wbishop, wknight, wpawn, bking,
  bqueen, brook, bbishop, bknight, bpawn
};

/* Piece-to-index base offsets for HalfKP feature indices */
enum : uint16_t {
  ps_w_pawn=1, ps_b_pawn=1 * 64 + 1,
  ps_w_knight=2 * 64 + 1, ps_b_knight=3 * 64 + 1,
  ps_w_bishop=4 * 64 + 1, ps_b_bishop=5 * 64 + 1,
  ps_w_rook=6 * 64 + 1, ps_b_rook=7 * 64 + 1,
  ps_w_queen=8 * 64 + 1, ps_b_queen=9 * 64 + 1,
  ps_end=10 * 64 + 1
};

/* Quantization and right-shift parameters used by affine kernels */
enum : uint8_t {
  fv_scale=16,
  shift_=6
};

/* Dimensions for feature-transformer and dense layers */
enum : uint16_t {
  k_half_dimensions=256,        /* features per side after transform */
  ft_in_dims=64 * ps_end,         /* total input dims for HalfKP */
  ft_out_dims=k_half_dimensions * 2 /* both sides concatenated */
};

/* SIMD layout (AVX2) */
enum : uint16_t {
  num_regs=16,
  simd_width=256
};

/* Serialized model offsets (transformer + dense network) */
enum {
  transformer_start=3 * 4 + 177,
  network_start=transformer_start + 4 + 2 * 256 + 2 * 256 * 64 * 641
};

/*
 * dirty_piece
 * -----------
 * Tracks up to 3 piece changes that occurred from the previous position.
 * Used to update the accumulator incrementally.
 */
using dirty_piece=struct dirty_piece {
  int dirty_num;
  int pc[3];
  int from[3];
  int to[3];
};

/*
 * accumulator
 * -----------
 * Per-color 256-lane accumulation buffer (int16) after HalfKP transform.
 * computed_accumulation == 1 means the buffers are valid for this position.
 */
using accumulator=struct accumulator {
  alignas(64) int16_t accumulation[2][256];
  int computed_accumulation;
};

/*
 * nnue_data
 * ---------
 * Bundle of the accumulator and the last move's dirty-piece info.
 * We typically keep a small ring of these across plies.
 */
using nnue_data=struct nnue_data {
  accumulator accu;
  dirty_piece dirty_piece;
};

/*
 * board
 * -----
 * Minimal board description used by the NNUE front-ends.
 * The main engine may keep richer structures; only these fields are read.
 */
using board=struct board {
  int player;         /* side to move: 0 = white, 1 = black */
  int* pieces;        /* piece list: index 0..1 are kings, then others, 0-terminated */
  int* squares;       /* parallel square list for pieces[] */
  nnue_data* nnue[3]; /* current and up to two previous nnue_data entries */
};

/* SIMD aliases and masks */
using vec16_t=__m256i; /* 16-bit packed */
using vec8_t=__m256i;  /* 8-bit packed */
using mask_t=uint32_t; /* bitmask chunk for activations */
using mask2_t=uint64_t;

/* Quantized value and weight element types */
using clipped_t=int8_t;
using weight_t=int8_t;

/*
 * index_list
 * ----------
 * Small container for active/changed feature indices per color.
 */
using index_list=struct {
  size_t size;
  unsigned values[30];
};

/* Vector helpers (AVX2) */
#define VEC_ADD_16(a, b) _mm256_add_epi16(a, b)
#define VEC_SUB_16(a, b) _mm256_sub_epi16(a, b)
#define VEC_PACKS(a, b)  _mm256_packs_epi16(a, b)
#define VEC_MASK_POS(a) \
    _mm256_movemask_epi8(_mm256_cmpgt_epi8(a, _mm256_setzero_si256()))

/* Network parameter storage (filled by init) */
inline weight_t output_weights alignas(64)[1 * 32];
inline int32_t hidden1_biases alignas(64)[32];
inline int32_t hidden2_biases alignas(64)[32];
inline int32_t output_biases[1];
inline weight_t hidden1_weights alignas(64)[64 * 512];
inline weight_t hidden2_weights alignas(64)[64 * 32];

/* Map piece and color to base index offset for HalfKP */
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

/*
 * net_data
 * --------
 * Temporary buffers used during evaluation passes.
 */
struct net_data {
  alignas(64) clipped_t input[ft_out_dims]; /* HalfKP output (both sides) */
  clipped_t hidden1_out[32];
  int8_t hidden2_out[32];
};

/* Public API */
int nnue_evaluate_pos(const board* pos);
int nnue_init(const char* eval_file);
int nnue_evaluate(int player, int* pieces, int* squares);

/* File helpers for model loading */
FD open_file(const char* name);
void close_file(FD fd);
size_t file_size(FD fd);
const void* map_file(FD fd, map_t* map);
void unmap_file(const void* data, map_t map);
