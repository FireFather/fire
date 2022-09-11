/*
  Fire is a freeware UCI chess playing engine authored by Norman Schmidt.

  Fire utilizes many state-of-the-art chess programming ideas and techniques
  which have been documented in detail at https://www.chessprogramming.org/
  and demonstrated via the very strong open-source chess engine Stockfish...
  https://github.com/official-stockfish/Stockfish.

  Fire is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or any later version.

  You should have received a copy of the GNU General Public License with
  this program: copying.txt.  If not, see <http://www.gnu.org/licenses/>.

  Thanks to Yu Nasu, Hisayori Noda. This implementation adapted from R. De Man
  and Daniel Shaw's Cfish nnue probe code https://github.com/dshawul/nnue-probe

  modified as follows via clang cpp core guidelines:
  declaration and assignment joined
  deprecated C++ headers replaced [modernize-deprecated-headers]
  macros used to declare a constant changed to 'constexpr' constant [cppcoreguidelines-macro-usage]
  parameters made const
  c-style casts replaced with C++ casts
  name does not match rule 'Global constants'
  name does not match rule 'Enum members'
  auto used when initializing with a cast to avoid duplicating the type name [modernize-use-auto]
  zero constants replaced with nullptr
  local variables made const
  cast from 'const char *' to 'char *' dropped const qualifier [clang-diagnostic-cast-qual]
  redundant C-style casts removed
*/

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

//--------------------
#ifdef _MSC_VER
#define USE_AVX2   1
#define USE_SSE41  1
#define USE_SSE3   1
#define USE_SSE2   1
#define USE_SSE    1
#define IS_64_BIT   1
#endif
//-------------------

#if defined(USE_AVX2)
#include <immintrin.h>
#elif defined(USE_SSE41)
#include <smmintrin.h>
#elif defined(USE_SSSE3)
#include <tmmintrin.h>
#elif defined(USE_SSE2)
#include <emmintrin.h>
#elif defined(USE_SSE)
#include <xmmintrin.h>
#elif defined(USE_MMX)
#include <mmintrin.h>
#elif defined(USE_NEON)
#include <arm_neon.h>
#endif

//-------------------
#include "misc.h"
//#define DLL_EXPORT
#include "nnue.h"
#include <iostream>
#undef DLL_EXPORT

#define KING(c)    ( (c) ? bking : wking )
#define IS_KING(p) ( ((p) == wking) || ((p) == bking) )
//-------------------

// Old gcc on Windows is unable to provide a 32-byte aligned stack.
// We need to hack around this when using AVX2 and AVX512.

#if defined(__GNUC__ ) && (__GNUC__ < 9) && defined(_WIN32) \
    && !defined(__clang__) && !defined(__INTEL_COMPILER) \
    &&  defined(USE_AVX2)
#define ALIGNMENT_HACK
#endif

#if defined(USE_NEON) && !defined(IS_64_BIT)
INLINE int16x8_t vmovl_high_s16(int8x16_t v)
{
	return vmovl_s16(vget_high_s16(v));
}
#endif

enum
{
	ps_w_pawn = 1,
	ps_b_pawn = 1 * 64 + 1,
	ps_w_knight = 2 * 64 + 1,
	ps_b_knight = 3 * 64 + 1,
	ps_w_bishop = 4 * 64 + 1,
	ps_b_bishop = 5 * 64 + 1,
	ps_w_rook = 6 * 64 + 1,
	ps_b_rook = 7 * 64 + 1,
	ps_w_queen = 8 * 64 + 1,
	ps_b_queen = 9 * 64 + 1,
	ps_end = 10 * 64 + 1
};

uint32_t piece_to_index[2][14] =
{
	{
	0, 0, ps_w_queen, ps_w_rook, ps_w_bishop, ps_w_knight, ps_w_pawn,
	0, ps_b_queen, ps_b_rook, ps_b_bishop, ps_b_knight, ps_b_pawn, 0
	},
	{
	0, 0, ps_b_queen, ps_b_rook, ps_b_bishop, ps_b_knight, ps_b_pawn,
	0, ps_w_queen, ps_w_rook, ps_w_bishop, ps_w_knight, ps_w_pawn, 0
	}
};

// Version of the evaluation file
static constexpr uint32_t nnue_version = 0x7AF32F16u;

// Constants used in evaluation value calculation
enum
{
	fv_scale = 16,
	shift = 6
};

enum
{
	k_half_dimensions = 256,
	ft_in_dims = 64 * ps_end, // 64 * 641
	ft_out_dims = k_half_dimensions * 2
};

// USE_MMX generates _mm_empty() instructions, so un-define if not needed
#if defined(USE_SSE2)
#undef USE_MMX
#endif

static_assert(k_half_dimensions % 256 == 0, "k_half_dimensions should be a multiple of 256");

#define VECTOR

#ifdef USE_AVX512
#define simd_width 512
typedef __m512i vec16_t;
typedef __m512i vec8_t;
typedef __mmask64 mask_t;
#define vec_add_16(a,b) _mm512_add_epi16(a,b)
#define vec_sub_16(a,b) _mm512_sub_epi16(a,b)
#define vec_packs(a,b) _mm512_packs_epi16(a,b)
#define vec_mask_pos(a) _mm512_cmpgt_epi8_mask(a,_mm512_setzero_si512())
#define num_regs 8 // only 8 are needed

#elif USE_AVX2
constexpr auto simd_width = 256;
constexpr auto num_regs = 16;

typedef __m256i vec16_t;
typedef __m256i vec8_t;
typedef uint32_t mask_t;

template<typename T1, typename T2>
constexpr auto vec_add_16(T1 a, T2 b) { return _mm256_add_epi16(a, b); }

template<typename T1, typename T2>
constexpr auto vec_sub_16(T1 a, T2 b) { return _mm256_sub_epi16(a, b); }

template<typename T1, typename T2>
constexpr auto vec_packs(T1 a, T2 b) { return _mm256_packs_epi16(a, b); }

template<typename T>
constexpr auto vec_mask_pos(T a) { return _mm256_movemask_epi8(_mm256_cmpgt_epi8(a, _mm256_setzero_si256())); }

#elif USE_SSE2
#define simd_width 128
typedef __m128i vec16_t;
typedef __m128i vec8_t;
typedef uint16_t mask_t;
#define vec_add_16(a,b) _mm_add_epi16(a,b)
#define vec_sub_16(a,b) _mm_sub_epi16(a,b)
#define vec_packs(a,b) _mm_packs_epi16(a,b)
#define vec_mask_pos(a) _mm_movemask_epi8(_mm_cmpgt_epi8(a,_mm_setzero_si128()))
#ifdef IS_64_BIT
#define num_regs 16
#else
#define num_regs 8
#endif

#elif USE_MMX
#define simd_width 64
typedef __m64 vec16_t;
typedef __m64 vec8_t;
typedef uint8_t mask_t;
#define vec_add_16(a,b) _mm_add_pi16(a,b)
#define vec_sub_16(a,b) _mm_sub_pi16(a,b)
#define vec_packs(a,b) _mm_packs_pi16(a,b)
#define vec_mask_pos(a) _mm_movemask_pi8(_mm_cmpgt_pi8(a,_mm_setzero_si64()))
#define num_regs 8

#elif USE_NEON
#define simd_width 128
typedef int16x8_t vec16_t;
typedef int8x16_t vec8_t;
typedef uint16_t mask_t;
#define vec_add_16(a,b) vaddq_s16(a,b)
#define vec_sub_16(a,b) vsubq_s16(a,b)
#define vec_packs(a,b) vcombine_s8(vqmovn_s16(a),vqmovn_s16(b))
#define vec_mask_pos(a) neon_movemask(vcgtq_s8(a,vdupq_n_u8(0)))
#ifdef IS_64_BIT
#define num_regs 16
#else
#define num_regs 8
#endif

