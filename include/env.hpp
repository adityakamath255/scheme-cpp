#pragma once
#include "types.hpp"
#include <unordered_map>

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
  ~GlobalEnv();

  std::optional<Obj> lookup(Symbol name) const override;
  void define(Symbol name, Obj value) override;
  bool set(Symbol name, Obj value) override;

  void trace(std::vector<HeapEntity *> *) const override;
};

class LocalEnv : public Env {
  std::vector<std::pair<Symbol, Obj>> bindings;
  Env *parent;

public:
  LocalEnv(Env *);
  ~LocalEnv();

  std::optional<Obj> lookup(Symbol name) const override;
  void define(Symbol name, Obj value) override;
  bool set(Symbol name, Obj value) override;

  void trace(std::vector<HeapEntity *> *) const override;
};
