#pragma once

#include <cstddef>
#include <optional>
#include <string>

class Arity {
  size_t minimum;
  std::optional<size_t> maximum;

  Arity(size_t minimum, std::optional<size_t> maximum);

public:
  static Arity exactly(size_t count);
  static Arity between(size_t minimum, size_t maximum);
  static Arity at_least(size_t minimum);

  std::optional<std::string> mismatch(size_t count) const;
};
