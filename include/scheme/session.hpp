#pragma once

#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace scheme {

class SessionState;

class EvaluationError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class ExitRequest : public std::exception {
  int requested_status;

public:
  explicit ExitRequest(int status) noexcept;

  int status() const noexcept;
  const char *what() const noexcept override;
};

struct Output {
  std::string text;
};

struct Result {
  std::string text;
};

using Event = std::variant<Output, Result>;
using Emit = std::function<void(const Event &)>;

struct RunResult {
  size_t consumed;
  bool incomplete;
};

class Session {
  std::unique_ptr<SessionState> state;

public:
  Session();
  ~Session();

  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;
  Session(Session &&) noexcept;
  Session &operator=(Session &&) noexcept;

  [[nodiscard]] RunResult run(std::string_view source, const Emit &emit);
  void execute(std::string_view source, const Emit &emit);
};

}
