#include "env.hpp"
#include <unordered_map>

// global envs (parent == nullptr) use hashmaps for bindings,
// local envs (parent != nullptr) use flat vectors instead

struct Env::Bindings {
  using VecBindings = std::vector<std::pair<Symbol, Obj>>;
  using MapBindings = std::unordered_map<Symbol, Obj>;
  using Value = std::variant<VecBindings, MapBindings>;

  Value data;

  Bindings(bool map) {
    if (map) {
      data = MapBindings{};
    }
    else {
      data = VecBindings{};
    }
  }

  bool is_vec() const {
    return std::holds_alternative<VecBindings>(data);
  }

  bool is_map() const {
    return std::holds_alternative<MapBindings>(data);
  }

  VecBindings &as_vec() { return std::get<VecBindings>(data); }
  const VecBindings &as_vec() const { return std::get<VecBindings>(data); }

  MapBindings &as_map() { return std::get<MapBindings>(data); }
  const MapBindings &as_map() const { return std::get<MapBindings>(data); }
};

Env::Env(Env *parent):
  bindings {std::make_unique<Bindings>(parent == nullptr)},
  parent {parent}
{}

Env::~Env() = default;

std::optional<Obj> Env::lookup(Symbol sym) const {
  if (bindings->is_map()) {
    auto &map = bindings->as_map();
    auto it = map.find(sym);
    if (it != map.end()) {
      return it->second;
    }
  } 
  else {
    for (const auto &[k, v] : bindings->as_vec()) {
      if (k == sym) return v;
    }
  }
  return parent ? parent->lookup(sym) : std::nullopt;
}

void Env::define(Symbol sym, Obj obj) {
  if (bindings->is_map()) {
    bindings->as_map().insert_or_assign(sym, obj);
  } 
  else {
    bindings->as_vec().emplace_back(sym, obj);
  }
}

bool Env::set(Symbol sym, Obj obj) {
  if (bindings->is_map()) {
    auto &map = bindings->as_map();
    auto it = map.find(sym);
    if (it != map.end()) {
      it->second = obj;
      return true;
    }
  } 
  else {
    for (auto &[k, v] : bindings->as_vec()) {
      if (k == sym) {
        v = obj;
        return true;
      }
    }
  }
  return parent ? parent->set(sym, obj) : false;
}

void Env::trace(std::vector<HeapEntity *> *worklist) const {
  auto trace_obj = [&](const Obj &obj) {
    if (auto entity = obj.heap_entity()) {
      worklist->push_back(*entity);
    }
  };

  if (bindings->is_map()) {
    for (const auto &[_, obj] : bindings->as_map()) {
      trace_obj(obj);
    }
  } 
  else {
    for (const auto &[_, obj] : bindings->as_vec()) {
      trace_obj(obj);
    }
  }

  if (parent) {
    worklist->push_back(parent);
  }
}
