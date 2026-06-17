#pragma once
#include "types.hpp"
#include "env.hpp"
#include <unordered_set>

class Ctx {
  std::vector<HeapEntity *> live;
  std::unordered_set<std::string> interned;
  size_t gc_threshold;

public:
  Ctx();
  ~Ctx();

  Env *const global_env;

  Symbol intern(std::string_view);

  template<typename T, typename... Args>
  T *alloc(Args&&... args) {
    T *obj = new T(std::forward<Args>(args)...);
    live.push_back(obj);
    return obj;
  }

  bool should_recycle() const;
  void recycle();

  Symbol sym_quote, sym_if, sym_define, sym_set, sym_lambda,
         sym_begin, sym_let, sym_letstar, sym_cond, sym_and,
         sym_or, sym_quasiquote, sym_unquote, sym_unquote_splicing,
         sym_else, sym_define_macro;
};
