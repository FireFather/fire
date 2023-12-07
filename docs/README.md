# fire
<div align="left">

  [![Release][release-badge]][release-link]
  [![Commits][commits-badge]][commits-link]

  ![Downloads][downloads-badge]
  [![License][license-badge]][license-link]
 
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
- bench, perft & divide
- asychronous cout (acout) class using std::unique_lock<std::mutex>
- unique NNUE (halfkp_256x2-32-32) evaluation

## uci options
- **Hash** size of the hash table. default is 64 MB.
- **Threads** number of processor threads to use. default is 1, max = 128.
- **MultiPV** number of pv's/principal variations (lines of play) to be output. default is 1.
- **MoveOverhead** Adjust this to compensate for network and GUI latency. This is useful to avoid losses on time.
- **Ponder** also think during opponent's time. default is false.
- **UCI_Chess960** play chess960 (often called FRC or Fischer Random Chess). default is false.
- **Clear Hash** clear the hash table. delete allocated memory and re-initialize.

## binaries
- **x64 bmi2** = fast pgo binary (for modern 64-bit systems w/ BMI2 instruction set) if you own a modern Intel cpu (Haswell or newer), use this compile.
- **x64 avx2** = fast pgo binary (for modern 64-bit systems w/ AVX2 instruction set) if you own a modern AMD cpu, use this compile.

## compile it
- **windows** (visual studio) use included project files: Fire.vcxproj or Fire.sln
- **minGW** run one of the included shell scripts: make_bmi2.sh, make_avx2.sh, or make_all.sh 
- **ubuntu** type 'make profile-build ARCH=x86-64-bmi2', 'make profile-build ARCH=x86-64-avx2', etc.

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

## banners
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/fire_1.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/fire_2.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/fire_3.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/fire_4.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/fire_5.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/fire_6.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/fire_7.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/fire_8.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/fire_9.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/fire_10.bmp)
![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/fire_11.bmp)

## more info
**http://chesslogik.wix.com/fire**

[license-badge]:https://img.shields.io/github/license/FireFather/fire?style=for-the-badge&label=license&color=success
[license-link]:https://github.com/FireFather/fire/blob/main/LICENSE
[release-badge]:https://img.shields.io/github/v/release/FireFather/fire?style=for-the-badge&label=official%20release
[release-link]:https://github.com/FireFather/fire/releases/latest
[commits-badge]:https://img.shields.io/github/commits-since/FireFather/fire/latest?style=for-the-badge
[commits-link]:https://github.com/FireFather/fire/commits/main
[downloads-badge]:https://img.shields.io/github/downloads/FireFather/fire/total?color=success&style=for-the-badge
