#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

struct BigInt;
class Ctx;
struct HeapEntity;

class Number {
  std::variant<int64_t, BigInt *, double> rep;
  explicit Number(std::variant<int64_t, BigInt *, double> rep);

public:
  static Number exact(int64_t v, Ctx &context);
  static Number inexact(double v);
  static Number parse(std::string_view lexeme, Ctx &context);

  bool is_exact() const;
  bool is_integer() const;
  bool is_zero() const;
  bool is_even() const;

  double to_double() const;
  std::optional<size_t> to_size() const;

  Number add(Number o, Ctx &context) const;
  Number sub(Number o, Ctx &context) const;
  Number mul(Number o, Ctx &context) const;
  Number div(Number o, Ctx &context) const;
  Number neg(Ctx &context) const;
  Number abs(Ctx &context) const;
  Number sqrt(Ctx &context) const;
  Number quotient(Number o, Ctx &context) const;
  Number remainder(Number o, Ctx &context) const;
  Number modulo(Number o, Ctx &context) const;
  Number expt(Number power, Ctx &context) const;
  Number to_inexact() const;
  Number to_exact(Ctx &context) const;

  std::partial_ordering compare(Number o) const;
  bool eqv(Number o) const;
  bool operator==(Number o) const;

  std::string to_string() const;
  HeapEntity *heap_entity() const;
};
