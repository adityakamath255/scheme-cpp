#pragma once

#include "scheme/session.hpp"
#include "types.hpp"

#include <optional>
#include <string>

// an UnattributedError is named by the innermost builtin boundary.
// the boundary prefixes its name and rethrows SchemeError. SchemeError passes
// through outer boundaries unchanged, preserving evaluator and fixed-origin
// errors. Argument decoding relabels accessor errors for the enclosing builtin.
struct SchemeError : scheme::EvaluationError {
  std::optional<Obj> payload;

  explicit SchemeError(const std::string &message);
  static SchemeError raised(Obj payload);

  Obj as_condition(Ctx &context);
};

struct UnattributedError : SchemeError {
  using SchemeError::SchemeError;
};
