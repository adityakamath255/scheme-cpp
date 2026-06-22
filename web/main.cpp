#include "ctx.hpp"
#include "builtins.hpp"
#include "preamble.hpp"
#include "driver.hpp"
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <emscripten.h>
#include <string>

class Session {
  Ctx ctx;

public:
  Session() {
    install_builtins(&ctx);
    run_all(preamble, &ctx);
  }

  emscripten::val step(const std::string &source) {
    using emscripten::val;
    auto tagged = [](const char *kind) {
      val o = val::object();
      o.set("kind", std::string(kind));
      return o;
    };
    try {
      ReadEval r = read_eval(source, &ctx);
      if (auto *e = std::get_if<Evaluated>(&r)) {
        val o = tagged("value");
        o.set("rest", std::string(e->rest));
        if (!e->value.is_void()) {
          o.set("value", e->value.stringify(true));
        }
        return o;
      }
      return tagged(std::holds_alternative<Incomplete>(r) ? "incomplete"
                                                          : "exhausted");
    }
    catch (const std::exception &e) {
      EM_ASM({ throw new Error(UTF8ToString($0)); }, e.what());
      return val::undefined();
    }
  }
};

EMSCRIPTEN_BINDINGS(scheme) {
  emscripten::class_<Session>("Session")
    .constructor<>()
    .function("step", &Session::step);
}
