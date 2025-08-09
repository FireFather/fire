/*
 * Chess Engine - NNUE Evaluation (Implementation)
 * -----------------------------------------------
 * HalfKP feature transformer + AVX2 dense layers with incremental updates.
 *
 * Features:
 *   - Oriented features: board is flipped for black so weights are shared
 *   - Accumulator supports delta updates via dirty_piece tracking
 *   - Two hidden layers (32 units each) and one scalar output
 *   - Quantized int8 weights/activations; int16 accumulators
 *
 * All comments are ASCII-only to avoid UTF-8 warnings.
 */

#include "nnue.h"
#include <cassert>
#include <cstdio>
#include "main.h"
#include "util.h"

 /* Optional embedded model (non-MSVC builds) */
#ifdef NNUE_EMBEDDED
#include "incbin.h"
INCBIN(Network, NNUE_EVAL_FILE);
#endif

/*
 * File helpers (cross-platform)
 * -----------------------------
 * Thin wrappers so higher-level code stays portable.
 */

 /* open_file(name)
  * ---------------
  * Open read-only; returns FD (POSIX int or Windows HANDLE).
  */
FD open_file(const char* name) {
#ifndef _WIN32
  return open(name, O_RDONLY);
#else
  return CreateFile(name, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
    FILE_FLAG_RANDOM_ACCESS, nullptr);
#endif
}

/* close_file(fd)
 * --------------
 * Close file handle or descriptor.
 */
void close_file(FD fd) {
#ifndef _WIN32
  close(fd);
#else
  CloseHandle(fd);
#endif
}

/* file_size(fd)
 * -------------
 * Return file size in bytes.
 */
size_t file_size(FD fd) {
#ifndef _WIN32
  struct stat statbuf;
  fstat(fd, &statbuf);
  return statbuf.st_size;
#else
  DWORD size_high;
  const DWORD size_low=GetFileSize(fd, &size_high);
  return static_cast<uint64_t>(size_high) << 32 | size_low;
#endif
}

/* map_file(fd, *map)
 * ------------------
 * Map entire file read-only; returns pointer to mapped memory.
 * Fills mapping token for unmap_file().
 */
const void* map_file(FD fd, map_t* map) {
#ifndef _WIN32
  * map=file_size(fd);
  void* data=mmap(NULL, *map, PROT_READ, MAP_SHARED, fd, 0);
#ifdef MADV_RANDOM
  /* Access is roughly random while reading weights. */
  madvise(data, *map, MADV_RANDOM);
#endif
  return data == MAP_FAILED ? NULL : data;
#else
  DWORD size_high;
  const DWORD size_low=GetFileSize(fd, &size_high);
  *map=CreateFileMapping(fd, nullptr, PAGE_READONLY, size_high, size_low,
    nullptr);
  if (*map == nullptr) return nullptr;
  return MapViewOfFile(*map, FILE_MAP_READ, 0, 0, 0);
#endif
}

/* unmap_file(data, map)
 * ---------------------
 * Unmap file mapping created by map_file().
 */
void unmap_file(const void* data, map_t map) {
  if (!data) return;
#ifndef _WIN32
  munmap((void*)data, map);
#else
  UnmapViewOfFile(data);
  CloseHandle(map);
#endif
}

/*
 * Internal helpers and SIMD kernels
 * ---------------------------------
 * Anonymous namespace keeps symbols local to this translation unit.
 */
namespace {

  /*
   * orient(c, s)
   * ------------
   * Flip square index for black so the network sees a white-like board.
   * XOR with 0x3f performs a 180-degree rotation of a 0..63 index grid.
   */
  int orient(const int c, const int s) {
    return s ^ (c == white ? 0x00 : 0x3f);
  }

  /*
   * make_index(c, s, pc, ksq)
   * -------------------------
   * Build HalfKP feature index using:
   *   - oriented piece square s
   *   - per-color piece-family base offset
   *   - king square bucket ksq
   */
  unsigned make_index(const int c, const int s, const int pc, const int ksq) {
    return orient(c, s) + piece_to_index[c][pc] + ps_end * ksq;
  }

