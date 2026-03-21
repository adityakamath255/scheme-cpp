#include "ctx.hpp"

Ctx::Ctx():
  live {},
  interned {},
  gc_threshold {1024}
{
  global_env = alloc<Env>(nullptr);
}

Ctx::~Ctx() {
  for (auto *entity : live) {
    delete entity;
  }
}

Symbol Ctx::intern(const std::string &name) {
  auto [it, _] = interned.insert(name);
  return Symbol{&*it};
}

Env *Ctx::get_global_env() {
  return global_env;
}

bool Ctx::should_recycle() const {
  return live.size() > gc_threshold;
}

void Ctx::recycle() {
  std::vector<HeapEntity *> worklist;
  std::unordered_set<HeapEntity *> marked;
  worklist.push_back(global_env);

  while (!worklist.empty()) {
    HeapEntity *entity = worklist.back();
    worklist.pop_back();

    auto inserted = marked.insert(entity).second;

    if (inserted) {
      entity->trace(&worklist);
    }
  }

  std::vector<HeapEntity *> surviving;

  for (auto *entity : live) {
    if (marked.contains(entity)) {
      surviving.push_back(entity);
    }
    else {
      delete entity;
    }
  }

  live = std::move(surviving);
  gc_threshold = live.size() * 2;
}
