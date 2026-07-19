#pragma once
#include "types.hpp"
#include "lex.hpp"
#include <vector>

class EvalContext;

Obj parse(const std::vector<Token> &, EvalContext &);
