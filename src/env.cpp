#include "env.hpp"

Env::Env(Env *parent): bindings {}, parent {parent} {}

std::optional<Obj> Env::lookup(Symbol sym) const {
  auto res = bindings.find(sym);
  if (res != bindings.end()) {
    return res->second;
  }
  else if (parent) {
    return parent->lookup(sym);
  }
  else {
    return std::nullopt;
  }
}

void Env::define(Symbol sym, Obj obj) {
  bindings.insert_or_assign(sym, obj);
}

bool Env::set(Symbol sym, Obj obj) {
  auto res = bindings.find(sym);
  if (res != bindings.end()) {
    res->second = obj;
    return true;
  }
  if (parent) {
    return parent->set(sym, obj);
  }
  else {
    return false;
  }
}

void Env::trace(std::vector<HeapEntity *> *worklist) const {
  for (const auto &[_, obj] : bindings) {
    if (auto entity = obj.heap_entity()) {
      worklist->push_back(*entity);
    }
  }
  if (parent) {
    worklist->push_back(parent);
  }
}
