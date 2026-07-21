#pragma once
#include "session.hpp"
#include "types.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class EvalContext;
class Expr;

class Arity {
  size_t minimum;
  std::optional<size_t> maximum;

  Arity(size_t minimum, std::optional<size_t> maximum);

public:
  static Arity exactly(size_t count);
  static Arity between(size_t minimum, size_t maximum);
  static Arity at_least(size_t minimum);

  std::optional<std::string> mismatch(size_t count) const;
};

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

  void own(std::unique_ptr<HeapEntity>);
  void result(std::string) const;
  void collect_if_needed();

public:
  EvalContext(scheme::SessionState &, const scheme::Emit &);

  Symbol intern(std::string_view);
  void install_builtin(std::string_view, Builtin::Implementation);

  template<typename T, typename... Args>
  T *alloc(Args&&... args) {
    auto owned = std::make_unique<T>(std::forward<Args>(args)...);
    T *object = owned.get();
    own(std::move(owned));
    return object;
  }

  void output(std::string_view) const;

  scheme::RunResult run(std::string_view source, ResultMode);
  void execute(std::string_view source, ResultMode);
  Obj eval(const Expr *expression, Env &environment);
  Obj eval_top_level(Obj expression, Env &environment);
  Obj eval_global(Obj expression);

  Procedure *lookup_macro(Symbol) const;
  void define_macro(Symbol, Procedure *);
};

template<std::ranges::bidirectional_range R>
Obj list_from(R &&elements, EvalContext &context, Obj tail = Null{}) {
  return std::ranges::fold_left(
      std::forward<R>(elements) | std::views::reverse, tail,
      [&context](Obj rest, Obj element) -> Obj {
        return context.alloc<Cons>(element, rest);
      });
}
