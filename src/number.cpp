#include "types.hpp"
#include "ctx.hpp"

#include <tommath.h>

#include <charconv>
#include <cmath>
#include <cstring>
#include <format>
#include <functional>
#include <limits>

namespace {

void check(mp_err e) {
  if (e != MP_OKAY) {
    throw SchemeError("bignum operation failed");
  }
}

}

struct BigInt : HeapEntity {
  mp_int val;
  BigInt() { check(mp_init(&val)); }
  ~BigInt() override { mp_clear(&val); }
};

namespace {

using Rep = std::variant<int64_t, BigInt *, double>;

struct Mp {
  mp_int v;
  Mp() { check(mp_init(&v)); }
  ~Mp() { mp_clear(&v); }
  Mp(const Mp &) = delete;
  Mp &operator=(const Mp &) = delete;
};

// a bignum never holds a value that fits a fixnum

constexpr int64_t fixnum_min = std::numeric_limits<int64_t>::min() + 1;
constexpr int64_t fixnum_max = std::numeric_limits<int64_t>::max();

Rep of_i64(int64_t v, Ctx *ctx) {
  if (v < fixnum_min) {
    BigInt *b = ctx->alloc<BigInt>();
    mp_set_i64(&b->val, v);
    return b;
  }
  else {
    return v;
  }
}

Rep from_bigint(BigInt *b, Ctx *ctx) {
  if (mp_count_bits(&b->val) < 64) {
    return of_i64(mp_get_i64(&b->val), ctx);
  }
  else {
    return b;
  }
}

const mp_int *as_mp(const Rep &r, Mp &scratch) {
  if (auto p = std::get_if<BigInt *>(&r)) {
    return &(*p)->val;
  }
  else {
    mp_set_i64(&scratch.v, std::get<int64_t>(r));
    return &scratch.v;
  }
}

bool rep_is_exact(const Rep &r) { return !std::holds_alternative<double>(r); }

double rep_to_double(const Rep &r) {
  return std::visit(overloaded {
    [](int64_t v) { return static_cast<double>(v); },
    [](BigInt *b) { return mp_get_double(&b->val); },
    [](double d)  { return d; },
  }, r);
}

bool rep_is_zero(const Rep &r) {
  return std::visit(overloaded {
    [](int64_t v) { return v == 0; },
    [](BigInt *b) { return bool(mp_iszero(&b->val)); },
    [](double d)  { return d == 0.0; },
  }, r);
}

bool rep_is_negative(const Rep &r) {
  return std::visit(overloaded {
    [](int64_t v) { return v < 0; },
    [](BigInt *b) { return bool(mp_isneg(&b->val)); },
    [](double d)  { return d < 0.0; },
  }, r);
}

using MpBinop = mp_err (*)(const mp_int *, const mp_int *, mp_int *);
using MpUnop  = mp_err (*)(const mp_int *, mp_int *);

Rep exact_binop(const Rep &a, const Rep &b, Ctx *ctx, MpBinop op) {
  BigInt *r = ctx->alloc<BigInt>();
  Mp sa, sb;
  check(op(as_mp(a, sa), as_mp(b, sb), &r->val));
  return from_bigint(r, ctx);
}

template<typename Op>
Rep arith(const Rep &a, const Rep &b, Ctx *ctx, Op op, MpBinop mp) {
  if (rep_is_exact(a) && rep_is_exact(b)) {
    auto ai = std::get_if<int64_t>(&a);
    auto bi = std::get_if<int64_t>(&b);
    if (ai && bi) {
      __int128 w = op(static_cast<__int128>(*ai), static_cast<__int128>(*bi));
      if (w >= fixnum_min && w <= fixnum_max) {
        return of_i64(static_cast<int64_t>(w), ctx);
      }
    }
    return exact_binop(a, b, ctx, mp);
  }
  else {
    return op(rep_to_double(a), rep_to_double(b));
  }
}

template<typename Fix>
Rep unary(const Rep &a, Ctx *ctx, MpUnop mp, Fix fix) {
  return std::visit(overloaded {
    [&](int64_t v) -> Rep { return of_i64(fix(v), ctx); },
    [&](BigInt *b) -> Rep {
      BigInt *r = ctx->alloc<BigInt>();
      check(mp(&b->val, &r->val));
      return from_bigint(r, ctx);
    },
    [&](double d) -> Rep { return fix(d); },
  }, a);
}

struct QuotRem { Rep quot; Rep rem; };

QuotRem divmod(const Rep &a, const Rep &b, Ctx *ctx) {
  if (auto ai = std::get_if<int64_t>(&a)) {
    if (auto bi = std::get_if<int64_t>(&b)) {
      return { of_i64(*ai / *bi, ctx), of_i64(*ai % *bi, ctx) };
    }
  }
  BigInt *q = ctx->alloc<BigInt>();
  BigInt *r = ctx->alloc<BigInt>();
  Mp sa, sb;
  check(mp_div(as_mp(a, sa), as_mp(b, sb), &q->val, &r->val));
  return { from_bigint(q, ctx), from_bigint(r, ctx) };
}

}  // namespace

