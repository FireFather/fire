#pragma once
#include "position.h"
class position;

namespace evaluate {
int eval(const position& pos);
int eval_after_null_move(int eval);
}  // namespace evaluate
