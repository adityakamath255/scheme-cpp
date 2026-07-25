#include "errors.hpp"

#include "ctx.hpp"

static std::string render_condition(Obj payload) {
  if (Error *error = payload.try_as_error()) {
    return error->describe();
  }
  return "uncaught exception: " + payload.to_write();
}

SchemeError::SchemeError(const std::string &message)
    : scheme::EvaluationError(message), payload{} {}

SchemeError SchemeError::raised(Obj payload) {
  SchemeError e(render_condition(payload));
  e.payload = payload;
  return e;
}

Obj SchemeError::as_condition(Ctx &ctx) {
  return payload ? *payload : Obj(ctx.alloc<Error>(what(), Null{}));
}
