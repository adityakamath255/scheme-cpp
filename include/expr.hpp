#pragma once

#include "types.hpp"

#include <functional>
#include <variant>

class Expr;

struct TailCall {
  const Expr *expression;
  std::reference_wrapper<Env> environment;
};

using EvalResult = std::variant<Obj, TailCall>;

class Expr : public HeapEntity {
public:
  virtual EvalResult eval(Env &, EvalContext &) const = 0;
};

