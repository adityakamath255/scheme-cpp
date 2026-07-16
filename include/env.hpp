#pragma once
#include "types.hpp"
#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

class Env : public HeapEntity {
public:
  virtual std::optional<Obj> lookup(Symbol name) const = 0;
  virtual void define(Symbol name, Obj value) = 0;
  virtual bool set(Symbol name, Obj value) = 0;
};

class GlobalEnv : public Env {
  std::unordered_map<Symbol, Obj> bindings;

public:
  GlobalEnv();

  std::optional<Obj> lookup(Symbol name) const override;
  void define(Symbol name, Obj value) override;
  bool set(Symbol name, Obj value) override;

  void trace(std::vector<HeapEntity *> &) const override;
};

class LocalEnv : public Env {
  std::vector<std::pair<Symbol, Obj>> bindings;
  std::reference_wrapper<Env> parent;

public:
  LocalEnv(Env &);

  std::optional<Obj> lookup(Symbol name) const override;
  void define(Symbol name, Obj value) override;
  bool set(Symbol name, Obj value) override;

  void trace(std::vector<HeapEntity *> &) const override;
};
