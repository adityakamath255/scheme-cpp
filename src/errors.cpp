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

Obj SchemeError::as_condition(Ctx &context) {
  return payload ? *payload : Obj(context.alloc<Error>(what(), Null{}));
}
