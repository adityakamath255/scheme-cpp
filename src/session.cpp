#include "scheme.hpp"

#include "builtins.hpp"
#include "eval.hpp"
#include "preamble.hpp"
#include "runtime.hpp"

#include <utility>

class scheme::Session::Impl {
public:
  Runtime runtime;

  Impl() {
    install_builtins(runtime);
    const Emit ignore;
    Evaluator evaluator {runtime, ignore};
    evaluator.execute(
      preamble,
      ResultMode::Suppress
    );
  }
};

scheme::Session::Session(): impl {std::make_unique<Impl>()} {}

scheme::Session::~Session() = default;

scheme::Session::Session(Session &&) noexcept = default;

scheme::Session &scheme::Session::operator=(Session &&) noexcept = default;

scheme::RunResult scheme::Session::run(
  std::string_view source,
  const Emit &emit
) {
  Evaluator evaluator {impl->runtime, emit};
  return evaluator.run(
    source,
    ResultMode::Emit
  );
}

void scheme::Session::execute(
  std::string_view source,
  const Emit &emit
) {
  Evaluator evaluator {impl->runtime, emit};
  evaluator.execute(
    source,
    ResultMode::Emit
  );
}
