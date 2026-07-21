#include "eval.hpp"
#include "expression.hpp"

#include <format>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>

static constexpr size_t max_eval_depth = 1000;

EvalContext::EvalContext(scheme::SessionState &state,
                         const scheme::Emit &emit)
    : state{state}, emit_event{emit}, depth{0} {}

void EvalContext::output(std::string_view text) const {
  if (emit_event) {
    emit_event(scheme::Output{std::string(text)});
  }
}

void EvalContext::result(std::string text) const {
  if (emit_event) {
    emit_event(scheme::Result{std::move(text)});
  }
}

EvalContext::Frame::Frame(EvalContext &context) : context{context} {
  if (context.depth >= max_eval_depth) {
    throw SchemeError("recursion too deep");
  }
  context.depth += 1;
}

EvalContext::Frame::~Frame() { context.depth -= 1; }

Arity::Arity(size_t minimum, std::optional<size_t> maximum)
    : minimum{minimum}, maximum{maximum} {
  if (maximum && *maximum < minimum) {
    throw std::invalid_argument("arity maximum is less than minimum");
  }
}

Arity Arity::exactly(size_t count) { return Arity{count, count}; }

Arity Arity::between(size_t minimum, size_t maximum) {
  return Arity{minimum, maximum};
}

Arity Arity::at_least(size_t minimum) {
  return Arity{minimum, std::nullopt};
}

std::optional<std::string> Arity::mismatch(size_t count) const {
  if (count >= minimum && (!maximum || count <= *maximum)) {
    return std::nullopt;
  }

  std::string expected;
  if (!maximum) {
    expected = std::format("{} or more", minimum);
  } else if (*maximum == minimum) {
    expected = std::to_string(minimum);
  } else {
    expected = std::format("{}-{}", minimum, *maximum);
  }
  return std::format("expected {} arguments, got {}", expected, count);
}

Formals Formals::parse(Obj formals) {
  if (formals.is_symbol()) {
    return {{}, formals.as_symbol()};
  }

  ListView params{formals};
  std::vector<Symbol> fixed;
  for (Obj param : params) {
    if (!param.is_symbol()) {
      throw SchemeError("parameter must be a symbol");
    }
    fixed.push_back(param.as_symbol());
  }

  Obj tail = params.tail();
  if (tail.is_null()) {
    return {std::move(fixed), std::nullopt};
  }
  if (tail.is_symbol()) {
    return {std::move(fixed), tail.as_symbol()};
  }
  throw SchemeError("invalid parameter list");
}

void Formals::bind(Env &env, const std::vector<Obj> &args,
                   EvalContext &context) const {
  auto arity = rest ? Arity::at_least(fixed.size())
                    : Arity::exactly(fixed.size());
  if (auto error = arity.mismatch(args.size())) {
    throw SchemeError(*error);
  }

  for (size_t i = 0; i < fixed.size(); i += 1) {
    env.define(fixed[i], args[i]);
  }
  if (rest) {
    env.define(*rest,
               list_from(args | std::views::drop(fixed.size()), context));
  }
}

Obj EvalContext::eval(const Expr *expression, Env &environment) {
  Frame frame{*this};
  EvalResult result = expression->eval(environment, *this);
  while (auto *tail_call = std::get_if<TailCall>(&result)) {
    result = tail_call->expression->eval(
        tail_call->environment.get(), *this);
  }
  return std::get<Obj>(result);
}
