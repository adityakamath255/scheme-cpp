#include "eval.hpp"
#include "expr.hpp"

#include <ranges>
#include <utility>
#include <variant>
#include <vector>

namespace {

class QuasiquoteTemplate;

struct QuasiquoteSplice {
  const Expr *expression;
};

using QuasiquoteElement =
    std::variant<const QuasiquoteTemplate *, QuasiquoteSplice>;

static void trace_quasiquote_element(
    const QuasiquoteElement &,
    std::vector<const HeapEntity *> &);

static std::vector<Obj> instantiate_splice(
    const Expr *expression, Env &env, EvalContext &context) {
  Obj values = context.eval(expression, env);
  auto profile = values.list_profile();
  if (!profile.is_proper()) {
    throw SchemeError("unquote-splicing: expected proper list");
  }
  return std::ranges::to<std::vector>(ListView{values});
}

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
  explicit QuasiquoteTemplate(Obj value) : value{value} {}
  explicit QuasiquoteTemplate(Pair value) : value{value} {}
  explicit QuasiquoteTemplate(VectorElements value)
      : value{std::move(value)} {}
  explicit QuasiquoteTemplate(Value value) : value{value} {}

  Obj instantiate(Env &env, EvalContext &context) const {
    return std::visit(
        overloaded{
            [&](Obj datum) { return datum; },
            [&](Pair pair) -> Obj {
              return std::visit(
                  overloaded{
                      [&](const QuasiquoteTemplate *car) -> Obj {
                        Obj car_value = car->instantiate(env, context);
                        Obj cdr_value =
                            pair.cdr->instantiate(env, context);
                        return context.alloc<Cons>(car_value, cdr_value);
                      },
                      [&](QuasiquoteSplice splice) -> Obj {
                        std::vector<Obj> elements = instantiate_splice(
                            splice.expression, env, context);
                        Obj tail = pair.cdr->instantiate(env, context);
                        return list_from(elements, context, tail);
                      },
                  },
                  pair.car);
            },
            [&](const VectorElements &vector) -> Obj {
              std::vector<Obj> elements;
              for (const QuasiquoteElement &element : vector.elements) {
                std::visit(
                    overloaded{
                        [&](const QuasiquoteTemplate *value) {
                          elements.push_back(
                              value->instantiate(env, context));
                        },
                        [&](QuasiquoteSplice splice) {
                          elements.append_range(instantiate_splice(
                              splice.expression, env, context));
                        },
                    },
                    element);
              }
              return context.alloc<Vector>(std::move(elements));
            },
            [&](Value hole) {
              return context.eval(hole.expression, env);
            },
        },
        value);
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    std::visit(overloaded{
                   [&](Obj datum) { trace_child(datum, worklist); },
                   [&](Pair pair) {
                     trace_quasiquote_element(pair.car, worklist);
                     worklist.push_back(pair.cdr);
                   },
                   [&](const VectorElements &vector) {
                     for (const QuasiquoteElement &element :
                          vector.elements) {
                       trace_quasiquote_element(element, worklist);
                     }
                   },
                   [&](Value hole) {
                     worklist.push_back(hole.expression);
                   },
               },
               value);
  }
};

static void trace_quasiquote_element(
    const QuasiquoteElement &element,
    std::vector<const HeapEntity *> &worklist) {
  std::visit(overloaded{
                 [&](const QuasiquoteTemplate *value) {
                   worklist.push_back(value);
                 },
                 [&](QuasiquoteSplice splice) {
                   worklist.push_back(splice.expression);
                 },
             },
             element);
}

class QuasiquoteExpr final : public Expr {
  const QuasiquoteTemplate *const value;

public:
  explicit QuasiquoteExpr(const QuasiquoteTemplate *value) : value{value} {}

  EvalResult eval(Env &env, EvalContext &context) const override {
    return value->instantiate(env, context);
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    worklist.push_back(value);
  }
};

}
