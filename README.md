![alt tag](https://raw.githubusercontent.com/FireFather/fire-zero/master/bitmaps/nnue-gui.png)

# fire-zero
fire 8.2 self-play nnue

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


PGNs used are available for download here:

https://drive.google.com/drive/folders/1047piVwcYW7yDV4TbX0qpSgcd9zph2Nr?usp=sharing