#else
#undef VECTOR
#define simd_width 16 // dummy
typedef uint8_t mask_t; // dummy

#endif

#ifdef IS_64_BIT
typedef uint64_t mask2_t;
#else
typedef uint32_t mask2_t;
#endif

typedef int8_t clipped_t;
#if defined(USE_MMX) || (defined(USE_SSE2) && !defined(USE_AVX2))
typedef int16_t weight_t;
#else
typedef int8_t weight_t;
#endif

typedef struct
{
	size_t size;
	unsigned values[30];
} index_list;

INLINE int orient(const int c, const int s)
{
	return s ^ (c == white_nnue ? 0x00 : 0x3f);
}

INLINE unsigned make_index(const int c, const int s, const int pc, const int ksq)
{
	return orient(c, s) + piece_to_index[c][pc] + ps_end * ksq;
}

static void half_kp_append_active_indices(const Position* pos, const int c,
	index_list* active)
{
	int ksq = pos->squares[c];
	ksq = orient(c, ksq);
	for (int i = 2; pos->pieces[i]; i++)
	{
		const int sq = pos->squares[i];
		const int pc = pos->pieces[i];
		active->values[active->size++] = make_index(c, sq, pc, ksq);
	}
}

static void half_kp_append_changed_indices(const Position* pos, const int c,
	const dirty_piece* dp, index_list* removed, index_list* added)
{
	int ksq = pos->squares[c];
	ksq = orient(c, ksq);
	for (int i = 0; i < dp->dirty_num; i++)
	{
		const int pc = dp->pc[i];
		if (IS_KING(pc)) continue;
		if (dp->from[i] != 64)
			removed->values[removed->size++] = make_index(c, dp->from[i], pc, ksq);
		if (dp->to[i] != 64)
			added->values[added->size++] = make_index(c, dp->to[i], pc, ksq);
	}
}

static void append_active_indices(const Position* pos, index_list active[2])
{
	for (int c = 0; c < 2; c++)
		half_kp_append_active_indices(pos, c, &active[c]);
}

static void append_changed_indices(const Position* pos, index_list removed[2],
	index_list added[2], bool reset[2])
{
	// assert(dp->dirtyNum != 0);

	if (const dirty_piece* dp = &(pos->nnue[0]->dirtyPiece); pos->nnue[1]->accumulator.computed_accumulation)
	{
		for (int c = 0; c < 2; c++)
		{
			reset[c] = dp->pc[0] == static_cast<int>(KING(c));

			if (reset[c])
				half_kp_append_active_indices(pos, c, &added[c]);
			else
				half_kp_append_changed_indices(pos, c, dp, &removed[c], &added[c]);
		}
	}
	else
	{
		const dirty_piece* dp2 = &(pos->nnue[1]->dirtyPiece);
		for (int c = 0; c < 2; c++)
		{
			reset[c] = dp->pc[0] == static_cast<int>(KING(c))
				|| dp2->pc[0] == static_cast<int>(KING(c));

			if (reset[c])
				half_kp_append_active_indices(pos, c, &added[c]);
			else
			{
				half_kp_append_changed_indices(pos, c, dp, &removed[c], &added[c]);
				half_kp_append_changed_indices(pos, c, dp2, &removed[c], &added[c]);
			}
		}
	}
}

// InputLayer = InputSlice<256 * 2>
// out: 512 x clipped_t

// Hidden1Layer = ClippedReLu<AffineTransform<InputLayer, 32>>
// 512 x clipped_t -> 32 x int32_t -> 32 x clipped_t

// Hidden2Layer = ClippedReLu<AffineTransform<hidden1, 32>>
// 32 x clipped_t -> 32 x int32_t -> 32 x clipped_t

// OutputLayer = AffineTransform<HiddenLayer2, 1>
// 32 x clipped_t -> 1 x int32_t

#if !defined(USE_AVX512)
static weight_t hidden1_weights alignas(64)[32 * 512];
static weight_t hidden2_weights alignas(64)[32 * 32];
#else
static weight_t hidden1_weights alignas(64)[64 * 512];
static weight_t hidden2_weights alignas(64)[64 * 32];
#endif
static weight_t output_weights alignas(64)[1 * 32];

static int32_t hidden1_biases alignas(64)[32];
static int32_t hidden2_biases alignas(64)[32];
static int32_t output_biases[1];

INLINE int32_t affine_propagate(clipped_t* input, const int32_t* biases, weight_t* weights)
{
#if defined(USE_AVX2)
	const auto iv = reinterpret_cast<__m256i*>(input);
	const auto row = reinterpret_cast<__m256i*>(weights);
#if defined(USE_VNNI)
	__m256i prod = _mm256_dpbusd_epi32(_mm256_setzero_si256(), iv[0], row[0]);
#else
	__m256i prod = _mm256_maddubs_epi16(iv[0], row[0]);
	prod = _mm256_madd_epi16(prod, _mm256_set1_epi16(1));
#endif
	__m128i sum = _mm_add_epi32(
		_mm256_castsi256_si128(prod), _mm256_extracti128_si256(prod, 1));
	sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x1b));
	return _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 1) + biases[0];

#elif defined(USE_SSE2)
	__m128i* iv = (__m128i*)input;
	__m128i* row = (__m128i*)weights;
#if defined(AVOID_USE_SSSE3)
	const __m128i kOnes = _mm_set1_epi16(1);
	__m128i p0 = _mm_madd_epi16(_mm_maddubs_epi16(iv[0], row[0]), kOnes);
	__m128i p1 = _mm_madd_epi16(_mm_maddubs_epi16(iv[1], row[1]), kOnes);
	__m128i sum = _mm_add_epi32(p0, p1);
#else
	__m128i p0 = _mm_madd_epi16(iv[0], row[0]);
	__m128i p1 = _mm_madd_epi16(iv[1], row[1]);
	__m128i p2 = _mm_madd_epi16(iv[2], row[2]);
	__m128i p3 = _mm_madd_epi16(iv[3], row[3]);
	__m128i sum = _mm_add_epi32(_mm_add_epi32(p0, p1), _mm_add_epi32(p2, p3));
#endif
	sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0xb));
#if defined(USE_SSE41)
	return _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 1) + biases[0];
#else
	sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x1));
	return _mm_cvtsi128_si32(sum) + biases[0];
#endif

#elif defined(USE_MMX)
	__m64* iv = (__m64*)input;
	__m64 s0 = _mm_setzero_si64(), s1 = s0;
	__m64* row = (__m64*)weights;
	for (unsigned j = 0; j < 4; j++) {
		s0 = _mm_add_pi32(s0, _mm_madd_pi16(row[2 * j], iv[2 * j]));
		s1 = _mm_add_pi32(s1, _mm_madd_pi16(row[2 * j + 1], iv[2 * j + 1]));
	}
	__m64 sum = _mm_add_pi32(s0, s1);
	sum = _mm_add_pi32(sum, _mm_unpackhi_pi32(sum, sum));
	return _mm_cvtsi64_si32(sum) + biases[0];

#elif defined(USE_NEON)
	int8x8_t* iv = (int8x8_t*)input;
	int32x4_t sum = { biases[0] };
	int8x8_t* row = (int8x8_t*)weights;
	int16x8_t p0 = vmull_s8(iv[0], row[0]);
	int16x8_t p1 = vmull_s8(iv[1], row[1]);
	p0 = vmlal_s8(p0, iv[2], row[2]);
	sum = vpadalq_s16(sum, p0);
	p1 = vmlal_s8(p1, iv[3], row[3]);
	sum = vpadalq_s16(sum, p1);
	return sum[0] + sum[1] + sum[2] + sum[3];

