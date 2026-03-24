#pragma once
#include "types.hpp"
#include "env.hpp"
#include <unordered_set>

class Ctx {
  std::vector<HeapEntity *> live;
  std::unordered_set<std::string> interned;
  size_t gc_threshold;
  Env *global_env;

public:
  Ctx();
  ~Ctx();

  Symbol intern(std::string_view);

  template<typename T, typename... Args>
  T *alloc(Args&&... args) {
    T *obj = new T(std::forward<Args>(args)...);
    live.push_back(obj);
    return obj;
  }

  Env *get_global_env();

  bool should_recycle() const;
  void recycle();

  // pre-interned special form symbols
  Symbol sym_quote, sym_if, sym_define, sym_set, sym_lambda,
         sym_begin, sym_let, sym_letstar, sym_cond, sym_and,
         sym_or, sym_quasiquote, sym_unquote, sym_unquote_splicing,
         sym_else, sym_define_macro;
};
