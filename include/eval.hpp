#pragma once
#include "types.hpp"

#include <utility>

std::pair<Obj, std::vector<Obj>> splice_apply(const std::vector<Obj> &args);

Obj eval(Obj expr, Env *env, Ctx *ctx);