#else
	int32_t sum = biases[0];
	for (unsigned j = 0; j < 32; j++)
		sum += weights[j] * input[j];
	return sum;

#endif
}

static_assert(ft_out_dims % 64 == 0, "ft_out_dims not a multiple of 64");

#ifdef VECTOR
INLINE bool next_idx(unsigned* idx, unsigned* offset, mask2_t* v,
	mask_t* mask, const unsigned in_dims)
{
	while (*v == 0)
	{
		*offset += 8 * sizeof(mask2_t);
		if (*offset >= in_dims) return false;
		memcpy(v, reinterpret_cast<char*>(mask) + (*offset / 8), sizeof(mask2_t));
	}
#ifdef IS_64_BIT
	* idx = *offset + bsf(*v);
#else
	* idx = *offset + bsf(*v);
#endif
	* v &= *v - 1;
	return true;
}

#if defined(USE_MMX) && !defined(USE_SSE)
INLINE int _mm_movemask_pi8(__m64 v)
{
	const __m64 powers = _mm_set_pi8(-128, 64, 32, 16, 8, 4, 2, 1);
	__m64 m = _mm_and_si64(v, powers);
	m = _mm_or_si64(m, _mm_srli_si64(m, 32));
	m = _mm_or_si64(m, _mm_srli_pi32(m, 16));
	m = _mm_or_si64(m, _mm_srli_pi16(m, 8));
	return _mm_cvtsi64_si32(m) & 0xff;
}
#elif defined(USE_NEON)
INLINE int neon_movemask(uint8x16_t v)
{
	const uint8_t __attribute__((aligned(16))) powers[16] =
	{ 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
	const uint8x16_t kPowers = vld1q_u8(powers);

	uint64x2_t mask = vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(vandq_u8(v, kPowers))));
	return   vgetq_lane_u8((uint8x16_t)mask, 0)
		| (vgetq_lane_u8((uint8x16_t)mask, 8) << 8);
}
#endif
#endif

