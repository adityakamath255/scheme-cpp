#include "runtime.hpp"

Runtime::Runtime():
  live {},
  interned {},
  gc_threshold {1024},
  global_env {alloc<GlobalEnv>()}
{}

Runtime::~Runtime() {
  for (auto *entity : live) {
    delete entity;
  }
}

Symbol Runtime::intern(std::string_view name) {
  auto [it, _] = interned.insert(std::string(name));
  return Symbol{&*it};
}

bool Runtime::should_recycle() const {
  return live.size() > gc_threshold;
}

void Runtime::recycle() {
  std::vector<HeapEntity *> worklist;
  std::unordered_set<HeapEntity *> marked;
  worklist.push_back(global_env);

  while (!worklist.empty()) {
    HeapEntity *entity = worklist.back();
    worklist.pop_back();

    if (marked.insert(entity).second) {
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
