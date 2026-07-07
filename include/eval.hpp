#pragma once
#include "types.hpp"

#include <utility>

std::pair<Obj, std::vector<Obj>> splice_apply(const std::vector<Obj> &args);

void check_arity(size_t count, std::string_view name, size_t min, size_t max);

Obj eval(Obj expr, Env *env, Ctx *ctx);
