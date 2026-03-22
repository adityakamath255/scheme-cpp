#pragma once
#include "types.hpp"

void bind_args(
  Env *env,
  const std::vector<Symbol> &params,
  const std::vector<Obj> &args,
  bool variadic,
  Ctx *ctx
);

Obj eval(Obj expr, Env *env, Ctx *ctx);
