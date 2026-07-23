#pragma once

#include "scheme/session.hpp"
#include "types.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class Expr;

class Ctx {
  std::vector<std::unique_ptr<HeapEntity>> heap;
  std::unordered_set<std::string> interned;
  std::unordered_map<Symbol, Procedure *> macros;
  size_t gc_threshold;
  Env global_env;
  bool active;
  size_t depth;
  scheme::Emit emit;

  void own(std::unique_ptr<HeapEntity>);
  bool outermost() const;
  bool should_collect() const;
  void collect();
  void collect_if_needed();
  void result(std::string) const;

public:
  class DepthGuard {
    Ctx &context;

  public:
    explicit DepthGuard(Ctx &);
    ~DepthGuard();

    DepthGuard(const DepthGuard &) = delete;
    DepthGuard &operator=(const DepthGuard &) = delete;
  };

  Ctx();

  Symbol intern(std::string_view);
  void install_builtin(std::string_view, Builtin::Implementation);

  template<typename T, typename... Args>
  T *alloc(Args &&...args) {
    auto owned = std::make_unique<T>(std::forward<Args>(args)...);
    T *object = owned.get();
    own(std::move(owned));
    return object;
  }

  void output(std::string_view) const;

  scheme::RunResult run(std::string_view, const scheme::Emit &);
  void execute(std::string_view, const scheme::Emit &);
  scheme::RunResult run(std::string_view);
  void execute(std::string_view);

  Obj eval(const Expr *, Env &);
  Obj eval_top_level(Obj, Env &);
  Obj eval_global(Obj);

  Procedure *lookup_macro(Symbol) const;
  void define_macro(Symbol, Procedure *);
};

template<std::ranges::bidirectional_range R>
Obj list_from(R &&elements, Ctx &context, Obj tail = Null{}) {
  return std::ranges::fold_left(
      std::forward<R>(elements) | std::views::reverse, tail,
      [&context](Obj rest, Obj element) -> Obj {
        return context.alloc<Cons>(element, rest);
      });
}
