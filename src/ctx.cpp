#include "ctx.hpp"

#include "builtins.hpp"
#include "errors.hpp"
#include "expression.hpp"
#include "parser.hpp"
#include "preamble.hpp"
#include "reader.hpp"

#include <cassert>
#include <stdexcept>
#include <utility>

// each guarded level spans several C stack frames; 1000 is calibrated for
// default 8MB stacks (see web/CMakeLists.txt)
static constexpr size_t max_depth = 1000;

namespace {

scheme::Emit no_op_emit() {
  return [](const scheme::Event &) {};
}

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

class EmitScope {
  scheme::Emit &slot;
  scheme::Emit previous;

public:
  EmitScope(scheme::Emit &slot, const scheme::Emit &replacement)
      : slot{slot},
        previous{replacement ? replacement : no_op_emit()} {
    slot.swap(previous);
  }

  ~EmitScope() { slot.swap(previous); }
};

}

Ctx::Ctx()
    : heap{}, interned{}, macros{}, gc_threshold{1024}, global_env{},
      active{false}, depth{0}, emit{no_op_emit()} {
  install_builtins(*this);
  // preamble results are discarded because the emitter defaults to a no-op
  execute(preamble);
}

Ctx::DepthGuard::DepthGuard(Ctx &context) : context{context} {
  if (context.depth >= max_depth) {
    throw SchemeError("recursion too deep");
  }
  context.depth += 1;
}

Ctx::DepthGuard::~DepthGuard() { context.depth -= 1; }

void Ctx::own(std::unique_ptr<HeapEntity> object) {
  heap.push_back(std::move(object));
}

Symbol Ctx::intern(std::string_view name) {
  auto [symbol, _] = interned.insert(std::string(name));
  return Symbol{*symbol};
}

void Ctx::install_builtin(
    std::string_view name, Builtin::Implementation implementation) {
  global_env.define(
      intern(name), alloc<Builtin>(std::move(implementation)));
}

bool Ctx::outermost() const {
  return depth == 0;
}

bool Ctx::should_collect() const {
  return heap.size() > gc_threshold;
}

void Ctx::collect() {
  assert(depth == 0);

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

void Ctx::collect_if_needed() {
  if (outermost() && should_collect()) {
    collect();
  }
}

void Ctx::output(std::string_view text) const {
  emit(scheme::Output{std::string(text)});
}

void Ctx::result(std::string text) const {
  emit(scheme::Result{std::move(text)});
}

Obj Ctx::eval(const Expr *expression, Env &environment) {
  DepthGuard guard{*this};
  EvalResult value = expression->eval(environment, *this);
  while (auto *tail_call = std::get_if<TailCall>(&value)) {
    value = tail_call->expression->eval(
        tail_call->environment.get(), *this);
  }
  return std::get<Obj>(value);
}

Obj Ctx::eval_top_level(Obj expression, Env &environment) {
  Parser parser{*this};
  return parser.top_level(expression, environment);
}

Obj Ctx::eval_global(Obj expression) {
  return eval_top_level(expression, global_env);
}

Procedure *Ctx::lookup_macro(Symbol name) const {
  auto macro = macros.find(name);
  return macro == macros.end() ? nullptr : macro->second;
}

void Ctx::define_macro(Symbol name, Procedure *macro) {
  macros.insert_or_assign(name, macro);
}

scheme::RunResult Ctx::run(std::string_view source) {
  std::string_view remaining = source;

  while (true) {
    // collection only runs between top-level forms
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

    // result echo relies on builtins running inside an eval frame at depth >= 1;
    // load stays silent, so do not invoke builtins outside the trampoline
    if (outermost() && !value.is_void()) {
      result(value.to_write());
    }

    remaining = datum.rest;
  }
}

void Ctx::execute(std::string_view source) {
  if (run(source).incomplete) {
    throw SchemeError("unexpected end of input");
  }
}

scheme::RunResult Ctx::run(
    std::string_view source, const scheme::Emit &emitter) {
  ActiveSession active_session{active};
  EmitScope emit_scope{emit, emitter};
  return run(source);
}

void Ctx::execute(
    std::string_view source, const scheme::Emit &emitter) {
  ActiveSession active_session{active};
  EmitScope emit_scope{emit, emitter};
  execute(source);
}
