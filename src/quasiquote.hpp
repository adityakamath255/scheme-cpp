#pragma once

#include "expression.hpp"

#include <variant>
#include <vector>

class QuasiquoteTemplate;

struct QuasiquoteSplice {
  const Expr *expression;
};

using QuasiquoteElement =
    std::variant<const QuasiquoteTemplate *, QuasiquoteSplice>;

class QuasiquoteTemplate final : public HeapEntity {
public:
  struct Pair {
    QuasiquoteElement car;
    const QuasiquoteTemplate *cdr;
  };

  struct VectorElements {
    std::vector<QuasiquoteElement> elements;
  };

  struct Value {
    const Expr *expression;
  };

private:
  const std::variant<Obj, Pair, VectorElements, Value> value;

public:
  explicit QuasiquoteTemplate(Obj value);
  explicit QuasiquoteTemplate(Pair value);
  explicit QuasiquoteTemplate(VectorElements value);
  explicit QuasiquoteTemplate(Value value);

  Obj instantiate(Env &, EvalContext &) const;
  void trace(std::vector<const HeapEntity *> &) const override;
};

class QuasiquoteExpr final : public Expr {
  const QuasiquoteTemplate *const value;

public:
  explicit QuasiquoteExpr(const QuasiquoteTemplate *value);

  EvalResult eval(Env &, EvalContext &) const override;
  void trace(std::vector<const HeapEntity *> &) const override;
};
