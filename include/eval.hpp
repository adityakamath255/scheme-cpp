#pragma once
#include "runtime.hpp"
#include "scheme.hpp"
#include "types.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

enum class ResultMode {
  Emit,
  Suppress
};

class Evaluator {
  Runtime &state;
  const scheme::Emit &emit_event;
  size_t depth;

public:
  Evaluator(Runtime &, const scheme::Emit &);

  Runtime &runtime();
  Env &global_env() const;
  Symbol intern(std::string_view);

  template<typename T, typename... Args>
  T *alloc(Args&&... args) {
    return state.alloc<T>(std::forward<Args>(args)...);
  }

  void output(std::string_view) const;
  void result(std::string) const;

  scheme::RunResult run(std::string_view source, ResultMode);
  void execute(std::string_view source, ResultMode);
  Obj eval(Obj expression, Env &environment);

  void collect_if_needed();
  bool push();
  void pop();
};

void check_arity(size_t count, std::string_view name, size_t min, size_t max);
