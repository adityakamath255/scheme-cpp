#include "ctx.hpp"
#include "builtins.hpp"
#include "preamble.hpp"
#include "lex.hpp"
#include "parse.hpp"
#include "eval.hpp"
#include <emscripten/bind.h>
#include <string>
#include <string_view>

struct Outcome {
  bool ok;
  std::string value; // repr of the result when ok, else the error message
};

class Session {
  Ctx ctx;

public:
  Session() {
    install_builtins(ctx.global_env, &ctx);
    evaluate(std::string(preamble));
  }

  Outcome evaluate(const std::string &source) {
    std::string_view src = source;
    Obj last = Obj(Void{});
    try {
      while (true) {
        auto result = lex(src);
        if (!result || result->tokens.empty()) {
          break;
        }
        Obj expr = parse(result->tokens, &ctx);
        last = eval(expr, ctx.global_env, &ctx);
        src = result->rest;
        if (ctx.should_recycle()) {
          ctx.recycle();
        }
      }
    }
    catch (const std::exception &e) {
      return {false, e.what()};
    }
    return {true, last.is_void() ? "" : last.stringify(true)};
  }

  bool complete(const std::string &source) {
    std::string_view src = source;
    try {
      while (true) {
        auto result = lex(src);
        if (!result) {
          return false;
        }
        if (result->tokens.empty()) {
          return true;
        }
        src = result->rest;
      }
    }
    catch (const std::exception &) {
      return true;
    }
  }
};

EMSCRIPTEN_BINDINGS(scheme) {
  emscripten::value_object<Outcome>("Outcome")
    .field("ok", &Outcome::ok)
    .field("value", &Outcome::value);

  emscripten::class_<Session>("Session")
    .constructor<>()
    .function("eval", &Session::evaluate)
    .function("complete", &Session::complete);
}
