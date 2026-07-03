#include "ctx.hpp"
#include "builtins.hpp"
#include "preamble.hpp"
#include "driver.hpp"
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <emscripten.h>
#include <string>

emscripten::val msg(const char *kind) {
  emscripten::val o = emscripten::val::object();
  o.set("kind", std::string(kind));
  return o;
};

emscripten::val msg(const char *kind, const std::string &text) {
  emscripten::val o = msg(kind);
  o.set("text", text);
  return o;
};

class Session {
  Ctx ctx;

public:
  Session() {
    install_builtins(&ctx);
    run_all(preamble, &ctx);
  }

  emscripten::val run(const std::string &source, emscripten::val emit) {
    std::string_view rest = source;
    try {
      for (;;) {
        ReadEval r = read_eval(rest, &ctx);
        if (std::holds_alternative<Incomplete>(r)) return msg("incomplete");
        if (std::holds_alternative<Exhausted>(r)) return msg("ok");
        auto &e = std::get<Evaluated>(r);
        if (!e.output.empty()) emit(msg("out", e.output));
        if (!e.value.is_void()) emit(msg("res", e.value.to_write()));
        rest = e.rest;
      }
    }
    catch (const std::exception &e) {
      return msg("error", e.what());
    }
  }
};

EMSCRIPTEN_BINDINGS(scheme) {
  emscripten::class_<Session>("Session")
    .constructor<>()
    .function("run", &Session::run);
}
