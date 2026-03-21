#pragma once
#include "types.hpp"
#include <unordered_map>

class Env : public HeapEntity {
private:
  std::unordered_map<Symbol, Obj> bindings;
  Env *parent;

public:
  Env(Env *);
  
  std::optional<Obj> lookup(Symbol name) const;
  void define(Symbol name, Obj value);
  bool set(Symbol name, Obj value);

  void trace(std::vector<HeapEntity *> *) const override;
};