  /*
   * half_kp_append_active_indices(pos, c, active)
   * ---------------------------------------------
   * Append all active feature indices for color c (excluding the king).
   * The king square ksq is used as the K in HalfKP.
   */
  void half_kp_append_active_indices(const board* pos, const int c, index_list* active) {
    int ksq=pos->squares[c];
    ksq=orient(c, ksq);
    /* pieces[0]=wking, pieces[1]=bking; others start at index 2. */
    for (int i=2; pos->pieces[i]; i++) {
      const int sq=pos->squares[i];
      const int pc=pos->pieces[i];
      active->values[active->size++]=make_index(c, sq, pc, ksq);
    }
  }

  /*
   * half_kp_append_changed_indices(pos, c, dp, removed, added)
   * ----------------------------------------------------------
   * Append indices removed/added due to dirty_piece changes, excluding kings.
   */
  void half_kp_append_changed_indices(const board* pos, const int c,
    const dirty_piece* dp, index_list* removed, index_list* added) {
    int ksq=pos->squares[c];
    ksq=orient(c, ksq);
    for (int i=0; i < dp->dirty_num; i++) {
      const int pc=dp->pc[i];
      if (IS_KING(pc)) continue; /* king moves handled via reset */
      if (dp->from[i] != 64) removed->values[removed->size++]=make_index(c, dp->from[i], pc, ksq);
      if (dp->to[i] != 64)   added->values[added->size++]=make_index(c, dp->to[i], pc, ksq);
    }
  }

  /* append_active_indices(pos, active[2])
   * -------------------------------------
   * Wrapper to fill both colors' active lists.
   */
  void append_active_indices(const board* pos, index_list active[2]) {
    for (int c=0; c < 2; c++) half_kp_append_active_indices(pos, c, &active[c]);
  }

  /* append_changed_indices(pos, removed[2], added[2], reset[2])
   * -----------------------------------------------------------
   * Compute per-color deltas and whether a full reset is required
   * (king move in current or previous position).
   */
  void append_changed_indices(const board* pos, index_list removed[2],
    index_list added[2], bool reset[2]) {
    const dirty_piece* dp=&pos->nnue[0]->dirty_piece;
    if (pos->nnue[1]->accu.computed_accumulation) {
      for (int c=0; c < 2; c++) {
        reset[c]=dp->pc[0] == static_cast<int>(KING(c));
        if (reset[c]) half_kp_append_active_indices(pos, c, &added[c]);
        else half_kp_append_changed_indices(pos, c, dp, &removed[c], &added[c]);
      }
    } else {
      const dirty_piece* dp2=&pos->nnue[1]->dirty_piece;
      for (int c=0; c < 2; c++) {
        reset[c]=dp->pc[0] == static_cast<int>(KING(c)) ||
          dp2->pc[0] == static_cast<int>(KING(c));
        if (reset[c]) half_kp_append_active_indices(pos, c, &added[c]);
        else {
          half_kp_append_changed_indices(pos, c, dp, &removed[c], &added[c]);
          half_kp_append_changed_indices(pos, c, dp2, &removed[c], &added[c]);
        }
      }
    }
  }

