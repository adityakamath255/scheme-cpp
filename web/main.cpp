#include "scheme/session.hpp"
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <emscripten.h>
#include <exception>
#include <string>
#include <variant>

namespace {

constexpr char output_kind[] = "out";
constexpr char result_kind[] = "res";
constexpr char ok_kind[] = "ok";
constexpr char incomplete_kind[] = "incomplete";
constexpr char error_kind[] = "error";

}

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
          emit(msg(output_kind, output->text));
        }
        else {
          emit(msg(result_kind, std::get<scheme::Result>(event).text));
        }
      });
      return msg(result.incomplete ? incomplete_kind : ok_kind);
    }
    catch (const std::exception &e) {
      return msg(error_kind, e.what());
    }
  }
};

EMSCRIPTEN_BINDINGS(scheme) {
  emscripten::class_<WasmSession>("Session")
    .constructor<>()
    .function("run", &WasmSession::run);
}
