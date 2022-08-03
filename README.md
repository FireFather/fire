# fire-zero
fire 8.2 self-play nnue

## tools used:

-f ire 8.2 https://github.com/FireFather/fire
- cutechess https://github.com/cutechess/cutechess
- Deeds Tools https://outskirts.altervista.org/forum/viewtopic.php?t=2009
- nnue-gui https://github.com/FireFather/nnue-gui
- sf-nnue https://github.com/FireFather/sf-nnue
- pgn-extract https://www.cs.kent.ac.uk/people/staff/djb/pgn-extract/

## opening book creation:
- ChessBase 16 64Bit https://shop.chessbase.com/en/products/chessbase_16_fritz18_bundle 
(or another suitable program to removes duplicate games from a PGN file)
- 40H-PGN Tools http://40hchess.epizy.com/

## efficiently-updatable neural network (NNUE) for any engine:
- Setting up a suitable Windows environment
- Creating an efficient opening book using cutechess-cli
- Running selfplay games for a 'Zero' NNUE approach
- Using eval-nnue eval-extract (Deeds Tools https://outskirts.altervista.org/forum/viewtopic.php?f=41&t=2009&start=30)
- position (fen) extraction from PGNs
- conversion to plain text
- conversion ofplain text files to NNUE .bin training format
- training management using nnue-gui (https://github.com/FireFather/nnue-gui)
- creating an initial NNUE
- testing the generated nnues
- improving the NNUE with Supervised and Reinforcement Learning