  /*
   * affine_propagate(input, biases, weights)
   * ----------------------------------------
   * Output layer (32 inputs -> 1 scalar).
   *   - int8 inputs and weights
   *   - uses AVX2 maddubs + madd to widen and sum
   *   - returns int32 score (bias added), caller scales by fv_scale
   */
  int32_t affine_propagate(clipped_t* input, const int32_t* biases, weight_t* weights) {
    const auto iv=reinterpret_cast<__m256i*>(input);
    const auto row=reinterpret_cast<__m256i*>(weights);
    __m256i prod=_mm256_maddubs_epi16(iv[0], row[0]);
    prod=_mm256_madd_epi16(prod, _mm256_set1_epi16(1)); /* horizontal add */
    __m128i sum=_mm_add_epi32(_mm256_castsi256_si128(prod),
                              _mm256_extracti128_si256(prod, 1));
    sum=_mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x1b));
    return _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 1) + biases[0];
  }

  /*
   * next_idx(idx, offset, v, mask, in_dims)
   * ---------------------------------------
   * Iterate over set bits of the input activation mask in 64-bit chunks.
   * Produces the next active index in idx; returns false when exhausted.
   */
  bool next_idx(unsigned* idx, unsigned* offset, mask2_t* v, mask_t* mask, const unsigned in_dims) {
    while (*v == 0) {
      *offset+=8 * sizeof(mask2_t);
      if (*offset >= in_dims) return false;
      memcpy(v, reinterpret_cast<char*>(mask) + *offset / 8, sizeof(mask2_t));
    }
    *idx=*offset + lsb(*v);
    *v&=*v - 1; /* clear least-significant set bit */
    return true;
  }

  /*
   * affine_txfm(...)
   * ----------------
   * General affine transform for a 32-wide block:
   *   - Sparse input traversal using in_mask (skips zeros)
   *   - Two-input packing to utilize maddubs efficiently
   *   - Optional: produce sign mask of outputs (pack8_and_calc_mask)
   *   - Optional: apply ReLU (when pack8_and_calc_mask == false)
   */
  void affine_txfm(
    int8_t* input,
    void* output,
    unsigned in_dims,
    unsigned out_dims,
    int32_t* biases,
    weight_t* weights,
    mask_t* in_mask,
    mask_t* out_mask,
    bool pack8_and_calc_mask) {
    (void)out_dims;
    const __m256i k_zero=_mm256_setzero_si256();
    auto* out_vec=static_cast<__m256i*>(output);

    /* Four accumulators for 32 outputs */
    __m256i acc[4]={
      reinterpret_cast<__m256i*>(biases)[0],
      reinterpret_cast<__m256i*>(biases)[1],
      reinterpret_cast<__m256i*>(biases)[2],
      reinterpret_cast<__m256i*>(biases)[3]
    };

    /* Walk active inputs, two at a time */
    mask2_t v;
    unsigned idx=0, offset=0;
    memcpy(&v, in_mask, sizeof(mask2_t));
    while (next_idx(&idx, &offset, &v, in_mask, in_dims)) {
      __m256i first=reinterpret_cast<__m256i*>(weights)[idx];
      uint16_t factor=static_cast<uint8_t>(input[idx]);
      __m256i second;
      if (next_idx(&idx, &offset, &v, in_mask, in_dims)) {
        second=reinterpret_cast<__m256i*>(weights)[idx];
        factor|=static_cast<uint16_t>(input[idx]) << 8;
      } else {
        second=k_zero;
      }

      const __m256i mul=_mm256_set1_epi16(static_cast<int16_t>(factor));

      /* low half */
      __m256i prod=_mm256_maddubs_epi16(mul, _mm256_unpacklo_epi8(first, second));
      __m256i signs=_mm256_cmpgt_epi16(k_zero, prod);
      acc[0]=_mm256_add_epi32(acc[0], _mm256_unpacklo_epi16(prod, signs));
      acc[1]=_mm256_add_epi32(acc[1], _mm256_unpackhi_epi16(prod, signs));

      /* high half */
      prod=_mm256_maddubs_epi16(mul, _mm256_unpackhi_epi8(first, second));
      signs=_mm256_cmpgt_epi16(k_zero, prod);
      acc[2]=_mm256_add_epi32(acc[2], _mm256_unpacklo_epi16(prod, signs));
      acc[3]=_mm256_add_epi32(acc[3], _mm256_unpackhi_epi16(prod, signs));
    }

    /* Quantize to int8 */
    __m256i out16_0=_mm256_srai_epi16(_mm256_packs_epi32(acc[0], acc[1]), shift_);
    __m256i out16_1=_mm256_srai_epi16(_mm256_packs_epi32(acc[2], acc[3]), shift_);
    out_vec[0]=_mm256_packs_epi16(out16_0, out16_1);

    if (pack8_and_calc_mask) {
      /* Emit positive mask for sparsity in the next layer */
      out_mask[0]=_mm256_movemask_epi8(_mm256_cmpgt_epi8(out_vec[0], k_zero));
    } else {
      /* ReLU */
      out_vec[0]=_mm256_max_epi8(out_vec[0], k_zero);
    }
  }

  /* Feature-transformer parameters (filled by init_weights) */
  int16_t ft_biases alignas(64)[k_half_dimensions];
  int16_t ft_weights alignas(64)[k_half_dimensions * ft_in_dims];

  /*
   * refresh_accumulator(pos)
   * ------------------------
   * Rebuild accumulation from scratch for both colors.
   * Used on initial positions and when king moves force a reset.
   */
  void refresh_accumulator(const board* pos) {
    accumulator* accu=&pos->nnue[0]->accu;

    index_list active_indices[2]={};
    append_active_indices(pos, active_indices);

    for (unsigned color=0; color < 2; ++color) {
      for (unsigned tile=0; tile < k_half_dimensions / TILE_HEIGHT; ++tile) {
        const auto* bias_tile=reinterpret_cast<const vec16_t*>(&ft_biases[tile * TILE_HEIGHT]);
        auto* acc_tile=reinterpret_cast<vec16_t*>(&accu->accumulation[color][tile * TILE_HEIGHT]);

        vec16_t acc[num_regs];
        for (unsigned i=0; i < num_regs; ++i) acc[i]=bias_tile[i];

        for (size_t k=0; k < active_indices[color].size; ++k) {
          const unsigned index=active_indices[color].values[k];
          const size_t offset=static_cast<size_t>(index) * k_half_dimensions + tile * TILE_HEIGHT;
          const auto* weight_tile=reinterpret_cast<const vec16_t*>(&ft_weights[offset]);
          for (unsigned i=0; i < num_regs; ++i) acc[i]=VEC_ADD_16(acc[i], weight_tile[i]);
        }

        for (unsigned i=0; i < num_regs; ++i) acc_tile[i]=acc[i];
      }
    }
    accu->computed_accumulation=1;
  }

  /*
   * update_accumulator(pos)
   * -----------------------
   * Try to delta-update from a previous accumulation.
   * Returns true on success; false means caller should refresh.
   */
  bool update_accumulator(const board* pos) {
    accumulator* accu=&pos->nnue[0]->accu;
    if (accu->computed_accumulation) return true;

    /* Find a valid previous accumulation */
    const accumulator* prev_acc=nullptr;
    if ((!pos->nnue[1] || !(prev_acc=&pos->nnue[1]->accu)->computed_accumulation) &&
       (!pos->nnue[2] || !(prev_acc=&pos->nnue[2]->accu)->computed_accumulation)) {
      return false;
    }

    index_list removed[2]={};
    index_list added[2]={};
    bool reset[2]={};
    append_changed_indices(pos, removed, added, reset);

    for (unsigned tile=0; tile < k_half_dimensions / TILE_HEIGHT; ++tile) {
      for (unsigned color=0; color < 2; ++color) {
        const auto acc_tile=reinterpret_cast<vec16_t*>(&accu->accumulation[color][tile * TILE_HEIGHT]);
        vec16_t acc[num_regs];

        if (reset[color]) {
          const auto* bias_tile=reinterpret_cast<const vec16_t*>(&ft_biases[tile * TILE_HEIGHT]);
          for (unsigned i=0; i < num_regs; ++i) acc[i]=bias_tile[i];
        } else {
          const auto* prev_tile=reinterpret_cast<const vec16_t*>(&prev_acc->accumulation[color][tile * TILE_HEIGHT]);
          for (unsigned i=0; i < num_regs; ++i) acc[i]=prev_tile[i];

          /* remove moved-out features */
          for (size_t i=0; i < removed[color].size; ++i) {
            const unsigned index=removed[color].values[i];
            const size_t offset=static_cast<size_t>(index) * k_half_dimensions + tile * TILE_HEIGHT;
            const auto* weight_tile=reinterpret_cast<const vec16_t*>(&ft_weights[offset]);
            for (unsigned j=0; j < num_regs; ++j) acc[j]=VEC_SUB_16(acc[j], weight_tile[j]);
          }
        }

        /* add moved-in features */
        for (size_t i=0; i < added[color].size; ++i) {
          const unsigned index=added[color].values[i];
          const size_t offset=static_cast<size_t>(index) * k_half_dimensions + tile * TILE_HEIGHT;
          const auto* weight_tile=reinterpret_cast<const vec16_t*>(&ft_weights[offset]);
          for (unsigned j=0; j < num_regs; ++j) acc[j]=VEC_ADD_16(acc[j], weight_tile[j]);
        }

        for (unsigned j=0; j < num_regs; ++j) acc_tile[j]=acc[j];
      }
    }

    accu->computed_accumulation=1;
    return true;
  }

  /*
   * transform(pos, output, out_mask)
   * --------------------------------
   * Pack per-color int16 accumulations into int8, generate positive masks,
   * and order halves so index 0..255 is the side to move, 256..511 is the opponent.
   */
  void transform(const board* pos, clipped_t* output, mask_t* out_mask) {
    if (!update_accumulator(pos)) refresh_accumulator(pos);

    auto& accumulation=pos->nnue[0]->accu.accumulation;
    const int perspectives[2]={ pos->player,!pos->player };

    for (unsigned p=0; p < 2; ++p) {
      const unsigned offset=k_half_dimensions * p;
      const auto out=reinterpret_cast<vec8_t*>(&output[offset]);

      /* 256 lanes -> 32 bytes per 64-lane chunk */
      constexpr unsigned num_chunks=16 * k_half_dimensions / simd_width;
      for (unsigned i=0; i < num_chunks / 2; ++i) {
        const vec16_t s0=reinterpret_cast<vec16_t*>(&accumulation[perspectives[p]])[i * 2 + 0];
        const vec16_t s1=reinterpret_cast<vec16_t*>(&accumulation[perspectives[p]])[i * 2 + 1];
        const vec8_t packed=VEC_PACKS(s0, s1);
        out[i]=packed;
        *out_mask++=VEC_MASK_POS(packed);
      }
    }
  }

  /*
   * wt_idx(r, c, dims)
   * ------------------
   * Storage index permutation for certain network layouts.
   */
  unsigned wt_idx(const unsigned r, unsigned c, const unsigned dims) {
    (void)dims;
    if (dims > 32) {
      unsigned b=c & 0x18;
      b=b << 1 | b >> 1;
      c=(c & ~0x18) | (b & 0x18);
    }
    return c * 32 + r;
  }

  /* read_hidden_weights(w, dims, d)
   * -------------------------------
   * Read a 32 x dims weight block into packed layout; returns advanced pointer.
   */
  const char* read_hidden_weights(weight_t* w, const unsigned dims, const char* d) {
    for (unsigned r=0; r < 32; r++) for (unsigned c=0; c < dims; c++) w[wt_idx(r, c, dims)]=*d++;
    return d;
  }

  /* permute_biases(biases)
   * ----------------------
   * Reorder bias lanes to match packed weight layout.
   */
  void permute_biases(int32_t* biases) {
    const auto b=reinterpret_cast<__m128i*>(biases);
    __m128i tmp[8];
    tmp[0]=b[0]; tmp[1]=b[4];
    tmp[2]=b[1]; tmp[3]=b[5];
    tmp[4]=b[2]; tmp[5]=b[6];
    tmp[6]=b[3]; tmp[7]=b[7];
    memcpy(b, tmp, 8 * sizeof(__m128i));
  }

  /* Little-endian readers */
  uint32_t readu_le_u32(const void* p) {
    const auto q=static_cast<const uint8_t*>(p);
    return q[0] | q[1] << 8 | q[2] << 16 | q[3] << 24;
  }
  uint16_t readu_le_u16(const void* p) {
    const auto q=static_cast<const uint8_t*>(p);
    return static_cast<uint16_t>(q[0] | (q[1] << 8));
  }

  /* read_output_weights(w, d)
   * -------------------------
   * Read 32 output weights from byte stream.
   */
  void read_output_weights(weight_t* w, const char* d) {
    for (unsigned i=0; i < 32; i++) {
      const unsigned c=i;
      w[c]=*d++;
    }
  }

  /*
   * verify_net(eval_data, size)
   * ---------------------------
   * Quick integrity checks:
   *   - exact serialized size
   *   - network/header magic values at fixed offsets
   */
  bool verify_net(const void* eval_data, const size_t size) {
    constexpr uint32_t magic1=0x3e5aa6eeU;
    constexpr uint32_t magic2=0x5d69d7b8;
    constexpr uint32_t magic3=0x63337156;
    if (constexpr size_t expected_size=21022697; size != expected_size) return false;
    const auto* d=static_cast<const char*>(eval_data);
    return
      readu_le_u32(d) == nnue_version &&
      readu_le_u32(d + 4) == magic1 &&
      readu_le_u32(d + 8) == 177 &&
      readu_le_u32(d + transformer_start) == magic2 &&
      readu_le_u32(d + network_start) == magic3;
  }

  /*
   * init_weights(eval_data)
   * -----------------------
   * Decode serialized blobs into aligned global arrays.
   */
  void init_weights(const void* eval_data) {
    const char* d=static_cast<const char*>(eval_data) + transformer_start + 4;

    /* Feature transformer (biases + weights) */
    for (unsigned i=0; i < k_half_dimensions; ++i, d+=2) ft_biases[i]=static_cast<int16_t>(readu_le_u16(d));
    for (unsigned i=0; i < k_half_dimensions * ft_in_dims; ++i, d+=2) ft_weights[i]=static_cast<int16_t>(readu_le_u16(d));
    d+=4; /* reserved */

    /* Hidden 1: 32 x 512 */
    for (unsigned i=0; i < 32; ++i, d+=4) hidden1_biases[i]=static_cast<int32_t>(readu_le_u32(d));
    d=read_hidden_weights(hidden1_weights, 512, d);

    /* Hidden 2: 32 x 32 */
    for (unsigned i=0; i < 32; ++i, d+=4) hidden2_biases[i]=static_cast<int32_t>(readu_le_u32(d));
    d=read_hidden_weights(hidden2_weights, 32, d);

    /* Output: 32 -> 1 */
    output_biases[0]=static_cast<int32_t>(readu_le_u32(d));
    d+=4;
    read_output_weights(output_weights, d);

    /* Match bias lane order with packed weight layout */
    permute_biases(hidden1_biases);
    permute_biases(hidden2_biases);
  }

  /*
   * load_eval_file(eval_file)
   * -------------------------
   * Load model from disk or embedded block, verify, then init weights.
   */
  bool load_eval_file(const char* eval_file) {
    const void* eval_data=nullptr;
    map_t mapping=nullptr;
    size_t size=0;

#if !defined(_MSC_VER) && defined(NNUE_EMBEDDED)
    if (strcmp(eval_file, NNUE_EVAL_FILE) == 0) {
      eval_data=gNetworkData;
      size=gNetworkSize;
    } else
#endif
    {
      const FD fd=open_file(eval_file);
      if (fd == FD_ERR) return false;
      eval_data=map_file(fd, &mapping);
      size=file_size(fd);
      close_file(fd);
    }

    const bool success=verify_net(eval_data, size);
    if (success) init_weights(eval_data);

    if (mapping) unmap_file(eval_data, mapping);
    return success;
  }

} /* anonymous namespace */

