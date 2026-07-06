#pragma once
#include "types.hpp"
#include "env.hpp"
#include <algorithm>
#include <ranges>
#include <string>
#include <unordered_set>

class Ctx {
  std::vector<HeapEntity *> live;
  std::unordered_set<std::string> interned;
  size_t gc_threshold;
  size_t eval_depth;
  std::string output;

public:
  Ctx();
  ~Ctx();

  Env *const global_env;

  void print(std::string_view);
  std::string take_output();

  Symbol intern(std::string_view);

  template<typename T, typename... Args>
  T *alloc(Args&&... args) {
    T *obj = new T(std::forward<Args>(args)...);
    live.push_back(obj);
    return obj;
  }

  bool should_recycle() const;
  void recycle();

  bool push_eval();
  void pop_eval();
};

// fold_left over the reversed range == fold_right, which libc++ (wasm) lacks
template<std::ranges::bidirectional_range R>
Obj list_from(R &&elems, Ctx *ctx, Obj tail = Null{}) {
  return std::ranges::fold_left(
    std::forward<R>(elems) | std::views::reverse, tail,
    [ctx](Obj acc, Obj elem) -> Obj { return ctx->alloc<Cons>(elem, acc); }
  );
}
