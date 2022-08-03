# fire-zero
fire 8.2 self-play nnue

## tools used:

- fire 8.2 https://github.com/FireFather/fire
	engine for generation of self-play games
- cutechess https://github.com/cutechess/cutechess
	command-line application to run self-play games
- deeds tools https://outskirts.altervista.org/forum/viewtopic.php?t=2009
	nnue-extract.exe to extract fen positions and evaluation data from pgn files
- nnue-gui https://github.com/FireFather/nnue-gui
	windows application for nnue training management
- sf-nnue https://github.com/FireFather/sf-nnue
	optimized port of a nodchip shogi neural network implementation
- pgn-extract https://www.cs.kent.ac.uk/people/staff/djb/pgn-extract/
	used by nnue-extract to extract fen positions and info from pgns
- little blitzer http://www.kimiensoftware.com/software/chess/littleblitzer
	for testing nnue vs nnue

## opening book creation:
- chessbase 16 64-bit https://shop.chessbase.com/en/products/chessbase_16_fritz18_bundle 
(or another suitable program to removes duplicate games from a pgn file)
- 40H-pgn tools http://40hchess.epizy.com/

## instructions to create an efficiently-updatable neural network (nnue) for any engine:
- setting up a suitable Windows environment
- creating an efficient opening book using cutechess-cli
- running selfplay games for a 'zero' nnue approach
- using eval-nnue eval-extract (Deeds Tools https://outskirts.altervista.org/forum/viewtopic.php?f=41&t=2009&start=30)
- position (fen) extraction from pgn files
- conversion to plain text
- conversion ofplain text files to nnue .bin training format
- training management using nnue-gui (https://github.com/FireFather/nnue-gui)
- creating an initial nnue
- testing the generated nnues
- improving the NNUE with supervised and reinforcement learning
