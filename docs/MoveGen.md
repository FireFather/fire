# Move Generation Developer Guide

This document describes how Fire’s move generation works and how to work with or modify it.

---

## 1. Overview

Fire’s move generator is **bitboard-based** and highly modular. It uses C++ templates to generate different classes of moves (captures, quiets, evasions, etc.) with minimal branching. The generator is designed for **speed** and **correctness**, supporting standard chess and Chess960.

Key goals:
- Generate legal moves quickly
- Avoid generating illegal moves (especially when in check)
- Provide specialized generators for different search stages

---

## 2. Core Structure

The move generation is defined in:

- `movegen.h`: templates, function declarations, type definitions
- `movegen.cpp`: function definitions, supporting helpers

**Key types and constants:**
- `Move`: 16-bit encoded move (from, to, flags, promotion)
- `MoveList`: simple array-based container with `size()` and `begin()/end()`
- Bitboard constants for files, ranks, piece attacks

---

## 3. Templates and Modes

Move generation is implemented with template parameters controlling **what** to generate.

Common modes:
- **CAPTURES**: Only generate captures (and possibly promotions)
- **QUIETS**: Only generate non-capturing moves
- **EVASIONS**: Moves that get the king out of check (including blocks, captures, king moves)
- **QUIET_CHECKS**: Quiet moves that give check
- **ALL**: All legal moves

Example:
```cpp
template<GenType Type>
MoveList generate(const Position& pos);
```

The compiler inlines the correct generator code based on the `GenType` template parameter.

---

## 4. King Safety and Legality

To avoid returning illegal moves, the generator:
- Tracks pinned pieces (bitboard) for the current position
- Forbids pinned pieces from moving off their pin line (unless they capture the attacker on that line)
- When in check:
  - **Double check** → only king moves are legal
  - **Single check** → only king moves, captures of the checker, or blocks are legal

King moves are validated against the **enemy attack map** to avoid moving into check.

---

## 5. Special Rules

- **Castling**:
  - Handled in king-move generation
  - Checks that the intermediate squares are empty and not attacked
  - Chess960 support: rook and king starting squares can vary

- **En Passant**:
  - Checks legality by simulating capture and ensuring own king is not in check after the pawn is removed
  - Prevents false positives where EP capture would expose the king to a discovered check

- **Promotions**:
  - Generated as separate moves for each promotion piece type
  - Both capturing and quiet promotions are supported

---

## 6. Bitboard Attack Tables

Fire precomputes attack tables for:
- Pawn attacks (by color)
- Knight moves
- King moves
- Sliding piece rays (rook/bishop/queen) with occupancy masking

These tables are indexed by square and, for sliders, by occupancy bitboard.

Macros/functions:
- `pawn_attacks(color, square)`
- `knight_attacks(square)`
- `king_attacks(square)`
- `rook_attacks(square, occupancy)`
- `bishop_attacks(square, occupancy)`

---

## 7. Perft and Divide

`perft` recursively generates all moves to a given depth and counts the leaf nodes.  
`divide` outputs per-move node counts to help isolate generator bugs.

Usage:
```text
perft 5
divide 3
```

If perft counts differ from known-correct values, debug with `divide` at decreasing depths to locate the bug.

---

## 8. Performance Notes

- **Target-masked generation**: many functions accept a target bitboard to limit generation (useful in captures-only mode).
- **Inlining**: template-based approach allows the compiler to inline all relevant code for the chosen mode.
- **Branch reduction**: separate generators for captures, quiets, etc. avoid runtime branching.
- **Bit scan**: use of `lsb()` and `pop_lsb()` to iterate set bits efficiently.

---

## 9. Debugging Tips

- Check perft results in both standard chess and Chess960
- Test positions with multiple pinned pieces, double checks, EP possibilities
- Ensure king safety logic matches `in_check()` and `attackers_to()` outputs
- Compare generated move lists against a reference engine in various modes

---

## 10. Integration

To generate moves in search:
```cpp
MoveList ml = generate<ALL>(pos);
for (Move m : ml) {
    // process move
}
```

For quiescence search (captures only):
```cpp
MoveList ml = generate<CAPTURES>(pos);
```

---