/*
 * Public API
 * ----------
 */

 /* nnue_init(eval_file)
  * --------------------
  * Load network; print status; flush output.
  */
int nnue_init(const char* eval_file) {
  if (load_eval_file(eval_file)) acout() << "NNUE loaded" << std::endl;
  else acout() << "NNUE not found" << std::endl;
  return fflush(stdout);
}

/* nnue_evaluate_pos(pos)
 * ----------------------
 * Evaluate a board using:
 *   transform -> hidden1 -> hidden2 (ReLU) -> output -> scale
 */
int nnue_evaluate_pos(const board* pos) {
  alignas(8) mask_t input_mask[ft_out_dims / (8 * sizeof(mask_t))];
  alignas(8) mask_t hidden1_mask[8 / sizeof(mask_t)]={};
  net_data buffer;

  /* 1) Feature transform + input mask */
  transform(pos, buffer.input, input_mask);

  /* 2) Hidden layer 1 (sparse using input_mask) */
  affine_txfm(
    buffer.input,
    buffer.hidden1_out,
    ft_out_dims,
    32,
    hidden1_biases,
    hidden1_weights,
    input_mask,
    hidden1_mask,
    true
  );

  /* 3) Hidden layer 2 (dense + ReLU) */
  affine_txfm(
    buffer.hidden1_out,
    buffer.hidden2_out,
    32,
    32,
    hidden2_biases,
    hidden2_weights,
    hidden1_mask,
    nullptr,
    false
  );

  /* 4) Output and scaling */
  const int32_t raw_output=affine_propagate(
    buffer.hidden2_out,
    output_biases,
    output_weights
  );
  return raw_output / fv_scale;
}

/* nnue_evaluate(player, pieces, squares)
 * --------------------------------------
 * Convenience entry: build a minimal board view and evaluate it.
 */
int nnue_evaluate(const int player, int* pieces, int* squares) {
  nnue_data nnue;
  nnue.accu.computed_accumulation=0;

  board pos{};
  pos.player=player;
  pos.pieces=pieces;
  pos.squares=squares;
  pos.nnue[0]=&nnue;
  pos.nnue[1]=nullptr;
  pos.nnue[2]=nullptr;

  return nnue_evaluate_pos(&pos);
}
