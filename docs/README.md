# Fire — UCI Chess Engine

[![Release][release-badge]][release-link]
[![Commits][commits-badge]][commits-link]

![Downloads][downloads-badge]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]

[![License][license-badge]][license-link]
[![Contributors][contributors-shield]][contributors-url]
[![Issues][issues-shield]][issues-url]

![NNUE GUI](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/nnue-gui.png)

A modern, CPU‑friendly chess engine with **NNUE** (Efficiently Updatable Neural Network) evaluation, fast move generation, and robust UCI support.

---

## Features

- C++20, 64‑bit
- Windows and Linux
- UCI protocol
- SMP (up to 256 threads)
- Alpha‑beta search
- Transposition table (up to 1024 GB)
- Ponder and MultiPV
- Chess960 (Fischer Random)
- Bench and fast perft (plus divide)
- Asynchronous console output (acout)
- NNUE evaluation (HalfKP 256x2‑32‑32)
- Visual Studio 2022 project files included

---

## Quick Start

### Download
- Prebuilt binaries: see **Releases** → [latest][release-link]

### Build from source

**Windows (Visual Studio 2022)**  
Open `fire.sln` or `fire.vcxproj` and build the `x64` configuration.

**MinGW**
```bash
./make_avx2.sh
```

**Ubuntu / GCC / Clang**
```bash
make profile-build ARCH=x86-64-avx2
```

Requirements: a C++20 compiler. AVX2 is recommended for best NNUE speed.

---

## Basic Usage (UCI)

Launch the engine in a UCI‑capable GUI, or interact on the command line:

```text
uci
isready
position startpos
go depth 10
```

Perft and divide are built in:
```text
perft 7
divide 7
```

---

## UCI Options

| Option         | Default | Range/Notes                                                |
|----------------|---------|------------------------------------------------------------|
| Hash           | 64 MB   | Size of transposition table. Up to 1024 GB.               |
| Threads        | 1       | CPU threads to use. Up to 128.                             |
| MultiPV        | 1       | Number of principal variations to output.                  |
| MoveOverhead   | 0       | Milliseconds to reserve for GUI/network latency.           |
| Ponder         | false   | Think on opponent’s time.                                  |
| UCI_Chess960   | false   | Enable Chess960 rules.                                     |
| Clear Hash     | —       | Clears and reinitializes the hash table.                   |

---

## Perft
```text
Fire 10 x64 avx2
Aug 09 2025 15:36:42
NNUE loaded
divide 7
rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
depth 7
a2a3 106743106
b2b3 133233975
c2c3 144074944
d2d3 227598692
e2e3 306138410
f2f3 102021008
g2g3 135987651
h2h3 106678423
a2a4 137077337
b2b4 134087476
c2c4 157756443
d2d4 269605599
e2e4 309478263
f2f4 119614841
g2g4 130293018
h2h4 138495290
b1a3 120142144
b1c3 148527161
g1f3 147678554
g1h3 120669525
nodes 3195901860
time 14.956 secs
nps 213686939
```
---

## NNUE

- HalfKP feature transformer with side‑to‑move orientation
- Incremental accumulator updates via dirty‑piece tracking
- AVX2‑optimized dense layers (32→32→1)

