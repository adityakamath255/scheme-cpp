#include "ctx.hpp"

Ctx::Ctx():
  live {},
  interned {},
  gc_threshold {1024},
  global_env {alloc<GlobalEnv>()},
  sym_quote {intern("quote")},
  sym_if {intern("if")},
  sym_define {intern("define")},
  sym_set {intern("set!")},
  sym_lambda {intern("lambda")},
  sym_begin {intern("begin")},
  sym_let {intern("let")},
  sym_letstar {intern("let*")},
  sym_cond {intern("cond")},
  sym_and {intern("and")},
  sym_or {intern("or")},
  sym_quasiquote {intern("quasiquote")},
  sym_unquote {intern("unquote")},
  sym_unquote_splicing {intern("unquote-splicing")},
  sym_else {intern("else")},
  sym_define_macro {intern("define-macro")}
{}

Ctx::~Ctx() {
  for (auto *entity : live) {
    delete entity;
  }
}

Symbol Ctx::intern(std::string_view name) {
  auto [it, _] = interned.insert(std::string(name));
  return Symbol{&*it};
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
