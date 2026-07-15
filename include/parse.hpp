#pragma once
#include "types.hpp"
#include "lex.hpp"

class Runtime;

Obj parse(const std::vector<Token> &, Runtime *);