Number::Number(Rep r): rep {std::move(r)} {}

Number Number::exact(int64_t v, Ctx *ctx) { 
  return Number(of_i64(v, ctx)); 
}

Number Number::inexact(double v) { 
  return Number(Rep {v});
}

bool Number::is_exact() const { 
  return rep_is_exact(rep); 
}

bool Number::is_zero() const { 
  return rep_is_zero(rep); 
}

double Number::to_double() const { 
  return rep_to_double(rep); 
}

bool Number::is_integer() const {
  if (is_exact()) {
    return true;
  }
  else {
    double d = std::get<double>(rep);
    return std::isfinite(d) && std::trunc(d) == d;
  }
}

bool Number::is_even() const {
  return std::visit(overloaded {
    [](int64_t v) { return (v & 1) == 0; },
    [](BigInt *b) { return bool(mp_iseven(&b->val)); },
    [](double d)  { return std::fmod(d, 2.0) == 0.0; },
  }, rep);
}

Number Number::add(Number o, Ctx *ctx) const { 
  return Number(arith(
    rep, o.rep, ctx, std::plus<>{}, mp_add
  ));
}

Number Number::sub(Number o, Ctx *ctx) const { 
  return Number(arith(
    rep, o.rep, ctx, std::minus<>{}, mp_sub
  )); 
}

Number Number::mul(Number o, Ctx *ctx) const {
  return Number(arith(
    rep, o.rep, ctx, std::multiplies<>{}, mp_mul
  ));
}

Number Number::div(Number o, Ctx *ctx) const {
  if (is_exact() && o.is_exact()) {
    if (o.is_zero()) {
      throw SchemeError("/: division by zero");
    }
    auto [q, r] = divmod(rep, o.rep, ctx);
    if (rep_is_zero(r)) {
      return Number(q);
    }
  }
  return inexact(to_double() / o.to_double());
}

Number Number::neg(Ctx *ctx) const {
  return Number(unary(
    rep, ctx, mp_neg, [](auto x) { return -x; }
  ));
}

Number Number::abs(Ctx *ctx) const {
  return Number(unary(
    rep, ctx, mp_abs, [](auto x) { return x < 0 ? -x : x; }
  ));
}

Number Number::sqrt(Ctx *ctx) const {
  if (is_exact() && !rep_is_negative(rep)) {
    Mp scratch;
    const mp_int *m = as_mp(rep, scratch);
    bool square = false;
    check(mp_is_square(m, &square));
    if (square) {
      BigInt *r = ctx->alloc<BigInt>();
      check(mp_sqrt(m, &r->val));
      return Number(from_bigint(r, ctx));
    }
  }
  return inexact(std::sqrt(to_double()));
}

Number Number::quotient(Number o, Ctx *ctx) const {
  if (o.is_zero()) {
    throw SchemeError("quotient: division by zero");
  }

  if (is_exact() && o.is_exact()) {
    return Number(divmod(rep, o.rep, ctx).quot);
  }
  else {
    return inexact(std::trunc(to_double() / o.to_double()));
  }
}
Number Number::remainder(Number o, Ctx *ctx) const {
  if (o.is_zero()) {
    throw SchemeError("remainder: division by zero");
  }

  if (is_exact() && o.is_exact()) {
    return Number(divmod(rep, o.rep, ctx).rem);
  }
  else {
    return inexact(std::fmod(to_double(), o.to_double()));
  }
}

Number Number::modulo(Number o, Ctx *ctx) const {
  if (o.is_zero()) {
    throw SchemeError("modulo: division by zero");
  }

  Number r = remainder(o, ctx);
  if (
    !r.is_zero() 
    && rep_is_negative(r.rep) != rep_is_negative(o.rep)
  ) {
    return r.add(o, ctx);
  }
  else {
    return r;
  }
}

