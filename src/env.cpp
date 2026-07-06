#include "env.hpp"
#include <optional>

GlobalEnv::GlobalEnv(): bindings {} {}

GlobalEnv::~GlobalEnv() = default;

std::optional<Obj> GlobalEnv::lookup(Symbol sym) const {
  auto it = bindings.find(sym);
  if (it != bindings.end()) {
    return it->second;
  }
  else {
    return std::nullopt;
  }
}

void GlobalEnv::define(Symbol sym, Obj obj) {
  bindings.insert_or_assign(sym, obj);
}

bool GlobalEnv::set(Symbol sym, Obj obj) {
  auto it = bindings.find(sym);
  if (it != bindings.end()) {
    it->second = obj;
    return true;
  }
  else {
    return false;
  }
}

void GlobalEnv::trace(std::vector<HeapEntity *> *worklist) const {
  for (const auto &[_, obj] : bindings) {
    trace_child(obj, worklist);
  }
}

LocalEnv::LocalEnv(Env *parent): bindings {}, parent {parent} {}

LocalEnv::~LocalEnv() = default;

std::optional<Obj> LocalEnv::lookup(Symbol sym) const {
  for (const auto &[k, v] : bindings) {
    if (k == sym) {
      return v;
    }
  }
  return parent->lookup(sym);
}

void LocalEnv::define(Symbol sym, Obj obj) {
  for (auto &[k, v] : bindings) {
    if (k == sym) {
      v = obj;
      return;
    }
  }
  bindings.emplace_back(sym, obj);
}

bool LocalEnv::set(Symbol sym, Obj obj) {
  for (auto &[k, v] : bindings) {
    if (k == sym) {
      v = obj;
      return true;
    }
  }
  return parent->set(sym, obj);
}

void LocalEnv::trace(std::vector<HeapEntity *> *worklist) const {
  for (const auto &[_, obj] : bindings) {
    trace_child(obj, worklist);
  }
  worklist->push_back(parent);
}
