#include "scheme.hpp"

#include "builtins.hpp"
#include "eval.hpp"
#include "preamble.hpp"

#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

class ActiveSession {
  bool &active;

public:
  explicit ActiveSession(bool &active) : active{active} {
    if (active) {
      throw std::logic_error("session is already running");
    }
    active = true;
  }

  ~ActiveSession() { active = false; }
};

}

scheme::SessionState::SessionState()
    : live{}, interned{}, gc_threshold{1024}, global_env{}, active{false} {}

std::unique_ptr<scheme::SessionState>
scheme::SessionState::create() {
  auto state = std::unique_ptr<SessionState>(new SessionState);
  state->initialize();
  return state;
}

void scheme::SessionState::initialize() {
  const Emit ignore;
  EvalContext context{*this, ignore};
  install_builtins(context);
  context.execute(preamble, ResultMode::Suppress);
}

scheme::SessionState::~SessionState() {
  for (auto *entity : live) {
    delete entity;
  }
}

bool scheme::SessionState::should_collect() const {
  return live.size() > gc_threshold;
}

void scheme::SessionState::collect() {
  std::vector<HeapEntity *> worklist;
  std::unordered_set<HeapEntity *> marked;
  worklist.push_back(&global_env);

  while (!worklist.empty()) {
    HeapEntity *entity = worklist.back();
    worklist.pop_back();

    if (marked.insert(entity).second) {
      entity->trace(worklist);
    }
  }

  std::vector<HeapEntity *> surviving;
  for (auto *entity : live) {
    if (marked.contains(entity)) {
      surviving.push_back(entity);
    } else {
      delete entity;
    }
  }

  live = std::move(surviving);
  gc_threshold = live.size() * 2;
}

scheme::RunResult scheme::SessionState::run(
    std::string_view source, const Emit &emit) {
  ActiveSession guard{active};
  EvalContext context{*this, emit};
  return context.run(source, ResultMode::Emit);
}

void scheme::SessionState::execute(std::string_view source,
                                   const Emit &emit) {
  ActiveSession guard{active};
  EvalContext context{*this, emit};
  context.execute(source, ResultMode::Emit);
}

scheme::Session::Session() : state{SessionState::create()} {}

scheme::Session::~Session() = default;

scheme::Session::Session(Session &&) noexcept = default;

scheme::Session &scheme::Session::operator=(Session &&) noexcept = default;

scheme::RunResult scheme::Session::run(
    std::string_view source,
    const Emit &emit
) {
  return state->run(source, emit);
}

void scheme::Session::execute(
    std::string_view source,
    const Emit &emit
) {
  state->execute(source, emit);
}
