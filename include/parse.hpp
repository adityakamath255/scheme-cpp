#pragma once
#include "types.hpp"
#include "lex.hpp"
#include <vector>

class Runtime;

Obj parse(const std::vector<Token> &, Runtime *);
