#include "scheme.hpp"
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <emscripten.h>
#include <exception>
#include <string>
#include <variant>

emscripten::val msg(const char *kind) {
  emscripten::val o = emscripten::val::object();
  o.set("kind", std::string(kind));
  return o;
}

emscripten::val msg(const char *kind, const std::string &text) {
  emscripten::val o = msg(kind);
  o.set("text", text);
  return o;
}

class WasmSession {
  scheme::Session session;

public:
  emscripten::val run(const std::string &source, emscripten::val emit) {
    try {
      auto result = session.run(source, [&](const scheme::Event &event) {
        if (auto *output = std::get_if<scheme::Output>(&event)) {
          emit(msg("out", output->text));
        }
        else {
          emit(msg("res", std::get<scheme::Result>(event).text));
        }
      });
      return msg(result.incomplete ? "incomplete" : "ok");
    }
    catch (const std::exception &e) {
      return msg("error", e.what());
    }
  }
};

EMSCRIPTEN_BINDINGS(scheme) {
  emscripten::class_<WasmSession>("Session")
    .constructor<>()
    .function("run", &WasmSession::run);
}
