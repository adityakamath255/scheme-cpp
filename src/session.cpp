#include "session.hpp"

#include "builtins.hpp"
#include "eval.hpp"
#include "preamble.hpp"
#include "read.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace scheme {

class SessionState {
  std::vector<std::unique_ptr<HeapEntity>> heap;
  std::unordered_set<std::string> interned;
  std::unordered_map<Symbol, Procedure *> macros;
  size_t gc_threshold;
  Env global_env;
  bool active;

  bool should_collect() const;
  void collect();

  friend class ::EvalContext;

public:
  SessionState();

  RunResult run(std::string_view source, const Emit &emit);
  void execute(std::string_view source, const Emit &emit);
};

}

namespace {

class ActiveSession {
  bool &active;

public:
  explicit ActiveSession(bool &active) : active{active} {
    if (active) {
      throw std::logic_error("session is already running");
    }
    active = true;
  }

  ~ActiveSession() { active = false; }
};

}

scheme::SessionState::SessionState()
    : heap{}, interned{}, macros{}, gc_threshold{1024},
      global_env{}, active{false} {
  const Emit ignore;
  EvalContext context{*this, ignore};
  install_builtins(context);
  context.execute(preamble, ResultMode::Suppress);
}

bool scheme::SessionState::should_collect() const {
  return heap.size() > gc_threshold;
}

void scheme::SessionState::collect() {
  std::vector<const HeapEntity *> worklist;
  std::unordered_set<const HeapEntity *> marked;
  worklist.push_back(&global_env);
  for (const auto &[_, macro] : macros) {
    worklist.push_back(macro);
  }

  while (!worklist.empty()) {
    const HeapEntity *entity = worklist.back();
    worklist.pop_back();

    if (marked.insert(entity).second) {
      entity->trace(worklist);
    }
  }

  std::erase_if(heap, [&](const auto &entity) {
    return !marked.contains(entity.get());
  });
  gc_threshold = heap.size() * 2;
}

void EvalContext::own(std::unique_ptr<HeapEntity> object) {
  state.heap.push_back(std::move(object));
}

Symbol EvalContext::intern(std::string_view name) {
  auto [symbol, _] = state.interned.insert(std::string(name));
  return Symbol{*symbol};
}

void EvalContext::install_builtin(
    std::string_view name, Builtin::Implementation implementation) {
  state.global_env.define(
      intern(name), alloc<Builtin>(std::move(implementation)));
}

void EvalContext::collect_if_needed() {
  if (depth == 0 && state.should_collect()) {
    state.collect();
  }
}

Obj EvalContext::eval_global(Obj expression) {
  return eval_top_level(expression, state.global_env);
}

Procedure *EvalContext::lookup_macro(Symbol name) const {
  auto macro = state.macros.find(name);
  return macro == state.macros.end() ? nullptr : macro->second;
}

void EvalContext::define_macro(Symbol name, Procedure *macro) {
  state.macros.insert_or_assign(name, macro);
}

scheme::RunResult EvalContext::run(std::string_view source,
                                   ResultMode result_mode) {
  std::string_view remaining = source;

  while (true) {
    collect_if_needed();
    ReadOutcome read = read_one(remaining, *this);

    if (std::holds_alternative<ReadIncomplete>(read)) {
      return {
          .consumed = source.size() - remaining.size(),
          .incomplete = true,
      };
    }

    if (auto *end = std::get_if<ReadEnd>(&read)) {
      return {
          .consumed = source.size() - end->rest.size(),
          .incomplete = false,
      };
    }

    auto &datum = std::get<ReadDatum>(read);
    Obj value = eval_global(datum.value);

    if (result_mode == ResultMode::Emit && !value.is_void()) {
      result(value.to_write());
    }

    remaining = datum.rest;
  }
}

void EvalContext::execute(std::string_view source, ResultMode result_mode) {
  if (run(source, result_mode).incomplete) {
    throw SchemeError("unexpected end of input");
  }
}

scheme::RunResult scheme::SessionState::run(
    std::string_view source, const Emit &emit) {
  ActiveSession guard{active};
  EvalContext context{*this, emit};
  return context.run(source, ResultMode::Emit);
}

void scheme::SessionState::execute(std::string_view source,
                                   const Emit &emit) {
  ActiveSession guard{active};
  EvalContext context{*this, emit};
  context.execute(source, ResultMode::Emit);
}

scheme::Session::Session() : state{std::make_unique<SessionState>()} {}

scheme::Session::~Session() = default;

scheme::RunResult scheme::Session::run(
    std::string_view source,
    const Emit &emit
) {
  return state->run(source, emit);
}

void scheme::Session::execute(
    std::string_view source,
    const Emit &emit
) {
  state->execute(source, emit);
}
