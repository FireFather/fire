# Fire Zero

![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/docs/fire.png)

a strong, state-of-the-art, highly optimized, open-source freeware UCI chess engine...
designed and programmed for modern 64-bit windows and linux systems
by Norman Schmidt


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
- timestamped bench, perft/divide logs
- asychronous cout (acout) class using std::unique_lock<std::mutex>
- uci option searchtype random w/ uniform_real_distribution & mesenne_twister_engine
- can read engine.conf on startup for search, eval, pawn, and material parameters
- fast alpha-beta search
- or optional MCTS-UCT search
 (Monte Carlo Tree Search w/ Upper Confidence Bounds Applied to Trees) pure/no minmax
- original NNUE evaluation based 100% on Fire 8.2 self-play games

**Fire Zero is now available**

![alt tag](https://raw.githubusercontent.com/FireFather/fire-NN/master/docs/Fire_8.NN.MCx64.png)

Fire has undergone meticulous analysis and refactoring using many of the most modern C++ tools available today, including Clang, ReSharper C++, and Visual Studio Code Analysis, ensuring the production of extremely fast highly optimized and stable executables.


## available binaries
- **x64 avx2** = fast pgo binary (for modern 64-bit systems w/ AVX2 instruction set)
- **x64 bmi2** = fast pgo binary (for modern 64-bit systems w/ BMI2 instruction set)

- **windows** : Fire_Zero_x64_bmi2.exe, Fire_Zero_x64_avx2.exe
- **linux** :   Fire_Zero_x64_bmi2, Fire_Zero_x64_avx2


Run 'bench' at command line to determine which binary runs best/fastest on your system. for greater accuracy, run it twice and calculate the average of both results.


## compile it yourself
- **windows** (visual studio) use included project files: Fire.vcxproj or Fire.sln
- **minGW** run one of the included bash shell scripts: makefire_bmi2.sh, makefire_axv2.sh or makefire_popc.sh
- **ubuntu** type 'make profile-build ARCH=x86-64-bmi2' or 'make profile-build ARCH=x86-64-avx2' or 'make profile-build ARCH=x86-64-popc'

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

please see **http://chesslogik.wix.com/fire** for more info

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
- [Ivanhoe](http://users.telenet.be/chesslogik/ivanhoe.htm)
- [Houdini](https://www.cruxis.com/chess/houdini.htm)
- [Gull](https://github.com/FireFather/seagull)

the endgame table bases are implemented using code adapted from Ronald de Man's
- [Syzygy EGTBs & probing code](https://github.com/syzygy1/tb)

the MCTS implementation is derived from Stephane Nicolet's work
- https://github.com/snicolet/Stockfish/commits/montecarlo

fire 8.2 self-play games were generated using cutechess-cli
- https://github.com/cutechess/cutechess

the NNUE implementation utilizes a modified version of Daniel Shaw's/Cfish excellent nnue probe code:
- [nnue-probe](https://github.com/dshawul/nnue-probe/)

the NNUE data was extracted and prepared using Deed's Tools
- https://outskirts.altervista.org/forum/viewtopic.php?f=41&t=2009

it was converted & trained using Nodchip's original NNUE halfkp_256x2-32-32 learn modules (2020-08-30)
- https://github.com/nodchip/Stockfish

training was launched and monitored using nnue-gui 1.5
- https://github.com/FireFather/nnue-gui

if you are interested in learning about my particular testing methodology, I've explained it in some detail here:
http://www.chessdom.com/fire-the-chess-engine-releases-a-new-version/


## license
Fire is distributed under the GNU General Public License. Please read LICENSE for more information.

## Does Fire play anything like Stockfish? 
No. Here are the results of Don Daily's SIM program (default values) today to measure Fire move selection vs the last 4 versions of Stockfish:

![alt tag](https://raw.githubusercontent.com/FireFather/fire-NN/master/docs/matrix.png)

As you can see Stockfish 14.1 and Stockfish 15 make the same moves ~64% of the time (see row 4, column 5)

Fire 8.NN has an excellent score of ~43.34 % (see row 1), making different moves than SF almost 60% of the time.

The Sim tool can be downloaded here:
https://komodochess.com/downloads.htm
(bottom of page)


best regards-
Norm

firefather@telenet.be
