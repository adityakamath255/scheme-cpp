#pragma once
#include "types.hpp"
#include <memory>

class Env : public HeapEntity {
  struct Bindings;
  std::unique_ptr<Bindings> bindings;
  Env *parent;

public:
  Env(Env *);
  ~Env();

  std::optional<Obj> lookup(Symbol name) const;
  void define(Symbol name, Obj value);
  bool set(Symbol name, Obj value);

  void trace(std::vector<HeapEntity *> *) const override;
};
