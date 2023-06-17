# fire

![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/nnue-gui.png)

**fire self-play nnue**

- fire without NNUE can be found here: https://github.com/FireFather/fire-HCE (archived, no longer actively maintained)
- fire w/ NNUE from shared SF & LC0 data: https://github.com/FireFather/fire-NN (archived, no longer actively maintained)

## features
- c++20
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
- timestamped bench, perft, & divide log files
- asychronous cout (acout) class using std::unique_lock<std::mutex>
- uci option EngineMode random w/ uniform_real_distribution & mesenne_twister_engine
- uses a unique NNUE (halfkp_256x2-32-32) evaluation
- fast alpha-beta search
- optional experimental MCTS-UCT search
 (Monte Carlo Tree Search w/ Upper Confidence Bounds Applied to Trees) pure/no minmax

**fire-9 is now available**

| strength estimate                     |    |       |                    |      |
| ------------------------------------- |--- | ----- | ------------------ | ---- |
|                                       |    | games |(+win, =draw, -loss)| (%)  |
|    fire-9_x64_bmi2  			|3460| 16384 | (+5968,=4937,-5479)|51.5 %|
|    vs.                                |     |      |                    |      |
|    stockfish-15                 	|3861|   863 | (+715,=135,-13)    |90.7 %|
|    komodo-dragon-3.1           	|3838|   863 | (+702,=141,-20)    |89.5 %|
|    berserk-9                   	|3754|   863 | (+631,=190,-42)    |84.1 %|
|    koivisto_8.0                 	|3637|   863 | (+501,=261,-101)   |73.2 %|
|    rubiChess-20220813          	|3564|   863 | (+386,=338,-139)   |64.3 %|
|    seer-v2.5                    	|3504|   862 | (+322,=324,-216)   |56.1 %|
|    rofChade-3.0                 	|3492|   862 | (+302,=336,-224)   |54.5 %|
|    slow64-2.9                   	|3452|   863 | (+254,=336,-273)   |48.9 %|
|    rebel-15.1                   	|3450|   862 | (+245,=348,-269)   |48.6 %|
|    wasp-6.00                   	|3395|   862 | (+207,=291,-364)   |40.9 %|
|    fire-8242022                	|3384|   862 | (+195,=290,-377)   |39.4 %|
|    nemorino-6.00                	|3374|   862 | (+156,=344,-362)   |38.1 %|
|    igel-3.1.0                   	|3334|   862 | (+122,=323,-417)   |32.9 %|
|    ethereal-12.75              	|3331|   862 | (+158,=244,-460)   |32.5 %|
|    clover-3.1                   	|3296|   862 | (+147,=194,-521)   |28.3 %|
|    minic-3.24                   	|3290|   862 | (+106,=264,-492)   |27.6 %|
|    xiphos-0.6                   	|3266|   862 | (+124,=184,-554)   |25.1 %|
|    tucano-10.00                 	|3266|   862 | (+99,=234,-529)    |25.1 %|
|    marvin-6.0.0                 	|3233|   862 | (+107,=160,-595)   |21.7 %|

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
  - [windows-environment.md](windows-environment.md)
- creating an efficient opening book using cutechess-cli
  - [cutechess-opening-books.md](cutechess-opening-books.md)
- running selfplay games for a 'zero' nnue approach
  - [engine-selfplay-via-cutechess-cli.md](engine-selfplay-via-cutechess-cli.md)
- using eval-extract to obtain fens & data from pgn files
  - [nnue-extract_fen-&-data-extraction-&-conversion-to-plain-text.md](nnue-extract_fen-&-data-extraction-&-conversion-to-plain-text.md)
- converting plain text data files to nnue .bin training format
  - [plain-text-conversion-to-nnue-training-.bin-format.md](plain-text-conversion-to-nnue-training-.bin-format.md)
- training & command-line management using nnue-gui
  - [create-a-nnue-with-nnue-gui.md](create-a-nnue-with-nnue-gui.md)
- testing the generated nnues
  - [version-testing-with-little-blitzer.md](version-testing-with-little-blitzer.md)
- improving the NNUE with supervised & reinforcement learning
  - [improving-the-nnue.md](improving-the-nnue.md)

## monte-carlo search mode:  
![alt tag](https://raw.githubusercontent.com/FireFather/fire-NN/master/docs/Fire_8.NN.MCx64.png)

fire-9 has undergone months of meticulous analysis and refactoring using many of the most modern C++ tools available today, including Clang, ReSharper C++, and Visual Studio Code Analysis, ensuring the production of extremely fast highly optimized and stable executables.

## available Windows binaries
- **x64 bmi2** = fast pgo binary (for modern 64-bit systems w/ BMI2 instruction set) if you own a Intel Haswell or newer cpu, this compile should be faster.
- **x64 avx2** = fast pgo binary (for modern 64-bit systems w/ AVX2 instruction set) if you own a modern AMD cpu, this compile should be the fastest.
- **x64 popc** = fast pgo binary (for older 64-bit systems w/ SSE41 & POPCNT instruction sets) 

- **windows** : fire-9_x64_x64_bmi2.exe, fire-9_x64_avx2.exe, fire-9_x64_popc.exe

run 'bench' at command line to determine which binary runs best/fastest on your system. for greater accuracy, run it twice and calculate the average of both results.

please see **http://chesslogik.wix.com/fire** for more info

## compile it yourself
- **windows** (visual studio) use included project files: Fire.vcxproj or Fire.sln
- **minGW** run one of the included shell scripts: make_bmi2.sh, make_avx2.sh, make_popc.sh, or make_all.sh 
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


![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/logos/fire_1.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/logos/fire_2.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/logos/fire_3.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/logos/fire_4.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/logos/fire_5.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/logos/fire_6.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/logos/fire_7.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/logos/fire_8.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/logos/fire_9.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/logos/fire_10.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/logos/fire_11.bmp)

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

The NNUE implementation utilizes a modified version of Daniel Shawul's/Cfish excellent nnue probe code:
- [nnue-probe](https://github.com/dshawul/nnue-probe/)

the MCTS implementation is derived from Stephane Nicolet's work
- https://github.com/snicolet/Stockfish/commits/montecarlo

if you are interested in learning about my particular ultra-fast testing methodology, I've explained it in some detail here:
http://www.chessdom.com/fire-the-chess-engine-releases-a-new-version/

## license
Fire is distributed under the GNU General Public License. Please read LICENSE for more information.

please see **http://chesslogik.wix.com/fire** for more info

best regards-

firefather@telenet.be
