#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

struct BigInt;
class EvalContext;
struct HeapEntity;

class Number {
  std::variant<int64_t, BigInt *, double> rep;
  explicit Number(std::variant<int64_t, BigInt *, double> rep);

public:
  static Number exact(int64_t v, EvalContext &context);
  static Number inexact(double v);
  static Number parse(std::string_view lexeme, EvalContext &context);

  bool is_exact() const;
  bool is_integer() const;
  bool is_zero() const;
  bool is_even() const;

  double to_double() const;
  std::optional<size_t> to_size() const;

  Number add(Number o, EvalContext &context) const;
  Number sub(Number o, EvalContext &context) const;
  Number mul(Number o, EvalContext &context) const;
  Number div(Number o, EvalContext &context) const;
  Number neg(EvalContext &context) const;
  Number abs(EvalContext &context) const;
  Number sqrt(EvalContext &context) const;
  Number quotient(Number o, EvalContext &context) const;
  Number remainder(Number o, EvalContext &context) const;
  Number modulo(Number o, EvalContext &context) const;
  Number expt(Number power, EvalContext &context) const;
  Number to_inexact() const;
  Number to_exact(EvalContext &context) const;

  std::partial_ordering compare(Number o) const;
  bool eqv(Number o) const;

  std::string to_string() const;
  HeapEntity *heap_entity() const;
};
