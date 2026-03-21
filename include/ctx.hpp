#pragma once
#include "types.hpp"
#include "env.hpp"
#include <unordered_set>

class Ctx {
  std::vector<HeapEntity *> live;
  std::unordered_set<HeapEntity *> marked;
  std::unordered_set<std::string> interned;
  size_t gc_threshold;
  Env *global_env;

public:
  Ctx();
  ~Ctx();

  Symbol intern(const std::string &);

  template<typename T, typename... Args>
  T *alloc(Args&&... args) {
    T *obj = new T(std::forward<Args>(args)...);
    live.push_back(obj);
    return obj;
  }

  Env *get_global_env();

  bool should_recycle() const;
  void recycle();
};
