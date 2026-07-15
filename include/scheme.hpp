#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

namespace scheme {

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
  class Impl;
  std::unique_ptr<Impl> impl;

public:
  Session();
  ~Session();

  Session(Session &&) noexcept;
  Session &operator=(Session &&) noexcept;

  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;

  RunResult run(std::string_view source, const Emit &emit);
  void execute(std::string_view source, const Emit &emit);
};

}
