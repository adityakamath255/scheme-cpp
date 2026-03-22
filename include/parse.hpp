#pragma once
#include "types.hpp"
#include "lex.hpp"

Obj parse(const std::vector<Token> &, Ctx *);
