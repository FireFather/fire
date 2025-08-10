# fire
<div align="left">

  [![Release][release-badge]][release-link]
  [![Commits][commits-badge]][commits-link]

  ![Downloads][downloads-badge]
  [![Forks][forks-shield]][forks-url]
  [![Stargazers][stars-shield]][stars-url]
  
  [![License][license-badge]][license-link]
  [![Contributors][contributors-shield]][contributors-url]
  [![Issues][issues-shield]][issues-url]
  
</div>

![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/nnue-gui.png)

## features
- c++20
- windows & linux
- uci
- 64-bit
- smp (to 256 threads)
- alpha-beta search
- hash (to 1024 GB)
- ponder
- multiPV
- chess960 (Fischer Random)
- bench
- fast perft (& divide)
- asychronous cout (acout) class using std::unique_lock <std::mutex>
- unique NNUE (halfkp_256x2-32-32) evaluation
- visual studio 2022 project files included

## perft
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/docs/perft_7.png)

## nnue
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/docs/nuue_halfkp_data_flow.png)
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/docs/nnue_orientation_mapping.png)

## new
- complete & detailed comments
- lean & mean codebase size optimizations
- source code footprint is now 208 KB (including comments = 282 KB)
- syzygy TBs have been removed (as NNUE evaluation becomes stronger TB's become less valuable)
- MCTS search has been removed
- the source code has been reformatted via Google style guidelines
- updated binaries available in /src directory (no change to the NNUE file)

## uci options
- **Hash** -size of the hash table. default is 64 MB.
- **Threads** -number of processor threads to use. default is 1, max = 128.
- **MultiPV** -number of pv's/principal variations (lines of play) to be output. default is 1.
- **MoveOverhead** -adjust this to compensate for network and GUI latency. This is useful to avoid losses on time.
- **Ponder** -also think during opponent's time. default is false.
- **UCI_Chess960** -play chess960 (often called FRC or Fischer Random Chess). default is false.
- **Clear Hash** -clear the hash table. delete allocated memory and re-initialize.

## architectural overview

The core state: position.{h,cpp} holds the full board state (piece lists, bitboards, side to move, castling, en-passant, Zobrist keys, phase, repetition info, etc.). It precomputes and caches

Attacked squares and x-rays/pins for both sides

“Check squares” masks (where a given piece type would give check)

Per-position keys: full key, pawn key, material key, bishop color key, plus a compact key for the 50-move bucket

Move generation: movegen.{h,cpp}
Templated, bitboard-based generators for different move classes (captures only, quiets, quiet checks, check evasions, castles, pawn pushes). Supports Chess960 castling. Includes a legal filter that discards pseudo-legal moves leaving the king in check and handles the tricky en-passant legality.

Move ordering / picker: movepick.{h,cpp}

## precise staged picker that feeds the search

Hash move

Good captures (kept via SEE ≥ 0)

Killers and counter-moves

A tactical BxN (bishop-takes-knight) pattern from delayed captures

Quiet moves ordered by rich history signals (see below)

Bad captures

Any delayed bucket
Uses multiple statistics tables: global history, counter-move history (CMH), follow-up counters, max gain for tactical quiets, and a dedicated evasion history. Also has QS/ProbCut/recapture-only stages.

Search loop: search.{h,cpp}

## classic iterative deepening with a TT, a staged move picker, NNUE evaluation, and quiescence

The supporting code has support for:

ProbCut (a verification-style fast cutoff on promising captures)

SEE for capture pruning and ordering

Quiet checks stage (useful for “check extension”–friendly QS)

Evaluation: evaluate.{h,cpp}
Delegates to NNUE (nnue.h/nnue.cpp elsewhere). It builds a compact list of (piece, square) pairs and asks the network for a score. Provides eval_after_null_move() as the quick flip used in null-move pruning paths.

Transposition table: hash.{h,cpp}
Cache-line-aware buckets (3 entries per bucket with padding), partial key (16 bits), ageing, and a replacement policy that prefers deeper or more recent entries. There’s explicit prefetching and a hash_full() estimator.

Time management: chrono.{h,cpp}

## Fire uses a modern, move-importance model that spreads available time across a horizon with guardrails:

“Optimal” and “maximum” time budgets per move

Increment awareness and ponder bonus

Ponder-hit rescaling so the remaining budget keeps its proportion

Threading: thread.{h,cpp}
A lightweight thread-pool with one main search thread and optional workers. Each thread owns a threadinfo block (root position, move lists, histories, etc.) and sleeps in an idle loop behind a condition variable until woken. The pool orchestrates UCI ‘go’, wakes workers, and aggregates node counts.

Protocol / I/O: uci.{h,cpp}, util.{h,cpp}
Full UCI support (Hash, Threads, MultiPV, Contempt, MoveOverhead, Ponder, Chess960). Position parsing from startpos/FEN and moves, UCI go parameterization, and a thread-safe acout wrapper for clean output. util also has move <-> string helpers (including Chess960 castling strings).

Bitboard layer: bitboard.{h,cpp}, macro.h, main.h

Precomputed attacks (empty_attack tables) and on-the-fly sliding attacks (e.g., attack_rook_bb, attack_bishop_bb).

Compact enums for square/file/rank, directional deltas, and operator overloads for clean arithmetic on those types.

Fast popcount, LSB/MSB via De Bruijn, and prefetch wrappers.

Zobrist hashing: zobrist.{h,cpp}
Global random tables for piece-square, castling rights, en-passant file, side-to-move, and 50-move counter buckets. position maintains full, material, pawn, and bishop-color keys incrementally.

Bench / Perft: bench.{h,cpp} (+ perft elsewhere)
A fixed set of FENs, timed fixed-depth runs, nodes/time/NPS per position and overall—very useful for regression and perf tracking.

## modern features in C++

NNUE evaluation (efficient neural net on CPUs): strong midgame strength with low branching cost. The code turns board state into compact (piece, square) arrays and calls a single nnue_evaluate.

Rich move ordering stack: TT hash move, SEE-filtered good captures, killers, counter-moves (CMH + “follow-up” CMH), max gain signals, and quiet checks/pawn-push stages. This combo dramatically improves fail-high early probability.

SEE-aware capture handling: keeps most good captures early and delays the poison ones—stability and speed.

ProbCut hook: fast tactical verification to cut branches early at high depths.

Sophisticated time manager: importance curve by ply with increment, move overhead, ponder bonus, and post-ponder rescale.

Thread pool with per-thread state: avoids false sharing by giving each thread its own move lists, histories, and root position.

Cache-friendly TT design: bucket size aligned to cache line, small partial key, and age field with a biased replacement policy.

Chess960 support in move generation and UCI parsing, including castle path/safety logic.

Data flow (from GUI command to best move)
UCI input → uci::set_position() builds a position from FEN and plays the listed moves (legal-checked).

Search start → threadpool::begin_search() publishes search_param (depth, nodes, times, inc, etc.), toggles stop flags, and wakes the main thread.

Search driver (in the main thread’s begin_search()): iterative deepening loop, calls TT probe, asks movepick for the next candidate in the current stage, makes the move on a lightweight copy of position, recurses, un-makes, updates histories/TT.

Evaluation on leaves → NNUE; QS runs with captures/quiet checks; SEE and ProbCut trim the tree.

Best line & info → UCI “info” lines with PV / depth / score / nodes / nps; final “bestmove”.

## high performance-oriented details

Incremental state: position::play_move() updates keys, phase, material, pawn keys, x-rays, check masks, capture info, 50-move counter—everything needed for quick legality and hash correctness.

Fast legality checks: pinned pieces tracked in pos_info_; king moves verified against live attack masks; en-passant verified by recomputing the discovered attack rays of sliders through the capture square.

Target-masked generators: most movegen functions accept a target mask so higher-level code (quiet only, captures only, specific square) can prune at the source.

History tables everywhere: move_value_stats, counter_move_values, follow-up counters, max_gain_stats, evasion_history—all feeding ordering.

ASCII-only comments: zero UTF-8 issues in toolchains and linters.

Prefetch & alignment: TT prefetch and cache-aligned buckets reduce memory stalls.

## extensibility hot-spots

Search heuristics: Easy to drop in LMR tuning, futility/razoring, or aspiration windows around the TT score. The staged picker already supports finer buckets.

Evaluation: NNUE front-end is isolated; adding tapered or specialized terms (tempo, threats, PST mixing) is straightforward if needed around the net output.

Time control: The importance curve parameters are centralized—experimenting with moves horizon, steal ratio, or ponder bonus is low friction.

Protocol features: UCI already exposes the usual suspects (Hash, Threads, MultiPV, Ponder, Chess960). Adding custom options (e.g., LMR on/off, contempt style) is simple in uci::set_option().

## what you get with Fire 10.0:

Strong, modernized CPU-friendly engine powered by a unique NNUE.

Solid move ordering stack with CMH, killers, SEE, and tailored stages for QS and check evasion.

Robust position model with fast legality and accurate keys (material/pawn/bishop-color sub-keys are great for caching eval terms).

Practical time management and threading that scale to multi-core and pondering workflows.

Full UCI support, Chess960 compatibility, and bench/perft tooling for regression.


# Benefits of Modern C++ in Fire

Fire leans on modern C++ idioms in ways that give it real, measurable advantages over the older, more procedural C-style engines from the 90s/early 2000s like Crafty & GNU Chess, and many of the numerous old-school C engines that exist today.

Here’s how:

## 1. Type safety and expressiveness
Strong enums (enum class) for square, file, rank, side, stage, and score mean you can’t accidentally mix them up the way you might with bare int constants in C.

Operator overloading for those enums keeps code clean and self-documenting:

cpp
Copy
Edit
for (square s = a1; s <= h8; ++s) { ... } // reads like chess, not like array math
Template specialization (e.g., templated move generators for MoveGenType) allows compile-time selection of optimized code paths without runtime branching.

**Benefit: Fewer bugs from mixing concepts, faster compile-time optimization, and cleaner high-level code.**

## 2. RAII and resource management
Thread management uses RAII-friendly thread objects and std::condition_variable to handle worker lifecycle, rather than manually tracking thread IDs and wake/sleep flags in global state.

Transposition table allocation and other memory setup are wrapped in initialization/destruction functions that respect object lifetime, reducing leaks and undefined behavior.

**Benefit: Safer, cleaner multithreading and fewer memory leaks, without the spaghetti of manual malloc/free + pthread calls.**

## 3. Inline and constexpr
constexpr tables for bitboards, masks, and direction deltas let the compiler fold values at compile time, removing runtime overhead.

inline attack generation functions (like attack_rook_bb) avoid function call cost while still keeping the code organized into logical units.

**Benefit: The compiler optimizes heavily while the source remains readable and maintainable.**

## 4. Standard library containers and algorithms
std::array for fixed-size attack tables and constants (instead of raw C arrays) — safe bounds checking in debug mode, trivially constexpr-friendly.

std::vector for dynamic lists like perft move lists or PV lines, replacing manually tracked C arrays and counters.

**Benefit: Cleaner code with less boilerplate for allocation and bounds tracking.**

## 5. Modern concurrency
Uses std::thread, std::mutex, and std::condition_variable instead of platform-specific APIs (pthread_* or CreateThread).

Thread-pool is portable across compilers and OSes without #ifdef clutter.

Atomic variables from <atomic> for stop flags ensure correctness without undefined behavior.

**Benefit: Portable, race-condition-safe threading without platform lock-in.**

## 6. Bitboard optimizations with modern intrinsics
Intrinsics like __builtin_popcountll (GCC/Clang) or _BitScanForward64 (MSVC) are wrapped in inline functions with clean C++ interfaces.

constexpr fallback popcount/LSB for compilers without the intrinsics.

**Benefit: Keeps the performance of old-school bit hacks, but with a clean, unified API.**

## 7. Template-based move picker & search tuning
Move pickers are specialized via template parameters for different search contexts (full-width search, quiescence, ProbCut, evasions).

This avoids runtime switches, letting the compiler inline the optimal path.

**Benefit: More efficient branching and better instruction cache usage compared to monolithic, switch-heavy C loops.**

## 8. Better code organization
Small, cohesive .cpp/.h modules: position, movegen, movepick, search, evaluate, hash, etc.

Clear separation of data (e.g., Zobrist tables, constants) from logic (e.g., movegen, search loop).

Inline documentation and readable identifiers — easier for others to contribute.

**Benefit: Large-scale maintainability — harder to achieve in older C engines that tend toward huge single files with sprawling globals.**

## 9. Compile-time configurability
Uses constexpr and compile-time constants for scoring scales, masks, and piece values, meaning these are baked into the binary with zero runtime cost.

Allows easy tuning in code without reworking data structures.

**Benefit: Fast tuning cycles without runtime indirection.**

## 10. NNUE integration
Neural net evaluation is cleanly integrated via a C++ wrapper API, with type-safe data structures feeding it (piece, square) features.

This contrasts with old C engines, where integrating an NNUE eval would require significant restructuring.

**Benefit: Modern evaluation techniques slot in without ripping apart the search code.**

# Summary Table

| Feature | Old-school C engines | This engine’s C++ approach | Practical Benefit |
| --- | --- | --- | --- |
| Type safety | int everywhere | Strong enums, operator overloads | Fewer logic bugs |
| Memory management | malloc/free | RAII, std::vector, std::array | No leaks, cleaner |
| Concurrency | pthread_*, globals | std::thread, <atomic>, condition variables | Portable, safe |
| Compile-time constants | #define, macros | constexpr tables | Faster, safer |
| Module organization | 1–2 huge files | Small cohesive modules | Maintainable |
| Eval integration | Hard-coded eval | Modular NNUE API | Easy upgrades |

## binaries
- **x64 avx2** = fast pgo binary (targeting modern 64-bit systems w/ AVX2 instruction set)

## compile it
- **windows** (visual studio) use included project files: fire.vcxproj or fire.sln
- **minGW** run the included shell script: make_avx2.sh
- **ubuntu** type 'make profile-build ARCH=x86-64-avx2', etc.

## ultra-fast testing
http://www.chessdom.com/fire-the-chess-engine-releases-a-new-version/

## acknowledgements
- [Chess Programming Wiki](https://www.chessprogramming.org)
- [Stockfish](https://github.com/official-stockfish/Stockfish)
- [Ippolit](https://github.com/FireFather/ippolit)
- [Robbolito](https://github.com/FireFather/robbolito)
- [Firebird](https://github.com/FireFather/firebird)
- [Ivanhoe](https://www.chessprogramming.org/IvanHoe)
- [Houdini](https://www.cruxis.com/chess/houdini.htm)
- [Gull](https://github.com/FireFather/seagull)
- [nnue-probe](https://github.com/dshawul/nnue-probe/)
- [LittleBlitzer](http://www.kimiensoftware.com)

## banners
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_1.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_2.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_3.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_4.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_5.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_6.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_7.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_8.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_9.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_10.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_11.bmp)

## more info
**http://chesslogik.wix.com/fire**

[contributors-url]:https://github.com/FireFather/fire/graphs/contributors
[forks-url]:https://github.com/FireFather/fire/network/members
[stars-url]:https://github.com/FireFather/fire/stargazers
[issues-url]:https://github.com/FireFather/fire/issues

[contributors-shield]:https://img.shields.io/github/contributors/FireFather/fire?style=for-the-badge&color=blue
[forks-shield]:https://img.shields.io/github/forks/FireFather/fire?style=for-the-badge&color=blue
[stars-shield]:https://img.shields.io/github/stars/FireFather/fire?style=for-the-badge&color=blue
[issues-shield]:https://img.shields.io/github/issues/FireFather/fire?style=for-the-badge&color=blue

[license-badge]:https://img.shields.io/github/license/FireFather/fire?style=for-the-badge&label=license&color=blue
[license-link]:https://github.com/FireFather/fire/blob/main/LICENSE
[release-badge]:https://img.shields.io/github/v/release/FireFather/fire?style=for-the-badge&label=official%20release
[release-link]:https://github.com/FireFather/fire/releases/latest
[commits-badge]:https://img.shields.io/github/commits-since/FireFather/fire/latest?style=for-the-badge
[commits-link]:https://github.com/FireFather/fire/commits/main
[downloads-badge]:https://img.shields.io/github/downloads/FireFather/fire/total?style=for-the-badge&color=blue
