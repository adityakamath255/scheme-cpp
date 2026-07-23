#include "arity.hpp"

#include <format>
#include <stdexcept>

Arity::Arity(size_t minimum, std::optional<size_t> maximum)
    : minimum{minimum}, maximum{maximum} {
  if (maximum && *maximum < minimum) {
    throw std::invalid_argument("arity maximum is less than minimum");
  }
}

Arity Arity::exactly(size_t count) { return Arity{count, count}; }

Arity Arity::between(size_t minimum, size_t maximum) {
  return Arity{minimum, maximum};
}

Arity Arity::at_least(size_t minimum) {
  return Arity{minimum, std::nullopt};
}

std::optional<std::string> Arity::mismatch(size_t count) const {
  if (count >= minimum && (!maximum || count <= *maximum)) {
    return std::nullopt;
  }

  std::string expected;
  if (!maximum) {
    expected = std::format("{} or more", minimum);
  } else if (*maximum == minimum) {
    expected = std::to_string(minimum);
  } else {
    expected = std::format("{}-{}", minimum, *maximum);
  }
  return std::format("expected {} arguments, got {}", expected, count);
}
