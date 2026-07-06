#pragma once
#include "types.hpp"

void install_builtins(Ctx *ctx);

Obj builtin_apply(const std::vector<Obj> &args, Ctx *ctx);
