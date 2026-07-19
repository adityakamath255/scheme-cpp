#include "eval.hpp"

#include "read.hpp"

scheme::RunResult EvalContext::run(
  std::string_view source,
  ResultMode result_mode
) {
  std::string_view remaining = source;

  while (true) {
    collect_if_needed();
    ReadOutcome read = read_one(remaining, *this);

    if (std::holds_alternative<ReadIncomplete>(read)) {
      return {
        .consumed = source.size() - remaining.size(),
        .incomplete = true
      };
    }

    if (auto *end = std::get_if<ReadEnd>(&read)) {
      return {
        .consumed = source.size() - end->rest.size(),
        .incomplete = false
      };
    }

    auto &datum = std::get<ReadDatum>(read);
    Obj value = eval(datum.value, state.global_env);

    if (result_mode == ResultMode::Emit && !value.is_void()) {
      result(value.to_write());
    }

    remaining = datum.rest;
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