#if defined(USE_AVX512)
INLINE void affine_txfm(int8_t* input, void* output, unsigned in_dims,
	unsigned out_dims, const int32_t* biases, const weight_t* weights,
	mask_t* in_mask, mask_t* out_mask, const bool pack8_and_calc_mask)
{
	assert(out_dims == 32);

	(void)out_dims;
	const __m512i kZero = _mm512_setzero_si512();
	__m512i out_0 = ((__m512i*)biases)[0];
	__m512i out_1 = ((__m512i*)biases)[1];
	__m512i first, second;
	mask2_t v;
	unsigned idx;

	memcpy(&v, in_mask, sizeof(mask2_t));
	for (unsigned offset = 0; offset < in_dims;)
	{
		if (!next_idx(&idx, &offset, &v, in_mask, in_dims))
			break;
		first = ((__m512i*)weights)[idx];
		uint16_t factor = input[idx];
		if (next_idx(&idx, &offset, &v, in_mask, in_dims))
		{
			second = ((__m512i*)weights)[idx];
			factor |= input[idx] << 8;
		}
		else {
			second = kZero;
		}
		__m512i mul = _mm512_set1_epi16(factor), prod, signs;
		prod = _mm512_maddubs_epi16(mul, _mm512_unpacklo_epi8(first, second));
		signs = _mm512_srai_epi16(prod, 15);
		out_0 = _mm512_add_epi32(out_0, _mm512_unpacklo_epi16(prod, signs));
		out_1 = _mm512_add_epi32(out_1, _mm512_unpackhi_epi16(prod, signs));
	}

	__m512i out16 = _mm512_srai_epi16(_mm512_packs_epi32(out_0, out_1), shift);

	__m256i* out_vec = (__m256i*)output;
	const __m256i kZero256 = _mm256_setzero_si256();
	out_vec[0] = _mm256_packs_epi16(
		_mm512_castsi512_si256(out16), _mm512_extracti64x4_epi64(out16, 1));
	if (pack8_and_calc_mask)
		out_mask[0] = (uint32_t)_mm256_movemask_epi8(_mm256_cmpgt_epi8(out_vec[0], kZero256));
	else
		out_vec[0] = _mm256_max_epi8(out_vec[0], kZero256);
}
#elif defined(USE_AVX2)
INLINE void affine_txfm(const int8_t* input, void* output, unsigned in_dims, unsigned out_dims, int32_t* biases, weight_t* weights,
	mask_t* in_mask, mask_t* out_mask, const bool pack8_and_calc_mask)
{
	assert(out_dims == 32);

	(void)out_dims;
	const __m256i k_zero = _mm256_setzero_si256();
	__m256i out_0 = reinterpret_cast<__m256i*>(biases)[0];
	__m256i out_1 = reinterpret_cast<__m256i*>(biases)[1];
	__m256i out_2 = reinterpret_cast<__m256i*>(biases)[2];
	__m256i out_3 = reinterpret_cast<__m256i*>(biases)[3];
	__m256i first, second;
	mask2_t v;
	unsigned idx;


	memcpy(&v, in_mask, sizeof(mask2_t));
	for (unsigned offset = 0; offset < in_dims;) {
		if (!next_idx(&idx, &offset, &v, in_mask, in_dims))
			break;

		first = reinterpret_cast<__m256i*>(weights)[idx];
		uint16_t factor = static_cast<unsigned char>(input[idx]);

		if (next_idx(&idx, &offset, &v, in_mask, in_dims))
		{
			second = reinterpret_cast<__m256i*>(weights)[idx];
			factor |= input[idx] << 8;
		}
		else
		{
			second = k_zero;
		}
		__m256i mul = _mm256_set1_epi16(static_cast<short>(factor)), prod, signs;
		prod = _mm256_maddubs_epi16(mul, _mm256_unpacklo_epi8(first, second));
		signs = _mm256_cmpgt_epi16(k_zero, prod);
		out_0 = _mm256_add_epi32(out_0, _mm256_unpacklo_epi16(prod, signs));
		out_1 = _mm256_add_epi32(out_1, _mm256_unpackhi_epi16(prod, signs));
		prod = _mm256_maddubs_epi16(mul, _mm256_unpackhi_epi8(first, second));
		signs = _mm256_cmpgt_epi16(k_zero, prod);
		out_2 = _mm256_add_epi32(out_2, _mm256_unpacklo_epi16(prod, signs));
		out_3 = _mm256_add_epi32(out_3, _mm256_unpackhi_epi16(prod, signs));
	}

	__m256i out16_0 = _mm256_srai_epi16(_mm256_packs_epi32(out_0, out_1), shift);
	__m256i out16_1 = _mm256_srai_epi16(_mm256_packs_epi32(out_2, out_3), shift);

	auto out_vec = static_cast<__m256i*>(output);
	out_vec[0] = _mm256_packs_epi16(out16_0, out16_1);
	if (pack8_and_calc_mask)
		out_mask[0] = _mm256_movemask_epi8(_mm256_cmpgt_epi8(out_vec[0], k_zero));
	else
		out_vec[0] = _mm256_max_epi8(out_vec[0], k_zero);
}
#elif AVOID_USE_SSSE3
INLINE void affine_txfm(int8_t* input, void* output, unsigned in_dims,
	unsigned out_dims, const int32_t* biases, const weight_t* weights,
	mask_t* in_mask, mask_t* out_mask, const bool pack8_and_calc_mask)
{
	assert(out_dims == 32);

	const __m128i kZeros[2] = { 0 };
	__m128i out_0 = ((__m128i*)biases)[0];
	__m128i out_1 = ((__m128i*)biases)[1];
	__m128i out_2 = ((__m128i*)biases)[2];
	__m128i out_3 = ((__m128i*)biases)[3];
	__m128i out_4 = ((__m128i*)biases)[4];
	__m128i out_5 = ((__m128i*)biases)[5];
	__m128i out_6 = ((__m128i*)biases)[6];
	__m128i out_7 = ((__m128i*)biases)[7];
	const __m128i* first, * second;
	mask2_t v;
	unsigned idx;

	memcpy(&v, in_mask, sizeof(mask2_t));
	for (unsigned offset = 0; offset < in_dims;)
	{
		if (!next_idx(&idx, &offset, &v, in_mask, in_dims))
			break;
		first = (__m128i*) & weights[out_dims * idx];
		uint16_t factor = input[idx];
		if (next_idx(&idx, &offset, &v, in_mask, in_dims))
		{
			second = (__m128i*) & weights[out_dims * idx];
			factor |= input[idx] << 8;
		}
		else
		{
			second = kZeros;
		}
		__m128i mul = _mm_set1_epi16(factor), prod, signs;
		prod = _mm_maddubs_epi16(mul, _mm_unpacklo_epi8(first[0], second[0]));
		signs = _mm_cmpgt_epi16(kZeros[0], prod);
		out_0 = _mm_add_epi32(out_0, _mm_unpacklo_epi16(prod, signs));
		out_1 = _mm_add_epi32(out_1, _mm_unpackhi_epi16(prod, signs));
		prod = _mm_maddubs_epi16(mul, _mm_unpackhi_epi8(first[0], second[0]));
		signs = _mm_cmpgt_epi16(kZeros[0], prod);
		out_2 = _mm_add_epi32(out_2, _mm_unpacklo_epi16(prod, signs));
		out_3 = _mm_add_epi32(out_3, _mm_unpackhi_epi16(prod, signs));
		prod = _mm_maddubs_epi16(mul, _mm_unpacklo_epi8(first[1], second[1]));
		signs = _mm_cmpgt_epi16(kZeros[0], prod);
		out_4 = _mm_add_epi32(out_4, _mm_unpacklo_epi16(prod, signs));
		out_5 = _mm_add_epi32(out_5, _mm_unpackhi_epi16(prod, signs));
		prod = _mm_maddubs_epi16(mul, _mm_unpackhi_epi8(first[1], second[1]));
		signs = _mm_cmpgt_epi16(kZeros[0], prod);
		out_6 = _mm_add_epi32(out_6, _mm_unpacklo_epi16(prod, signs));
		out_7 = _mm_add_epi32(out_7, _mm_unpackhi_epi16(prod, signs));
	}

	__m128i out16_0 = _mm_srai_epi16(_mm_packs_epi32(out_0, out_1), shift);
	__m128i out16_1 = _mm_srai_epi16(_mm_packs_epi32(out_2, out_3), shift);
	__m128i out16_2 = _mm_srai_epi16(_mm_packs_epi32(out_4, out_5), shift);
	__m128i out16_3 = _mm_srai_epi16(_mm_packs_epi32(out_6, out_7), shift);

	__m128i* out_vec = (__m128i*)output;
	if (pack8_and_calc_mask)
	{
		out_vec[0] = _mm_packs_epi16(out16_0, out16_1);
		out_mask[0] = _mm_movemask_epi8(_mm_cmpgt_epi8(out_vec[0], kZeros[0]));
		out_vec[1] = _mm_packs_epi16(out16_2, out16_3);
		out_mask[1] = _mm_movemask_epi8(_mm_cmpgt_epi8(out_vec[1], kZeros[0]));
	}
	else
	{
#if defined(USE_SSE41)
		out_vec[0] = _mm_max_epi8(_mm_packs_epi16(out16_0, out16_1), kZeros[0]);
		out_vec[1] = _mm_max_epi8(_mm_packs_epi16(out16_2, out16_3), kZeros[0]);
#else
		out_vec[0] = _mm_packs_epi16(_mm_max_epi16(out16_0, kZeros[0]), _mm_max_epi16(out16_1, kZeros[0]));
		out_vec[1] = _mm_packs_epi16(_mm_max_epi16(out16_2, kZeros[0]), _mm_max_epi16(out16_3, kZeros[0]));
#endif
	}
}
#elif defined(USE_SSE2)
INLINE void affine_txfm(clipped_t* input, void* output, unsigned in_dims,
	unsigned out_dims, const int32_t* biases, const weight_t* weights,
	mask_t* in_mask, mask_t* out_mask, const bool pack8_and_calc_mask)
{
	assert(out_dims == 32);

	const __m128i kZeros[4] = { 0 };
	__m128i out_0 = ((__m128i*)biases)[0];
	__m128i out_1 = ((__m128i*)biases)[1];
	__m128i out_2 = ((__m128i*)biases)[2];
	__m128i out_3 = ((__m128i*)biases)[3];
	__m128i out_4 = ((__m128i*)biases)[4];
	__m128i out_5 = ((__m128i*)biases)[5];
	__m128i out_6 = ((__m128i*)biases)[6];
	__m128i out_7 = ((__m128i*)biases)[7];
	const __m128i* first, * second;
	mask2_t v;
	unsigned idx;

	memcpy(&v, in_mask, sizeof(mask2_t));
	for (unsigned offset = 0; offset < in_dims;)
	{
		if (!next_idx(&idx, &offset, &v, in_mask, in_dims))
			break;
		first = (__m128i*) & weights[out_dims * idx];
		uint32_t factor = input[idx];
		if (next_idx(&idx, &offset, &v, in_mask, in_dims))
		{
			second = (__m128i*) & weights[out_dims * idx];
			factor |= input[idx] << 16;
		}
		else
		{
			second = kZeros;
		}
		__m128i mul = _mm_set1_epi32(factor);
		out_0 = _mm_add_epi32(out_0, _mm_madd_epi16(mul, _mm_unpacklo_epi16(first[0], second[0])));
		out_1 = _mm_add_epi32(out_1, _mm_madd_epi16(mul, _mm_unpackhi_epi16(first[0], second[0])));
		out_2 = _mm_add_epi32(out_2, _mm_madd_epi16(mul, _mm_unpacklo_epi16(first[1], second[1])));
		out_3 = _mm_add_epi32(out_3, _mm_madd_epi16(mul, _mm_unpackhi_epi16(first[1], second[1])));
		out_4 = _mm_add_epi32(out_4, _mm_madd_epi16(mul, _mm_unpacklo_epi16(first[2], second[2])));
		out_5 = _mm_add_epi32(out_5, _mm_madd_epi16(mul, _mm_unpackhi_epi16(first[2], second[2])));
		out_6 = _mm_add_epi32(out_6, _mm_madd_epi16(mul, _mm_unpacklo_epi16(first[3], second[3])));
		out_7 = _mm_add_epi32(out_7, _mm_madd_epi16(mul, _mm_unpackhi_epi16(first[3], second[3])));
	}

	__m128i out16_0 = _mm_srai_epi16(_mm_packs_epi32(out_0, out_1), shift);
	__m128i out16_1 = _mm_srai_epi16(_mm_packs_epi32(out_2, out_3), shift);
	__m128i out16_2 = _mm_srai_epi16(_mm_packs_epi32(out_4, out_5), shift);
	__m128i out16_3 = _mm_srai_epi16(_mm_packs_epi32(out_6, out_7), shift);

	__m128i* out_vec = (__m128i*)output;
	if (pack8_and_calc_mask)
	{
		out_vec[0] = _mm_packs_epi16(out16_0, out16_1);
		out_mask[0] = _mm_movemask_epi8(_mm_cmpgt_epi8(out_vec[0], kZeros[0]));
		out_vec[1] = _mm_packs_epi16(out16_2, out16_3);
		out_mask[1] = _mm_movemask_epi8(_mm_cmpgt_epi8(out_vec[1], kZeros[0]));
	}
	else
	{
		const __m128i kx07f = _mm_set1_epi16(127);
		out_vec[0] = _mm_min_epi16(_mm_max_epi16(out16_0, kZeros[0]), kx07f);
		out_vec[1] = _mm_min_epi16(_mm_max_epi16(out16_1, kZeros[0]), kx07f);
		out_vec[2] = _mm_min_epi16(_mm_max_epi16(out16_2, kZeros[0]), kx07f);
		out_vec[3] = _mm_min_epi16(_mm_max_epi16(out16_3, kZeros[0]), kx07f);
	}
}
#elif defined(USE_MMX)
INLINE void affine_txfm(clipped_t* input, void* output, unsigned in_dims,
	unsigned out_dims, const int32_t* biases, const weight_t* weights,
	mask_t* in_mask, mask_t* out_mask, const bool pack8_and_calc_mask)
{
	assert(out_dims == 32);

#if 0
	const __m64 kZeros[2] = { 0 };
	for (unsigned t = 0; t < 4; t++) {
		__m64 out_0 = ((__m64*)biases)[4 * t + 0];
		__m64 out_1 = ((__m64*)biases)[4 * t + 1];
		__m64 out_2 = ((__m64*)biases)[4 * t + 2];
		__m64 out_3 = ((__m64*)biases)[4 * t + 3];
		const __m64* first, * second;
		mask2_t v;
		unsigned idx;

		memcpy(&v, in_mask, sizeof(mask2_t));
		for (unsigned offset = 0; offset < in_dims;)
		{
			if (!next_idx(&idx, &offset, &v, in_mask, in_dims))
				break;
			first = &((__m64*) & weights[out_dims * idx])[2 * t];
			uint32_t factor = input[idx];
			if (next_idx(&idx, &offset, &v, in_mask, in_dims))
			{
				second = &((__m64*) & weights[out_dims * idx])[2 * t];
				factor |= input[idx] << 16;
			}
			else {
				second = kZeros;
			}
			__m64 mul = _mm_set1_pi32(factor);
			out_0 = _mm_add_pi32(out_0, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[0], second[0])));
			out_1 = _mm_add_pi32(out_1, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[0], second[0])));
			out_2 = _mm_add_pi32(out_2, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[1], second[1])));
			out_3 = _mm_add_pi32(out_3, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[1], second[1])));
		}

		__m64 out16_0 = _mm_srai_pi16(_mm_packs_pi32(out_0, out_1), shift);
		__m64 out16_1 = _mm_srai_pi16(_mm_packs_pi32(out_2, out_3), shift);

		__m64* out_vec = (__m64*)output;
		if (pack8_and_calc_mask)
		{
			out_vec[t] = _mm_packs_pi16(out16_0, out16_1);
			out_mask[t] = _mm_movemask_pi8(_mm_cmpgt_pi8(out_vec[t], kZeros[0]));
		}
		else {
#ifdef USE_SSE
			const __m64 kx07f = _mm_set1_pi16(127);
			out_vec[2 * t] = _mm_min_pi16(_mm_max_pi16(out16_0, kZeros[0]), kx07f);
			out_vec[2 * t + 1] = _mm_min_pi16(_mm_max_pi16(out16_1, kZeros[0]), kx07f);
#else
			const __m64 k0x7f80 = _mm_set1_pi16(0x7f80);
			const __m64 k0x0080 = _mm_set1_pi16(0x0080);
			const __m64 k0x8000 = _mm_set1_pi16(-0x8000);
			out_vec[2 * t] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_0, k0x7f80), k0x0080), k0x8000);
			out_vec[2 * t + 1] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_1, k0x7f80), k0x0080), k0x8000);
