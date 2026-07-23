#include "eval.hpp"

#include <format>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>

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
                   Ctx &context) const {
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
