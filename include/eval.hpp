#pragma once
#include "scheme.hpp"
#include "types.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

class EvalContext;

namespace scheme {

class SessionState {
  std::vector<HeapEntity *> live;
  std::unordered_set<std::string> interned;
  size_t gc_threshold;
  Env global_env;
  bool active;

  bool should_collect() const;
  void collect();
  void initialize();

  SessionState();

  friend class ::EvalContext;

public:
  static std::unique_ptr<SessionState> create();

  ~SessionState();

  SessionState(const SessionState &) = delete;
  SessionState &operator=(const SessionState &) = delete;

  RunResult run(std::string_view source, const Emit &emit);
  void execute(std::string_view source, const Emit &emit);
};

}

enum class ResultMode {
  Emit,
  Suppress
};

class EvalContext {
  scheme::SessionState &state;
  const scheme::Emit &emit_event;
  size_t depth;

  class Frame {
    EvalContext &context;

  public:
    explicit Frame(EvalContext &);
    ~Frame();
  };

  void result(std::string) const;
  void collect_if_needed();

public:
  EvalContext(scheme::SessionState &, const scheme::Emit &);

  Symbol intern(std::string_view);
  void install_builtin(std::string_view, Builtin::Implementation);

  template<typename T, typename... Args>
  T *alloc(Args&&... args) {
    T *obj = new T(std::forward<Args>(args)...);
    state.live.push_back(obj);
    return obj;
  }

  void output(std::string_view) const;

  scheme::RunResult run(std::string_view source, ResultMode);
  void execute(std::string_view source, ResultMode);
  Obj eval(Obj expression, Env &environment);
  Obj eval_global(Obj expression);
};

template<std::ranges::bidirectional_range R>
Obj list_from(R &&elements, EvalContext &context, Obj tail = Null{}) {
  return std::ranges::fold_left(
      std::forward<R>(elements) | std::views::reverse, tail,
      [&context](Obj rest, Obj element) -> Obj {
        return context.alloc<Cons>(element, rest);
      });
}

void check_arity(size_t count, std::string_view name, size_t min, size_t max);