#endif
		}
	}
#else
	const __m64 kZeros[8] = { 0 };
	__m64 out_0 = ((__m64*)biases)[0];
	__m64 out_1 = ((__m64*)biases)[1];
	__m64 out_2 = ((__m64*)biases)[2];
	__m64 out_3 = ((__m64*)biases)[3];
	__m64 out_4 = ((__m64*)biases)[4];
	__m64 out_5 = ((__m64*)biases)[5];
	__m64 out_6 = ((__m64*)biases)[6];
	__m64 out_7 = ((__m64*)biases)[7];
	__m64 out_8 = ((__m64*)biases)[8];
	__m64 out_9 = ((__m64*)biases)[9];
	__m64 out_10 = ((__m64*)biases)[10];
	__m64 out_11 = ((__m64*)biases)[11];
	__m64 out_12 = ((__m64*)biases)[12];
	__m64 out_13 = ((__m64*)biases)[13];
	__m64 out_14 = ((__m64*)biases)[14];
	__m64 out_15 = ((__m64*)biases)[15];
	const __m64* first, * second;
	mask2_t v;
	unsigned idx;

	memcpy(&v, in_mask, sizeof(mask2_t));
	for (unsigned offset = 0; offset < in_dims;)
	{
		if (!next_idx(&idx, &offset, &v, in_mask, in_dims))
			break;
		first = (__m64*) & weights[out_dims * idx];
		uint32_t factor = input[idx];
		if (next_idx(&idx, &offset, &v, in_mask, in_dims))
		{
			second = (__m64*) & weights[out_dims * idx];
			factor |= input[idx] << 16;
		}
		else
		{
			second = kZeros;
		}
		__m64 mul = _mm_set1_pi32(factor);
		out_0 = _mm_add_pi32(out_0, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[0], second[0])));
		out_1 = _mm_add_pi32(out_1, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[0], second[0])));
		out_2 = _mm_add_pi32(out_2, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[1], second[1])));
		out_3 = _mm_add_pi32(out_3, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[1], second[1])));
		out_4 = _mm_add_pi32(out_4, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[2], second[2])));
		out_5 = _mm_add_pi32(out_5, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[2], second[2])));
		out_6 = _mm_add_pi32(out_6, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[3], second[3])));
		out_7 = _mm_add_pi32(out_7, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[3], second[3])));
		out_8 = _mm_add_pi32(out_8, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[4], second[4])));
		out_9 = _mm_add_pi32(out_9, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[4], second[4])));
		out_10 = _mm_add_pi32(out_10, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[5], second[5])));
		out_11 = _mm_add_pi32(out_11, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[5], second[5])));
		out_12 = _mm_add_pi32(out_12, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[6], second[6])));
		out_13 = _mm_add_pi32(out_13, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[6], second[6])));
		out_14 = _mm_add_pi32(out_14, _mm_madd_pi16(mul, _mm_unpacklo_pi16(first[7], second[7])));
		out_15 = _mm_add_pi32(out_15, _mm_madd_pi16(mul, _mm_unpackhi_pi16(first[7], second[7])));
	}

	__m64 out16_0 = _mm_srai_pi16(_mm_packs_pi32(out_0, out_1), shift);
	__m64 out16_1 = _mm_srai_pi16(_mm_packs_pi32(out_2, out_3), shift);
	__m64 out16_2 = _mm_srai_pi16(_mm_packs_pi32(out_4, out_5), shift);
	__m64 out16_3 = _mm_srai_pi16(_mm_packs_pi32(out_6, out_7), shift);
	__m64 out16_4 = _mm_srai_pi16(_mm_packs_pi32(out_8, out_9), shift);
	__m64 out16_5 = _mm_srai_pi16(_mm_packs_pi32(out_10, out_11), shift);
	__m64 out16_6 = _mm_srai_pi16(_mm_packs_pi32(out_12, out_13), shift);
	__m64 out16_7 = _mm_srai_pi16(_mm_packs_pi32(out_14, out_15), shift);

	__m64* out_vec = (__m64*)output;
	if (pack8_and_calc_mask)
	{
		out_vec[0] = _mm_packs_pi16(out16_0, out16_1);
		out_mask[0] = _mm_movemask_pi8(_mm_cmpgt_pi8(out_vec[0], kZeros[0]));
		out_vec[1] = _mm_packs_pi16(out16_2, out16_3);
		out_mask[1] = _mm_movemask_pi8(_mm_cmpgt_pi8(out_vec[1], kZeros[0]));
		out_vec[2] = _mm_packs_pi16(out16_4, out16_5);
		out_mask[2] = _mm_movemask_pi8(_mm_cmpgt_pi8(out_vec[2], kZeros[0]));
		out_vec[3] = _mm_packs_pi16(out16_6, out16_7);
		out_mask[3] = _mm_movemask_pi8(_mm_cmpgt_pi8(out_vec[3], kZeros[0]));
	}
	else
	{
#ifdef USE_SSE
		const __m64 kx07f = _mm_set1_pi16(127);
		out_vec[0] = _mm_min_pi16(_mm_max_pi16(out16_0, kZeros[0]), kx07f);
		out_vec[1] = _mm_min_pi16(_mm_max_pi16(out16_1, kZeros[0]), kx07f);
		out_vec[2] = _mm_min_pi16(_mm_max_pi16(out16_2, kZeros[0]), kx07f);
		out_vec[3] = _mm_min_pi16(_mm_max_pi16(out16_3, kZeros[0]), kx07f);
		out_vec[4] = _mm_min_pi16(_mm_max_pi16(out16_4, kZeros[0]), kx07f);
		out_vec[5] = _mm_min_pi16(_mm_max_pi16(out16_5, kZeros[0]), kx07f);
		out_vec[6] = _mm_min_pi16(_mm_max_pi16(out16_6, kZeros[0]), kx07f);
		out_vec[7] = _mm_min_pi16(_mm_max_pi16(out16_7, kZeros[0]), kx07f);
#else
		const __m64 k0x7f80 = _mm_set1_pi16(0x7f80);
		const __m64 k0x0080 = _mm_set1_pi16(0x0080);
		const __m64 k0x8000 = _mm_set1_pi16(-0x8000);
		out_vec[0] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_0, k0x7f80), k0x0080), k0x8000);
		out_vec[1] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_1, k0x7f80), k0x0080), k0x8000);
		out_vec[2] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_2, k0x7f80), k0x0080), k0x8000);
		out_vec[3] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_3, k0x7f80), k0x0080), k0x8000);
		out_vec[4] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_4, k0x7f80), k0x0080), k0x8000);
		out_vec[5] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_5, k0x7f80), k0x0080), k0x8000);
		out_vec[6] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_6, k0x7f80), k0x0080), k0x8000);
		out_vec[7] = _mm_subs_pu16(_mm_add_pi16(_mm_adds_pi16(out16_7, k0x7f80), k0x0080), k0x8000);
