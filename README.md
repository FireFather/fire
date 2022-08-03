![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/nnue-gui.png)

# fire-zero
fire 8.2 self-play nnue

## tools used:

- engine for generation of self-play games
  - fire 8.2 https://github.com/FireFather/fire
- application to run self-play games
  - cutechess-cli https://github.com/cutechess/cutechess- 
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
- utility for opening book creation...remove duplicate games from pgn files
  - chessbase 16 64-bit https://shop.chessbase.com/en/products/chessbase_16_fritz18_bundle
- misc utilities: truncate.exe opening book ply limitation, & gameSplit.exe to break up pgns to multiple parts
  - 40H-pgn tools http://40hchess.epizy.com/

## instructions to create an efficiently-updatable neural network (nnue) for any engine:
- setting up a suitable Windows environment
  - [environment.md](docs/environment.md)
- creating an efficient opening book using cutechess-cli
  - [environment.md](docs/environment.md)
- running selfplay games for a 'zero' nnue approach
  - [environment.md](docs/environment.md)
- using eval-nnue and/or eval-extract
  - [environment.md](docs/environment.md)
- position (fen) extraction from pgn files
  - [environment.md](docs/environment.md)
- conversion to plain text
  - [environment.md](docs/environment.md)
- conversion of plain text files to nnue .bin training format
  - [environment.md](docs/environment.md)
- training & command-line management using nnue-gui
  - [environment.md](docs/environment.md)
- creating an initial nnue
  - [environment.md](docs/environment.md)
- testing the generated nnues
  - [environment.md](docs/environment.md)
- improving the NNUE with supervised & reinforcement learning
  - [environment.md](docs/environment.md)

