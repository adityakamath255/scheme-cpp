#include "quasiquote.hpp"

#include "ctx.hpp"
#include "errors.hpp"

#include <utility>
#include <variant>

namespace {

void trace_quasiquote_element(
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

std::vector<Obj> instantiate_splice(
    const Expr *expression, Env &env, Ctx &ctx) {
  List values{ctx.eval(expression, env)};
  if (!values.proper()) {
    throw SchemeError("unquote-splicing: expected proper list");
  }
  return std::move(values.elements);
}

}

QuasiquoteTemplate::QuasiquoteTemplate(Obj value) : value{value} {}

QuasiquoteTemplate::QuasiquoteTemplate(Pair value) : value{value} {}

QuasiquoteTemplate::QuasiquoteTemplate(VectorElements value)
    : value{std::move(value)} {}

QuasiquoteTemplate::QuasiquoteTemplate(Value value) : value{value} {}

Obj QuasiquoteTemplate::instantiate(
    Env &env, Ctx &ctx) const {
  return std::visit(
      overloaded{
          [&](Obj datum) { return datum; },
          [&](Pair pair) -> Obj {
            return std::visit(
                overloaded{
                    [&](const QuasiquoteTemplate *car) -> Obj {
                      Obj car_value = car->instantiate(env, ctx);
                      Obj cdr_value = pair.cdr->instantiate(env, ctx);
                      return ctx.alloc<Cons>(car_value, cdr_value);
                    },
                    [&](QuasiquoteSplice splice) -> Obj {
                      std::vector<Obj> elements = instantiate_splice(
                          splice.expression, env, ctx);
                      Obj tail = pair.cdr->instantiate(env, ctx);
                      return list_from(elements, ctx, tail);
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
                            value->instantiate(env, ctx));
                      },
                      [&](QuasiquoteSplice splice) {
                        elements.append_range(instantiate_splice(
                            splice.expression, env, ctx));
                      },
                  },
                  element);
            }
            return ctx.alloc<Vector>(std::move(elements));
          },
          [&](Value hole) {
            return ctx.eval(hole.expression, env);
          },
      },
      value);
}

void QuasiquoteTemplate::trace(
    std::vector<const HeapEntity *> &worklist) const {
  std::visit(overloaded{
                 [&](Obj datum) { trace_child(datum, worklist); },
                 [&](Pair pair) {
                   trace_quasiquote_element(pair.car, worklist);
                   worklist.push_back(pair.cdr);
                 },
                 [&](const VectorElements &vector) {
                   for (const QuasiquoteElement &element : vector.elements) {
                     trace_quasiquote_element(element, worklist);
                   }
                 },
                 [&](Value hole) {
                   worklist.push_back(hole.expression);
                 },
             },
             value);
}

QuasiquoteExpr::QuasiquoteExpr(const QuasiquoteTemplate *value)
    : value{value} {}

EvalResult QuasiquoteExpr::eval(Env &env, Ctx &ctx) const {
  return value->instantiate(env, ctx);
}

void QuasiquoteExpr::trace(
    std::vector<const HeapEntity *> &worklist) const {
  worklist.push_back(value);
}
