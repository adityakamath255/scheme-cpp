#include "scheme/session.hpp"

#include "ctx.hpp"

#include <memory>

scheme::ExitRequest::ExitRequest(int status) noexcept
    : requested_status{status} {}

int scheme::ExitRequest::status() const noexcept {
  return requested_status;
}

const char *scheme::ExitRequest::what() const noexcept {
  return "Scheme program requested exit";
}

scheme::Session::Session() : state{std::make_unique<::Ctx>()} {}

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