#endif
	}
#endif
}
#elif defined(USE_NEON)
INLINE void affine_txfm(clipped_t* input, void* output, unsigned in_dims,
	unsigned out_dims, const int32_t* biases, const weight_t* weights,
	mask_t* in_mask, mask_t* out_mask, const bool pack8_and_calc_mask)
{
	assert(out_dims == 32);

	int32x4_t out_0 = ((int32x4_t*)biases)[0];
	int32x4_t out_1 = ((int32x4_t*)biases)[1];
	int32x4_t out_2 = ((int32x4_t*)biases)[2];
	int32x4_t out_3 = ((int32x4_t*)biases)[3];
	int32x4_t out_4 = ((int32x4_t*)biases)[4];
	int32x4_t out_5 = ((int32x4_t*)biases)[5];
	int32x4_t out_6 = ((int32x4_t*)biases)[6];
	int32x4_t out_7 = ((int32x4_t*)biases)[7];
	const int8x8_t* first;
	mask2_t v;
	unsigned idx;

	memcpy(&v, in_mask, sizeof(mask2_t));
	for (unsigned offset = 0; offset < in_dims;)
	{
		if (!next_idx(&idx, &offset, &v, in_mask, in_dims))
			break;
		first = (int8x8_t*)&weights[out_dims * idx];
		int16_t factor = input[idx];

		int16x8_t prod;
		prod = vmulq_n_s16(vmovl_s8(first[0]), factor);
		out_0 = vaddq_s32(out_0, vmovl_s16(vget_low_s16(prod)));
		out_1 = vaddq_s32(out_1, vmovl_high_s16(prod));
		prod = vmulq_n_s16(vmovl_s8(first[1]), factor);
		out_2 = vaddq_s32(out_2, vmovl_s16(vget_low_s16(prod)));
		out_3 = vaddq_s32(out_3, vmovl_high_s16(prod));
		prod = vmulq_n_s16(vmovl_s8(first[2]), factor);
		out_4 = vaddq_s32(out_4, vmovl_s16(vget_low_s16(prod)));
		out_5 = vaddq_s32(out_5, vmovl_high_s16(prod));
		prod = vmulq_n_s16(vmovl_s8(first[3]), factor);
		out_6 = vaddq_s32(out_6, vmovl_s16(vget_low_s16(prod)));
		out_7 = vaddq_s32(out_7, vmovl_high_s16(prod));
	}

	int16x8_t out16_0 = vcombine_s16(vqshrn_n_s32(out_0, shift), vqshrn_n_s32(out_1, shift));
	int16x8_t out16_1 = vcombine_s16(vqshrn_n_s32(out_2, shift), vqshrn_n_s32(out_3, shift));
	int16x8_t out16_2 = vcombine_s16(vqshrn_n_s32(out_4, shift), vqshrn_n_s32(out_5, shift));
	int16x8_t out16_3 = vcombine_s16(vqshrn_n_s32(out_6, shift), vqshrn_n_s32(out_7, shift));

	if (pack8_and_calc_mask)
	{
		const int8x16_t kZero = { 0 };
		int8x16_t* out_vec = (int8x16_t*)output;
		out_vec[0] = vcombine_s8(vqmovn_s16(out16_0), vqmovn_s16(out16_1));
		out_mask[0] = neon_movemask(vcgtq_s8(out_vec[0], kZero));
		out_vec[1] = vcombine_s8(vqmovn_s16(out16_2), vqmovn_s16(out16_3));
		out_mask[1] = neon_movemask(vcgtq_s8(out_vec[1], kZero));
	}
	else
	{
		// The next step takes int8x8_t as input, so store as int8x8_t
		const int8x8_t kZero = { 0 };
		int8x8_t* out_vec = (int8x8_t*)output;
		out_vec[0] = vmax_s8(vqmovn_s16(out16_0), kZero);
		out_vec[1] = vmax_s8(vqmovn_s16(out16_1), kZero);
		out_vec[2] = vmax_s8(vqmovn_s16(out16_2), kZero);
		out_vec[3] = vmax_s8(vqmovn_s16(out16_3), kZero);
	}
}
#else /* generic fallback */
INLINE void affine_txfm(clipped_t* input, void* output, unsigned in_dims,
	unsigned out_dims, int32_t* biases, const weight_t* weights,
	mask_t* in_mask, mask_t* out_mask, const bool pack8_and_calc_mask)
{
	(void)in_mask; (void)out_mask; (void)pack8_and_calc_mask;

	int32_t tmp[out_dims];

	for (unsigned i = 0; i < out_dims; i++)
		tmp[i] = biases[i];

	for (unsigned idx = 0; idx < in_dims; idx++)
		if (input[idx])
			for (unsigned i = 0; i < out_dims; i++)
				tmp[i] += (int8_t)input[idx] * weights[out_dims * idx + i];

	clipped_t* out_vec = (clipped_t*)output;
	for (unsigned i = 0; i < out_dims; i++)
		out_vec[i] = clamp(tmp[i] >> shift, 0, 127);
}
#endif

