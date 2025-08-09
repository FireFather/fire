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
- bench, perft & divide
- asychronous cout (acout) class using std::unique_lock <std::mutex>
- unique NNUE (halfkp_256x2-32-32) evaluation
- visual studio 2022 project files included

## nnue
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/docs/nuue_halfkp_data_flow.png)
![alt tag](https://raw.githubusercontent.com/FireFather/fire/master/docs/nnue_orientation_mapping.png)

## new
- lean & mean codebase size optimizations
- syzygy TBs have been removed (as NNUE evaluation becomes stronger TB's become less valuable)
- MCTS search has been removed
- the source code footprint has been reduced more than 50% from 491 KB to 232 KB
- and the source code has been reformatted via Google style guidelines
- updated binaries available in /src directory (no change to the NNUE file)

## uci options
- **Hash** -size of the hash table. default is 64 MB.
- **Threads** -number of processor threads to use. default is 1, max = 128.
- **MultiPV** -number of pv's/principal variations (lines of play) to be output. default is 1.
- **MoveOverhead** -adjust this to compensate for network and GUI latency. This is useful to avoid losses on time.
- **Ponder** -also think during opponent's time. default is false.
- **UCI_Chess960** -play chess960 (often called FRC or Fischer Random Chess). default is false.
- **Clear Hash** -clear the hash table. delete allocated memory and re-initialize.

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
