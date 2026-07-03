#include "ctx.hpp"
#include "builtins.hpp"
#include "preamble.hpp"
#include "driver.hpp"
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <emscripten.h>
#include <string>

emscripten::val status(const char *kind) {
  emscripten::val o = emscripten::val::object();
  o.set("kind", std::string(kind));
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
        if (std::holds_alternative<Incomplete>(r)) return status("incomplete");
        if (std::holds_alternative<Exhausted>(r)) return status("ok");
        auto &e = std::get<Evaluated>(r);
        if (!e.value.is_void()) emit(e.value.stringify(true));
        rest = e.rest;
      }
    }
    catch (const std::exception &e) {
      auto o = status("error");
      o.set("message", std::string(e.what()));
      return o;
    }
  }
};

EMSCRIPTEN_BINDINGS(scheme) {
  emscripten::class_<Session>("Session")
    .constructor<>()
    .function("run", &Session::run);
}