![NNUE Data Flow](https://raw.githubusercontent.com/FireFather/fire/master/docs/nuue_halfkp_data_flow.png)
![NNUE Orientation Mapping](https://raw.githubusercontent.com/FireFather/fire/master/docs/nnue_orientation_mapping.png)


_For full technical details, see [NNUE Developer Guide](NNUE.md)._

---

## What’s New

- Complete and detailed comments
- Lean codebase size optimizations
- Source footprint ~208 KB (comments included ~282 KB)
- Syzygy TBs removed (NNUE reduces TB value)
- MCTS search removed
- Code formatted to Google style
- Updated binaries in `/src` (no change to the NNUE file)

---

## Architectural Overview (condensed)

- **Core state** (`position.{h,cpp}`): piece lists, bitboards, side to move, castling, en‑passant, Zobrist keys, phase, repetition; cached attack maps and check masks.
- **Move generation** (`movegen.{h,cpp}`): templated, bitboard‑based; captures, quiets, checks, evasions, castles, pawn pushes; legal filter ensures king safety and handles en‑passant edge cases.
- **Move ordering** (`movepick.{h,cpp}`): TT move, SEE‑filtered captures, killers, counter‑moves (CMH, follow‑up CMH), quiet checks; staged picker for QS/ProbCut/recaptures.
- **Search** (`search.{h,cpp}`): iterative deepening with TT, staged picker, NNUE evaluation, quiescence; ProbCut hooks, SEE, quiet checks stage.
- **Evaluation** (`evaluate.{h,cpp}`): delegates to NNUE; fast null‑move flip via `eval_after_null_move()`.
- **TT** (`hash.{h,cpp}`): cache‑line‑aware buckets, partial key, aging, depth‑biased replacement, prefetch, `hash_full()` estimator.
- **Time** (`chrono.{h,cpp}`): per‑ply importance model, move overhead, increment awareness, ponder‑hit rescale.
- **Threads** (`thread.{h,cpp}`): lightweight pool; each worker owns its state; condition variable wake/sleep; aggregates NPS.
- **Protocol/I‑O** (`uci.{h,cpp}`, `util.{h,cpp}`): full UCI, FEN parsing, thread‑safe `acout`, move<->string helpers (incl. Chess960 castling).
- **Bitboards** (`bitboard.{h,cpp}`, `macro.h`, `main.h`): precomputed attacks, sliding attacks, enums for square/file/rank, fast LSB/MSB, prefetch.


_For a deeper explanation of move generation, see [Move Generation Developer Guide](MoveGen.md)._

<details>
<summary>Show more technical notes</summary>

- **Incremental state:** `position::play_move()` updates all keys and masks (material, pawn, bishop‑color, x‑rays, check squares, 50‑move bucket).
- **Legality:** pinned pieces tracked; king moves checked against live attack masks; en‑passant verified by recomputing slider rays through the capture square.
- **Target‑masked generators:** most `movegen` functions accept a destination mask for early pruning.
- **History tables:** global history, counter‑move history (CMH), follow‑up counters, max‑gain for tactical quiets, and evasion history.
- **Alignment & prefetch:** TT buckets aligned to cache lines; prefetch hints reduce stalls.
- **ASCII‑only comments:** avoid UTF‑8 issues in toolchains/linters.
</details>

---

## Benefits of Modern C++ (short)

- **Type safety:** strong enums, operator overloads → fewer bugs.
- **RAII:** safer threading and memory management.
- **constexpr:** compile‑time tables and masks, no runtime overhead.
- **Standard containers:** `std::array` / `std::vector` where appropriate.
- **Portable concurrency:** `std::thread`, `std::mutex`, `std::condition_variable`.
- **Intrinsics wrapped:** clean C++ interfaces over platform intrinsics.

<details>
<summary>Full comparison table</summary>

| Feature | Old‑school C engines | This engine’s C++ approach | Practical benefit |
| --- | --- | --- | --- |
| Type safety | int everywhere | Strong enums, overloads | Fewer logic bugs |
| Memory mgmt | malloc/free | RAII, vectors/arrays | No leaks, cleaner |
| Concurrency | pthread_*, globals | std::thread, atomics, CVs | Portable, safe |
| Constants | #define | constexpr tables | Faster, safer |
| Organization | 1–2 huge files | Cohesive modules | Maintainable |
| Eval | Hard‑coded PST | Modular NNUE API | Easy upgrades |
</details>

---

## Binaries

- **x64 AVX2**: fast PGO‑built binary for modern CPUs.

---

## Build Tips

- Prefer AVX2‑capable compilers for best NNUE speed.
- Use `Hash` and `Threads` UCI options to match your hardware.
- For profiling builds on Linux: `make profile-build ARCH=x86-64-avx2`.

---

## Ultra‑fast Testing

http://www.chessdom.com/fire-the-chess-engine-releases-a-new-version/

---

## Acknowledgements

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

---

## Banners

![fire_1](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_1.bmp)
![fire_2](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_2.bmp)
![fire_3](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_3.bmp)
![fire_4](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_4.bmp)
![fire_5](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_5.bmp)
![fire_6](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_6.bmp)
![fire_7](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_7.bmp)
![fire_8](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_8.bmp)
![fire_9](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_9.bmp)
![fire_10](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_10.bmp)
![fire_11](https://raw.githubusercontent.com/FireFather/fire/master/bitmaps/fire_11.bmp)

---

## More Info

http://chesslogik.wix.com/fire

[contributors-url]: https://github.com/FireFather/fire/graphs/contributors
[forks-url]: https://github.com/FireFather/fire/network/members
[stars-url]: https://github.com/FireFather/fire/stargazers
[issues-url]: https://github.com/FireFather/fire/issues

[contributors-shield]: https://img.shields.io/github/contributors/FireFather/fire?style=for-the-badge&color=blue
[forks-shield]: https://img.shields.io/github/forks/FireFather/fire?style=for-the-badge&color=blue
[stars-shield]: https://img.shields.io/github/stars/FireFather/fire?style=for-the-badge&color=blue
[issues-shield]: https://img.shields.io/github/issues/FireFather/fire?style=for-the-badge&color=blue

[license-badge]: https://img.shields.io/github/license/FireFather/fire?style=for-the-badge&label=license&color=blue
[license-link]: https://github.com/FireFather/fire/blob/main/LICENSE
[release-badge]: https://img.shields.io/github/v/release/FireFather/fire?style=for-the-badge&label=official%20release
[release-link]: https://github.com/FireFather/fire/releases/latest
[commits-badge]: https://img.shields.io/github/commits-since/FireFather/fire/latest?style=for-the-badge
[commits-link]: https://github.com/FireFather/fire/commits/main
[downloads-badge]: https://img.shields.io/github/downloads/FireFather/fire/total?style=for-the-badge&color=blue
