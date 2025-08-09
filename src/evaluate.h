/*
 * Chess Engine - Evaluation Module (Interface)
 * ---------------------------------------------
 * Declares evaluation functions used by the search module.
 *
 * Public API:
 * - eval(const position&): Return static evaluation of current position.
 * - eval_after_null_move(int): Return adjusted eval after a null move.
 *
 * The implementation (evaluate.cpp) uses NNUE for speed and strength.
 */

#pragma once
#include "position.h"
class position;

namespace evaluate{
  int eval(const position& pos);
  int eval_after_null_move(int eval);
}