Number Number::expt(Number power, Ctx *ctx) const {
  if (is_exact()) {
    if (auto e = std::get_if<int64_t>(&power.rep)) {
      if (*e >= 0 && *e <= std::numeric_limits<int>::max()) {
        BigInt *r = ctx->alloc<BigInt>();
        Mp sb;
        check(mp_expt_n(as_mp(rep, sb), static_cast<int>(*e), &r->val));
        return Number(from_bigint(r, ctx));
      }
    }
  }
  return inexact(std::pow(to_double(), power.to_double()));
}

Number Number::to_inexact() const {
  return is_exact() ? inexact(to_double()) : *this;
}

Number Number::to_exact(Ctx *ctx) const {
  if (is_exact()) {
    return *this;
  }
  else {
    double d = std::get<double>(rep);
    if (!std::isfinite(d) || std::trunc(d) != d) {
      throw SchemeError("inexact->exact: not an integer");
    }
    if (d < static_cast<double>(fixnum_min)
        || d > static_cast<double>(fixnum_max)) {
      throw SchemeError("inexact->exact: magnitude too large");
    }
    return Number(of_i64(static_cast<int64_t>(d), ctx));
  }
}

std::partial_ordering Number::compare(Number o) const {
  if (is_exact() && o.is_exact()) {
    auto x = std::get_if<int64_t>(&rep);
    auto y = std::get_if<int64_t>(&o.rep);

    if (x && y) {
      return *x <=> *y;
    }

    else if (x) {
      BigInt *b = std::get<BigInt *>(o.rep);
      return mp_isneg(&b->val)
        ? std::partial_ordering::greater
        : std::partial_ordering::less;
    }

    else if (y) {
      BigInt *a = std::get<BigInt *>(rep);
      return mp_isneg(&a->val) 
        ? std::partial_ordering::less
        : std::partial_ordering::greater;
    }

    else {
      BigInt *a = std::get<BigInt *>(rep);
      BigInt *b = std::get<BigInt *>(o.rep);
      switch (mp_cmp(&a->val, &b->val)) {
        case MP_LT: return std::partial_ordering::less;
        case MP_GT: return std::partial_ordering::greater;
        default:    return std::partial_ordering::equivalent;
      }
    }
  }

  else {
    return to_double() <=> o.to_double();
  }
}

bool Number::eqv(Number o) const {
  return is_exact() == o.is_exact()
    && compare(o) == std::partial_ordering::equivalent;
}

Number Number::parse(std::string_view lexeme, Ctx *ctx) {
  bool inexactp = lexeme.find_first_of(".eE") != std::string_view::npos;
  const char *begin = lexeme.data();
  const char *end = lexeme.data() + lexeme.size();

  if (inexactp) {
    double d;
    auto [p, ec] = std::from_chars(begin, end, d);
    if (ec != std::errc{} || p != end) {
      throw SchemeError("invalid number");
    }
    return inexact(d);
  }

  else {
    int64_t v;
    auto [p, ec] = std::from_chars(begin, end, v);

    if (ec == std::errc{} && p == end) {
      return Number(of_i64(v, ctx));
    }
    else if (ec == std::errc::result_out_of_range) {
      std::string s(lexeme);
      BigInt *b = ctx->alloc<BigInt>();
      check(mp_read_radix(&b->val, s.c_str(), 10));
      return Number(from_bigint(b, ctx));
    }
    else {
      throw SchemeError("invalid number");
    }
  }
}

std::string Number::to_string() const {
  return std::visit(overloaded {
    [](int64_t v) -> std::string { return std::to_string(v); },
    [](BigInt *b) -> std::string {
      size_t size;
      check(mp_radix_size(&b->val, 10, &size));
      std::string s(size, '\0');
      check(mp_to_radix(&b->val, s.data(), size, nullptr, 10));
      s.resize(std::strlen(s.c_str()));
      return s;
    },
    [](double d) -> std::string {
      std::string s = std::format("{}", d);
      if (
        std::isfinite(d)
        && s.find_first_of(".eEnN") == std::string::npos
      ) {
        return s + ".0";
      }
      else {
        return s;
      }
    },
  }, rep);
}

std::optional<HeapEntity *> Number::heap_entity() const {
  return std::visit(overloaded {
    [](int64_t)   -> std::optional<HeapEntity *> { return std::nullopt; },
    [](BigInt *b) -> std::optional<HeapEntity *> { return b; },
    [](double)    -> std::optional<HeapEntity *> { return std::nullopt; },
  }, rep);
}
