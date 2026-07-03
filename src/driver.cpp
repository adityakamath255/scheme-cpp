#include "driver.hpp"
#include "ctx.hpp"
#include "lex.hpp"
#include "parse.hpp"
#include "eval.hpp"

ReadEval read_eval(std::string_view source, Ctx *ctx) {
  auto result = lex(source);
  if (!result) {
    return Incomplete{};
  }
  if (result->tokens.empty()) {
    return Exhausted{};
  }
  Obj expr = parse(result->tokens, ctx);
  Obj value = eval(expr, ctx->global_env, ctx);
  if (ctx->should_recycle()) {
    ctx->recycle();
  }
  return Evaluated{value, ctx->take_output(), result->rest};
}

void run_all(std::string_view source, Ctx *ctx) {
  for (;;) {
    ReadEval r = read_eval(source, ctx);
    auto *e = std::get_if<Evaluated>(&r);
    if (!e) {
      break;
    }
    source = e->rest;
  }
}
