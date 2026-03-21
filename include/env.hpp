#pragma once

#include "types.hpp"

#include <unordered_map>

class Env : public HeapEntity {
private:
  std::unordered_map<Symbol, Obj> bindings;
  std::optional<Env *> parent;

public:

  void push_children(MarkStack *) const override;
}

