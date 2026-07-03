#pragma once
#include "types.hpp"
#include "env.hpp"
#include <string>
#include <unordered_set>

class Ctx {
  std::vector<HeapEntity *> live;
  std::unordered_set<std::string> interned;
  size_t gc_threshold;
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

  Symbol sym_quote, sym_if, sym_define, sym_set, sym_lambda,
         sym_begin, sym_let, sym_letstar, sym_letrec, sym_cond,
         sym_and, sym_or, sym_quasiquote, sym_unquote,
         sym_unquote_splicing, sym_else, sym_define_macro,
         sym_when, sym_unless, sym_case;
};
