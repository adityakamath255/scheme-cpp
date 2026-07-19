#include "eval.hpp"

#include "lex.hpp"
#include "parse.hpp"

scheme::RunResult EvalContext::run(
  std::string_view source,
  ResultMode result_mode
) {
  std::string_view remaining = source;

  while (true) {
    auto read = lex(remaining);

    if (!read) {
      return {
        .consumed = source.size() - remaining.size(),
        .incomplete = true
      };
    }

    if (read->tokens.empty()) {
      return {
        .consumed = source.size() - read->rest.size(),
        .incomplete = false
      };
    }

    collect_if_needed();

    Obj expression = parse(read->tokens, *this);
    Obj value = eval(expression, state.global_env);

    if (result_mode == ResultMode::Emit && !value.is_void()) {
      result(value.to_write());
    }

    remaining = read->rest;
  }
}

void EvalContext::execute(
  std::string_view source,
  ResultMode result_mode
) {
  if (run(source, result_mode).incomplete) {
    throw SchemeError("unexpected end of input");
  }
}