// Input feature converter
static int16_t ft_biases alignas(64)[k_half_dimensions];
static int16_t ft_weights alignas(64)[k_half_dimensions * ft_in_dims];

#ifdef VECTOR
constexpr int tile_height = num_regs * simd_width / 16;
#endif

// Calculate cumulative value without using difference calculation
INLINE void refresh_accumulator(const Position* pos)
{
	Accumulator* accumulator = &(pos->nnue[0]->accumulator);

	index_list active_indices[2]{};
	active_indices[0].size = active_indices[1].size = 0;
	append_active_indices(pos, active_indices);

	for (unsigned c = 0; c < 2; c++)
	{
#ifdef VECTOR
		for (unsigned i = 0; i < k_half_dimensions / tile_height; i++)
		{
			const auto ft_biases_tile = reinterpret_cast<vec16_t*>(&ft_biases[i * tile_height]);
			const auto acc_tile = reinterpret_cast<vec16_t*>(&accumulator->accumulation[c][i * tile_height]);
			vec16_t acc[num_regs]{};

			for (unsigned j = 0; j < num_regs; j++)
				acc[j] = ft_biases_tile[j];

			for (size_t k = 0; k < active_indices[c].size; k++)
			{
				const unsigned index = active_indices[c].values[k];
				const unsigned offset = k_half_dimensions * index + i * tile_height;
				const auto column = reinterpret_cast<vec16_t*>(&ft_weights[offset]);

				for (unsigned j = 0; j < num_regs; j++)
					acc[j] = vec_add_16(acc[j], column[j]);
			}

			for (unsigned j = 0; j < num_regs; j++)
				acc_tile[j] = acc[j];
		}
#else
		memcpy(accumulator->accumulation[c], ft_biases,
			k_half_dimensions * sizeof(int16_t));

		for (size_t k = 0; k < active_indices[c].size; k++)
		{
			unsigned index = active_indices[c].values[k];
			unsigned offset = k_half_dimensions * index;

			for (unsigned j = 0; j < k_half_dimensions; j++)
				accumulator->accumulation[c][j] += ft_weights[offset + j];
		}
#endif
	}
	accumulator->computed_accumulation = 1;
}

// Calculate cumulative value using difference calculation if possible
INLINE bool update_accumulator(const Position* pos)
{
	Accumulator* accumulator = &(pos->nnue[0]->accumulator);
	if (accumulator->computed_accumulation)
		return true;

	Accumulator* prev_acc;
	if ((!pos->nnue[1] || !(prev_acc = &pos->nnue[1]->accumulator)->computed_accumulation)
		&& (!pos->nnue[2] || !(prev_acc = &pos->nnue[2]->accumulator)->computed_accumulation))
		return false;

	index_list removed_indices[2]{}, added_indices[2]{};
	removed_indices[0].size = removed_indices[1].size = 0;
	added_indices[0].size = added_indices[1].size = 0;
	bool reset[2];
	append_changed_indices(pos, removed_indices, added_indices, reset);

#ifdef VECTOR
	for (unsigned i = 0; i < k_half_dimensions / tile_height; i++)
	{
		for (unsigned c = 0; c < 2; c++)
		{
			const auto acc_tile = reinterpret_cast<vec16_t*>(&accumulator->accumulation[c][i * tile_height]);
			vec16_t acc[num_regs]{};

			if (reset[c])
			{
				const auto ft_b_tile = reinterpret_cast<vec16_t*>(&ft_biases[i * tile_height]);
				for (unsigned j = 0; j < num_regs; j++)
					acc[j] = ft_b_tile[j];
			}
			else
			{
				const auto prev_acc_tile = reinterpret_cast<vec16_t*>(&prev_acc->accumulation[c][i * tile_height]);
				for (unsigned j = 0; j < num_regs; j++)
					acc[j] = prev_acc_tile[j];

				// Difference calculation for the deactivated features
				for (unsigned k = 0; k < removed_indices[c].size; k++)
				{
					const unsigned index = removed_indices[c].values[k];
					const unsigned offset = k_half_dimensions * index + i * tile_height;

					const auto column = reinterpret_cast<vec16_t*>(&ft_weights[offset]);
					for (unsigned j = 0; j < num_regs; j++)
						acc[j] = vec_sub_16(acc[j], column[j]);
				}
			}

			// Difference calculation for the activated features
			for (unsigned k = 0; k < added_indices[c].size; k++)
			{
				const unsigned index = added_indices[c].values[k];
				const unsigned offset = k_half_dimensions * index + i * tile_height;

				const auto column = reinterpret_cast<vec16_t*>(&ft_weights[offset]);
				for (unsigned j = 0; j < num_regs; j++)
					acc[j] = vec_add_16(acc[j], column[j]);
			}

			for (unsigned j = 0; j < num_regs; j++)
				acc_tile[j] = acc[j];
		}
	}
#else
	for (unsigned c = 0; c < 2; c++)
	{
		if (reset[c]) {
			memcpy(accumulator->accumulation[c], ft_biases,
				k_half_dimensions * sizeof(int16_t));
		}
		else
		{
			memcpy(accumulator->accumulation[c], prev_acc->accumulation[c],
				k_half_dimensions * sizeof(int16_t));
			// Difference calculation for the deactivated features
			for (unsigned k = 0; k < removed_indices[c].size; k++)
			{
				unsigned index = removed_indices[c].values[k];
				const unsigned offset = k_half_dimensions * index;

				for (unsigned j = 0; j < k_half_dimensions; j++)
					accumulator->accumulation[c][j] -= ft_weights[offset + j];
			}
		}

		// Difference calculation for the activated features
		for (unsigned k = 0; k < added_indices[c].size; k++)
		{
			unsigned index = added_indices[c].values[k];
			const unsigned offset = k_half_dimensions * index;

			for (unsigned j = 0; j < k_half_dimensions; j++)
				accumulator->accumulation[c][j] += ft_weights[offset + j];
		}
	}
#endif

	accumulator->computed_accumulation = 1;
	return true;
}

// Convert input features
INLINE void transform(const Position* pos, clipped_t* output, mask_t* out_mask)
{
	if (!update_accumulator(pos))
		refresh_accumulator(pos);

	int16_t(*accumulation)[2][256] = &pos->nnue[0]->accumulator.accumulation;
	(void)out_mask; // avoid compiler warning

	const int perspectives[2] =
	{
		pos->player, !pos->player
	};

	for (unsigned p = 0; p < 2; p++)
	{
		const unsigned offset = k_half_dimensions * p;

#ifdef VECTOR
		constexpr unsigned num_chunks = (16 * k_half_dimensions) / simd_width;
		const auto out = reinterpret_cast<vec8_t*>(&output[offset]);
		for (unsigned i = 0; i < num_chunks / 2; i++)
		{
			const vec16_t s0 = reinterpret_cast<vec16_t*>((*accumulation)[perspectives[p]])[i * 2];
			const vec16_t s1 = reinterpret_cast<vec16_t*>((*accumulation)[perspectives[p]])[i * 2 + 1];
			out[i] = vec_packs(s0, s1);
			*out_mask++ = vec_mask_pos(out[i]);
		}
#else
		for (unsigned i = 0; i < k_half_dimensions; i++)
		{
			int16_t sum = (*accumulation)[perspectives[p]][i];
			output[offset + i] = clamp(sum, 0, 127);
		}
#endif
	}
}

