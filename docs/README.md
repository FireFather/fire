# fire-zero

![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/nnue-gui.png)

fire 8.2 self-play nnue

- fire without NNUE can be found here: https://github.com/FireFather/fire
- fire w/ NNUE from shared SF & LC0 data can be found here: https://github.com/FireFather/fire-NN

## features
- c++17
- windows & linux
- uci
- 64-bit
- smp (to 256 threads)
- configurable hash (to 1024 GB)
- ponder
- multiPV
- analysis (infinite) mode
- chess960 (Fischer Random)
- syzygy tablebases
- adjustable contempt setting
- fast perft & divide
- bench (includes ttd time-to-depth calculation)
- timestamped bench, perft/divide, and tuner logs
- asychronous cout (acout) class using std::unique_lock<std::mutex>
- uci option searchtype random w/ uniform_real_distribution & mesenne_twister_engine
- reads engine.conf on startup for search, eval, pawn, and material parameters
- uses a unique NNUE evaluation (halfkp_256x2-32-32)
- fast alpha-beta search

## tools used:

- engine for generation of self-play games
  - fire 8.2 https://github.com/FireFather/fire
- application to run self-play games
  - cutechess-cli https://github.com/cutechess/cutechess
- application to obtain fen positions and evaluation data from pgn files & convert to plain text
  - nnue-extract.exe from deeds tools https://outskirts.altervista.org/forum/viewtopic.php?t=2009
- windows application for nnue training session management
  - nnue-gui https://github.com/FireFather/nnue-gui
- optimized port of the original nodchip shogi neural network implementation
  - sf-nnue https://github.com/FireFather/sf-nnue
- application used by nnue-extract to extract fen positions and info from pgns
  - pgn-extract https://www.cs.kent.ac.uk/people/staff/djb/pgn-extract/
- application for testing nnue vs nnue, etc.
  - little blitzer http://www.kimiensoftware.com/software/chess/littleblitzer
- utility for removing duplicate games from pgn files
  - chessbase 16 64-bit https://shop.chessbase.com/en/products/chessbase_16_fritz18_bundle
- misc utilities: truncate.exe (opening book ply limitation) & gameSplit.exe (break up pgns to multiple parts)
  - 40H-pgn tools http://40hchess.epizy.com/

## instructions to create an efficiently-updatable neural network (nnue) for any engine:
- setting up a suitable Windows environment
  - [windows-environment.md](docs/windows-environment.md)
- creating an efficient opening book using cutechess-cli
  - [cutechess-opening-books.md](docs/cutechess-opening-books.md)
- running selfplay games for a 'zero' nnue approach
  - [engine-selfplay-via-cutechess-cli.md](docs/engine-selfplay-via-cutechess-cli.md)
- using eval-extract to obtain fens & data from pgn files
  - [nnue-extract_fen-&-data-extraction-&-conversion-to-plain-text.md](docs/nnue-extract_fen-&-data-extraction-&-conversion-to-plain-text.md)
- converting plain text data files to nnue .bin training format
  - [plain-text-conversion-to-nnue-training-.bin-format.md](docs/plain-text-conversion-to-nnue-training-.bin-format.md)
- training & command-line management using nnue-gui
  - [create-a-nnue-with-nnue-gui.md](docs/create-a-nnue-with-nnue-gui.md)
- testing the generated nnues
  - [version-testing-with-little-blitzer.md](docs/version-testing-with-little-blitzer.md)
- improving the NNUE with supervised & reinforcement learning
  - [improving-the-nnue.md](docs/improving-the-nnue.md)

## available Windows binaries
- **x64 bmi2** = fast pgo binary (for modern 64-bit systems w/ BMI2 instruction set) if you own a Intel Haswell or newer cpu, this compile should be faster.
- **x64 avx2** = fast pgo binary (for modern 64-bit systems w/ AVX2 instruction set) if you own a modern AMD cpu, this compile should be the fastest.


run 'bench' at command line to determine which binary runs best/fastest on your system. for greater accuracy, run it twice and calculate the average of both results.


please see **http://chesslogik.wix.com/fire** for more info

## compile it yourself
- **windows** (visual studio) use included project files: Fire.vcxproj or Fire.sln
- **minGW** run one of the included bash shell scripts: make_bmi2.sh, make_axv2.sh, or make_all.sh 
- **ubuntu** type 'make profile-build ARCH=x86-64-bmi2', 'make profile-build ARCH=x86-64-avx2', etc.

## uci options
- **Hash** size of the hash table. default is 64 MB.
- **Threads** number of processor threads to use. default is 1, max = 128.
- **MultiPV** number of pv's/principal variations (lines of play) to be output. default is 1.
- **Contempt** higher contempt resists draws.
- **MoveOverhead** Adjust this to compensate for network and GUI latency. This is useful to avoid losses on time.
- **MinimumTime** Absolute minimum time (in ms) to spend on a single move. Also useful to avoid losses on time.
- **Ponder** also think during opponent's time. default is false.
- **UCI_Chess960** play chess960 (often called FRC or Fischer Random Chess). default is false.
- **Clear Hash** clear the hash table. delete allocated memory and re-initialize.
- **SyzygyProbeDepth** engine begins probing at specified depth. increasing this option makes the engine probe less.
- **EngineMode** choose NNUE (default), or random.
- **MCTS** enable Monte Carlo Tree Search w/UCT (Upper Confidence Bounds Applied to Trees)
- **SyzygyProbeLimit** number of pieces that have to be on the board in the endgame before the table-bases are probed.
- **Syzygy50MoveRule** set to false, tablebase positions that are drawn by the 50-move rule will count as a win or loss.
- **SyzygyPath** path to the syzygy tablebase files.
- **NnueEvalFile** path to the NNUE evaluation file.

## acknowledgements
many of the ideas & techiques incorporated into Fire are documented in detail here
- [Chess Programming Wiki](https://www.chessprogramming.org)

and some have been adapted from the super strong open-source chess engine
- [Stockfish](https://github.com/official-stockfish/Stockfish)

and others
- [Ippolit](https://github.com/FireFather/ippolit)
- [Robbolito](https://github.com/FireFather/robbolito)
- [Firebird](https://github.com/FireFather/firebird)
- [Ivanhoe](https://www.chessprogramming.org/IvanHoe)
- [Houdini](https://www.cruxis.com/chess/houdini.htm)
- [Gull](https://github.com/FireFather/seagull)

the endgame table bases are implemented using code adapted from Ronald de Man's
- [Syzygy EGTBs & probing code](https://github.com/syzygy1/tb)

The NNUE implementation utilizes a modified version of Daniel Shaw's/Cfish excellent nnue probe code:
- [nnue-probe](https://github.com/dshawul/nnue-probe/)

## license
Fire is distributed under the GNU General Public License. Please read LICENSE for more information.

please see **http://chesslogik.wix.com/fire** for more info

best regards-

firefather@telenet.be
