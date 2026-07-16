#pragma once

#include "env.hpp"
#include "types.hpp"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

class Runtime {
  std::vector<HeapEntity *> live;
  std::unordered_set<std::string> interned;
  size_t gc_threshold;

  bool should_recycle() const;
  void recycle();

  friend class Evaluator;

public:
  Runtime();
  ~Runtime();

  Runtime(const Runtime &) = delete;
  Runtime &operator=(const Runtime &) = delete;

  Env *const global_env;

  Symbol intern(std::string_view);

  template<typename T, typename... Args>
  T *alloc(Args&&... args) {
    T *obj = new T(std::forward<Args>(args)...);
    live.push_back(obj);
    return obj;
  }
};

template<std::ranges::bidirectional_range R, typename Allocator>
Obj list_from(R &&elems, Allocator *allocator, Obj tail = Null{}) {
  return std::ranges::fold_left(
    std::forward<R>(elems) | std::views::reverse, tail,
    [allocator](Obj acc, Obj elem) -> Obj {
      return allocator->template alloc<Cons>(elem, acc);
    }
  );
}