struct net_data
{
	alignas(64) clipped_t input[ft_out_dims];
	clipped_t hidden1_out[32];
#if (defined(USE_SSE2) || defined(USE_MMX)) && !defined(USE_AVX2)
	int16_t hidden2_out[32];
#else
	int8_t hidden2_out[32];
#endif
};

// Evaluation function
int nnue_evaluate_pos(const Position* pos)
{
	int32_t out_value;
	alignas(8) mask_t input_mask[ft_out_dims / (8 * sizeof(mask_t))];
	alignas(8) mask_t hidden1_mask[8 / sizeof(mask_t)] = { 0 };

#ifdef ALIGNMENT_HACK // work around a bug in old gcc on Windows
	uint8_t buf[sizeof(struct NetData) + 63];
	struct NetData* b = (struct NetData*)(buf + ((((uintptr_t)buf - 1) ^ 0x3f) & 0x3f));
#define B(x) (b->x)
#else
	net_data buf{};
#define B(x) (buf.x)
#endif

	transform(pos, B(input), input_mask);

	affine_txfm(B(input), B(hidden1_out), ft_out_dims, 32,
		hidden1_biases, hidden1_weights, input_mask, hidden1_mask, true);

	affine_txfm(B(hidden1_out), B(hidden2_out), 32, 32,
		hidden2_biases, hidden2_weights, hidden1_mask, nullptr, false);

	out_value = affine_propagate(B(hidden2_out), output_biases,
		output_weights);

#if defined(USE_MMX)
	_mm_empty();
#endif

	return out_value / fv_scale;
}

static void read_output_weights(weight_t* w, const char* d)
{
	for (unsigned i = 0; i < 32; i++)
	{
		const unsigned c = i;
#if defined(USE_AVX512)
		unsigned b = c & 0x18;
		b = (b << 1) | (b >> 1);
		c = (c & ~0x18) | (b & 0x18);
#endif
		w[c] = *d++;
	}
}

INLINE unsigned wt_idx(const unsigned r, unsigned c, const unsigned dims)
{
	(void)dims;

#if defined(USE_AVX512)
	if (dims > 32)
	{
		unsigned b = c & 0x38;
		b = (b << 1) | (b >> 2);
		c = (c & ~0x38) | (b & 0x38);
	}
	else if (dims == 32)
	{
		unsigned b = c & 0x18;
		b = (b << 1) | (b >> 1);
		c = (c & ~0x18) | (b & 0x18);
	}
#elif defined(USE_AVX2)
	if (dims > 32)
	{
		unsigned b = c & 0x18;
		b = (b << 1) | (b >> 1);
		c = (c & ~0x18) | (b & 0x18);
	}
#endif

#if defined(USE_AVX512)
	return c * 64 + r + (r & ~7);
#else
	return c * 32 + r;
#endif
}

static const char* read_hidden_weights(weight_t* w, const unsigned dims, const char* d)
{
	for (unsigned r = 0; r < 32; r++)
		for (unsigned c = 0; c < dims; c++)
			w[wt_idx(r, c, dims)] = *d++;

	return d;
}

#ifdef USE_AVX2
static void permute_biases(int32_t* biases)
{
	const auto b = reinterpret_cast<__m128i*>(biases);
	__m128i tmp[8]{};
#ifdef USE_AVX512
	tmp[0] = b[0];
	tmp[1] = b[2];
	tmp[2] = b[4];
	tmp[3] = b[6];
	tmp[4] = b[1];
	tmp[5] = b[3];
	tmp[6] = b[5];
	tmp[7] = b[7];
#elif USE_AVX2
	tmp[0] = b[0];
	tmp[1] = b[4];
	tmp[2] = b[1];
	tmp[3] = b[5];
	tmp[4] = b[2];
	tmp[5] = b[6];
	tmp[6] = b[3];
	tmp[7] = b[7];
#else
#error
#endif
	memcpy(b, tmp, 8 * sizeof(__m128i));
}
#endif

enum
{
	transformer_start = 3 * 4 + 177,
	network_start = transformer_start + 4 + 2 * 256 + 2 * 256 * 64 * 641
};

static bool verify_net(const void* eval_data, const size_t size)
{
	if (size != 21022697) return false;

	const auto d = static_cast<const char*>(eval_data);
	if (readu_le_u32(d) != nnue_version)
		return false;
	if (readu_le_u32(d + 4) != 0x3e5aa6eeU)
		return false;
	if (readu_le_u32(d + 8) != 177)
		return false;
	if (readu_le_u32(d + transformer_start) != 0x5d69d7b8)
		return false;
	if (readu_le_u32(d + network_start) != 0x63337156)
		return false;

	return true;
}

static void init_weights(const void* eval_data)
{
	const char* d = static_cast<const char*>(eval_data) + transformer_start + 4;

	// Read transformer
	for (unsigned i = 0; i < k_half_dimensions; i++, d += 2)
		ft_biases[i] = static_cast<int16_t>(readu_le_u16(d));

	for (unsigned i = 0; i < k_half_dimensions * ft_in_dims; i++, d += 2)
		ft_weights[i] = static_cast<int16_t>(readu_le_u16(d));

	// Read network
	d += 4;
	for (unsigned i = 0; i < 32; i++, d += 4)
		hidden1_biases[i] = static_cast<int32_t>(readu_le_u32(d));

	d = read_hidden_weights(hidden1_weights, 512, d);
	for (unsigned i = 0; i < 32; i++, d += 4)
		hidden2_biases[i] = static_cast<int32_t>(readu_le_u32(d));

	d = read_hidden_weights(hidden2_weights, 32, d);
	for (unsigned i = 0; i < 1; i++, d += 4)
		output_biases[i] = static_cast<int32_t>(readu_le_u32(d));

	read_output_weights(output_weights, d);

#ifdef USE_AVX2
	permute_biases(hidden1_biases);
	permute_biases(hidden2_biases);
#endif
}

static bool load_eval_file(const char* eval_file)
{
	const void* eval_data;
	map_t mapping;
	size_t size;

	{
		const FD fd = open_file(eval_file);
		if (fd == FD_ERR)
			return false;

		eval_data = map_file(fd, &mapping);
		size = file_size(fd);
		close_file(fd);
	}

	const bool success = verify_net(eval_data, size);
	if (success)
		init_weights(eval_data);

	if (mapping)
		unmap_file(eval_data, mapping);

	return success;
}

/*
Interfaces
*/
void _CDECL nnue_init(const char* eval_file)
{
	fflush(stdout);

	if (load_eval_file(eval_file))
	{
		printf("NNUE found: %s\n", eval_file);
		fflush(stdout);
		return;
	}

	printf("NNUE not found: %s\n", eval_file);
	fflush(stdout);
}

int _CDECL nnue_evaluate(const int player, int* pieces, int* squares)
{
	nnue_data nnue{};
	nnue.accumulator.computed_accumulation = 0;

	Position pos{};
	pos.nnue[0] = &nnue;
	pos.nnue[1] = nullptr;
	pos.nnue[2] = nullptr;
	pos.player = player;
	pos.pieces = pieces;
	pos.squares = squares;
	return nnue_evaluate_pos(&pos);
}

