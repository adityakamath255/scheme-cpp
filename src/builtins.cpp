#include "builtins.hpp"
#include "eval.hpp"
#include "lex.hpp"
#include "parse.hpp"
#include "runtime.hpp"

#include <algorithm>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <ranges>
#include <sstream>
#include <utility>

static void check_arity(const std::vector<Obj> &args, std::string_view name,
                        size_t min, size_t max) {
  check_arity(args.size(), name, min, max);
}

static void check_type(Obj obj, bool (Obj::*pred)() const,
                       std::string_view type_name, std::string_view context) {
  if (!(obj.*pred)()) {
    throw SchemeError(std::format("{}: expected {}, got {}", context, type_name,
                                  obj.type_name()));
  }
}

static Number as_num(Obj obj, std::string_view context) {
  check_type(obj, &Obj::is_number, "number", context);
  return obj.as_number();
}

static size_t as_index(Obj obj, std::string_view context) {
  auto index = as_num(obj, context).to_size();
  if (!index) {
    throw SchemeError(
        std::format("{}: expected non-negative integer", context));
  }
  return *index;
}

static String *as_string(Obj obj, std::string_view context) {
  check_type(obj, &Obj::is_string, "string", context);
  return obj.as_string();
}

static Cons *as_cons(Obj obj, std::string_view context) {
  check_type(obj, &Obj::is_cons, "pair", context);
  return obj.as_cons();
}

static Vector *as_vector(Obj obj, std::string_view context) {
  check_type(obj, &Obj::is_vector, "vector", context);
  return obj.as_vector();
}

static char as_char(Obj obj, std::string_view context) {
  check_type(obj, &Obj::is_char, "char", context);
  return obj.as_char();
}

static Obj
numeric_compare(const std::vector<Obj> &args, std::string_view name,
                const std::function<bool(std::partial_ordering)> &ok) {
  check_arity(args, name, 1, SIZE_MAX);
  auto nums = args |
              std::views::transform([&](Obj a) { return as_num(a, name); }) |
              std::ranges::to<std::vector>();
  return std::ranges::adjacent_find(nums, [&](Number a, Number b) {
           return !ok(a.compare(b));
         }) == nums.end();
}

static Obj builtin_add(const std::vector<Obj> &args, Evaluator &evaluator) {
  return std::ranges::fold_left(args, Number::exact(0, evaluator),
                                [&evaluator](Number acc, Obj x) {
                                  return acc.add(as_num(x, "+"), evaluator);
                                });
}

static Obj builtin_sub(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "-", 1, SIZE_MAX);
  Number result = as_num(args[0], "-");
  if (args.size() == 1) {
    return result.neg(evaluator);
  } else {
    for (size_t i = 1; i < args.size(); i += 1) {
      result = result.sub(as_num(args[i], "-"), evaluator);
    }
    return result;
  }
}

static Obj builtin_mul(const std::vector<Obj> &args, Evaluator &evaluator) {
  return std::ranges::fold_left(args, Number::exact(1, evaluator),
                                [&evaluator](Number acc, Obj x) {
                                  return acc.mul(as_num(x, "*"), evaluator);
                                });
}

static Obj builtin_div(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "/", 1, SIZE_MAX);
  Number result = as_num(args[0], "/");
  if (args.size() == 1) {
    return Number::exact(1, evaluator).div(result, evaluator);
  } else {
    for (size_t i = 1; i < args.size(); i += 1) {
      result = result.div(as_num(args[i], "/"), evaluator);
    }
    return result;
  }
}

static Obj builtin_lt(const std::vector<Obj> &args, Evaluator &) {
  return numeric_compare(args, "<", [](std::partial_ordering o) {
    return o == std::partial_ordering::less;
  });
}

static Obj builtin_gt(const std::vector<Obj> &args, Evaluator &) {
  return numeric_compare(args, ">", [](std::partial_ordering o) {
    return o == std::partial_ordering::greater;
  });
}

static Obj builtin_num_eq(const std::vector<Obj> &args, Evaluator &) {
  return numeric_compare(args, "=", [](std::partial_ordering o) {
    return o == std::partial_ordering::equivalent;
  });
}

static Obj builtin_le(const std::vector<Obj> &args, Evaluator &) {
  return numeric_compare(args, "<=", [](std::partial_ordering o) {
    return o == std::partial_ordering::less ||
           o == std::partial_ordering::equivalent;
  });
}

static Obj builtin_ge(const std::vector<Obj> &args, Evaluator &) {
  return numeric_compare(args, ">=", [](std::partial_ordering o) {
    return o == std::partial_ordering::greater ||
           o == std::partial_ordering::equivalent;
  });
}

static Obj builtin_abs(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "abs", 1, 1);
  return as_num(args[0], "abs").abs(evaluator);
}

static Obj builtin_sqrt(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "sqrt", 1, 1);
  return as_num(args[0], "sqrt").sqrt(evaluator);
}

static Obj builtin_sin(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "sin", 1, 1);
  return std::sin(as_num(args[0], "sin").to_double());
}

static Obj builtin_cos(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "cos", 1, 1);
  return std::cos(as_num(args[0], "cos").to_double());
}

static Obj builtin_log(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "log", 1, 1);
  return std::log(as_num(args[0], "log").to_double());
}

static Obj builtin_expt(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "expt", 2, 2);
  return as_num(args[0], "expt").expt(as_num(args[1], "expt"), evaluator);
}

static Obj builtin_ceil(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "ceiling", 1, 1);
  Number n = as_num(args[0], "ceiling");
  return n.is_exact() ? n : Number::inexact(std::ceil(n.to_double()));
}

static Obj builtin_floor(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "floor", 1, 1);
  Number n = as_num(args[0], "floor");
  return n.is_exact() ? n : Number::inexact(std::floor(n.to_double()));
}

static Obj builtin_round(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "round", 1, 1);
  Number n = as_num(args[0], "round");
  return n.is_exact() ? n : Number::inexact(std::round(n.to_double()));
}

static Obj minmax(const std::vector<Obj> &args, std::string_view name,
                  std::partial_ordering want) {
  check_arity(args, name, 1, SIZE_MAX);
  auto nums = args |
              std::views::transform([&](Obj a) { return as_num(a, name); }) |
              std::ranges::to<std::vector>();
  bool inexact =
      std::ranges::any_of(nums, [](Number n) { return !n.is_exact(); });
  Number best = std::ranges::fold_left(
      nums | std::views::drop(1), nums[0],
      [want](Number a, Number b) { return b.compare(a) == want ? b : a; });
  return inexact ? best.to_inexact() : best;
}

static Obj builtin_max(const std::vector<Obj> &args, Evaluator &) {
  return minmax(args, "max", std::partial_ordering::greater);
}

static Obj builtin_min(const std::vector<Obj> &args, Evaluator &) {
  return minmax(args, "min", std::partial_ordering::less);
}

static Obj builtin_quotient(const std::vector<Obj> &args,
                            Evaluator &evaluator) {
  check_arity(args, "quotient", 2, 2);
  return as_num(args[0], "quotient")
      .quotient(as_num(args[1], "quotient"), evaluator);
}

static Obj builtin_remainder(const std::vector<Obj> &args,
                             Evaluator &evaluator) {
  check_arity(args, "remainder", 2, 2);
  return as_num(args[0], "remainder")
      .remainder(as_num(args[1], "remainder"), evaluator);
}

static Obj builtin_modulo(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "modulo", 2, 2);
  return as_num(args[0], "modulo").modulo(as_num(args[1], "modulo"), evaluator);
}

static Obj builtin_even(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "even?", 1, 1);
  return as_num(args[0], "even?").is_even();
}

static Obj builtin_odd(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "odd?", 1, 1);
  return !as_num(args[0], "odd?").is_even();
}

static Obj builtin_is_zero(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "zero?", 1, 1);
  return as_num(args[0], "zero?").is_zero();
}

static Obj builtin_is_positive(const std::vector<Obj> &args,
                               Evaluator &evaluator) {
  check_arity(args, "positive?", 1, 1);
  return as_num(args[0], "positive?").compare(Number::exact(0, evaluator)) ==
         std::partial_ordering::greater;
}

static Obj builtin_is_negative(const std::vector<Obj> &args,
                               Evaluator &evaluator) {
  check_arity(args, "negative?", 1, 1);
  return as_num(args[0], "negative?").compare(Number::exact(0, evaluator)) ==
         std::partial_ordering::less;
}

static Obj builtin_is_exact(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "exact?", 1, 1);
  return as_num(args[0], "exact?").is_exact();
}

static Obj builtin_is_inexact(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "inexact?", 1, 1);
  return !as_num(args[0], "inexact?").is_exact();
}

static Obj builtin_exact(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "exact", 1, 1);
  return as_num(args[0], "exact").to_exact(evaluator);
}

static Obj builtin_inexact(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "inexact", 1, 1);
  return as_num(args[0], "inexact").to_inexact();
}

static Obj builtin_is_null(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "null?", 1, 1);
  return args[0].is_null();
}

static Obj builtin_is_boolean(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "boolean?", 1, 1);
  return args[0].is_bool();
}

static Obj builtin_is_number(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "number?", 1, 1);
  return args[0].is_number();
}

static Obj builtin_is_integer(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "integer?", 1, 1);
  return args[0].is_number() && args[0].as_number().is_integer();
}

static Obj builtin_is_pair(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "pair?", 1, 1);
  return args[0].is_cons();
}

static Obj builtin_is_symbol(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "symbol?", 1, 1);
  return args[0].is_symbol();
}

static Obj builtin_is_string(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "string?", 1, 1);
  return args[0].is_string();
}

static Obj builtin_is_procedure(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "procedure?", 1, 1);
  return args[0].is_procedure() || args[0].is_builtin();
}

static Obj builtin_is_list(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "list?", 1, 1);
  return args[0].is_list();
}

static Obj builtin_is_void(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "void?", 1, 1);
  return args[0].is_void();
}

static Obj builtin_is_promise(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "promise?", 1, 1);
  return args[0].is_promise();
}

static Obj builtin_not(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "not", 1, 1);
  return args[0].is_false();
}

static Obj builtin_void(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "void", 0, 0);
  return Void{};
}

static Obj builtin_eq(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "eq?", 2, 2);
  Obj a = args[0], b = args[1];
  if (!a.same_type(b)) {
    return false;
  } else {
    switch (a.type()) {
    case Type::Bool:
      return a.as_bool() == b.as_bool();
    case Type::Number:
      return a.as_number().eqv(b.as_number());
    case Type::Char:
      return a.as_char() == b.as_char();
    case Type::Symbol:
      return a.as_symbol() == b.as_symbol();
    case Type::String:
      return a.as_string() == b.as_string();
    case Type::Cons:
      return a.as_cons() == b.as_cons();
    case Type::Vector:
      return a.as_vector() == b.as_vector();
    case Type::Procedure:
      return a.as_procedure() == b.as_procedure();
    case Type::Builtin:
      return a.as_builtin() == b.as_builtin();
    case Type::Promise:
      return a.as_promise() == b.as_promise();
    case Type::Error:
      return a.as_error() == b.as_error();
    case Type::Null:
    case Type::Void:
      return true;
    }
  }
  std::unreachable();
}

static Obj builtin_equal(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "equal?", 2, 2);
  return args[0].equals(args[1]);
}

static Obj builtin_car(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "car", 1, 1);
  return as_cons(args[0], "car")->car;
}

static Obj builtin_cdr(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "cdr", 1, 1);
  return as_cons(args[0], "cdr")->cdr;
}

static Obj builtin_cons(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "cons", 2, 2);
  return evaluator.alloc<Cons>(args[0], args[1]);
}

static Obj builtin_list(const std::vector<Obj> &args, Evaluator &evaluator) {
  return list_from(args, evaluator);
}

static Obj builtin_length(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "length", 1, 1);
  if (!args[0].is_null() && !args[0].is_cons()) {
    throw SchemeError("length: expected list, got " + args[0].type_name());
  }
  auto profile = args[0].list_profile();
  if (!profile.is_proper) {
    throw SchemeError("length: expected proper list");
  }
  return Number::exact(static_cast<int64_t>(profile.size), evaluator);
}

static Obj builtin_list_ref(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "list-ref", 2, 2);
  check_type(args[0], &Obj::is_cons, "pair", "list-ref");
  size_t index = as_index(args[1], "list-ref");
  Obj curr = args[0];
  for (size_t i = 0; i < index; i += 1) {
    if (!curr.is_cons()) {
      throw SchemeError("list-ref: index out of range");
    }
    curr = curr.cdr();
  }
  if (!curr.is_cons()) {
    throw SchemeError("list-ref: index out of range");
  }
  return curr.car();
}

static Obj builtin_set_car(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "set-car!", 2, 2);
  as_cons(args[0], "set-car!")->car = args[1];
  return Void{};
}

static Obj builtin_set_cdr(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "set-cdr!", 2, 2);
  as_cons(args[0], "set-cdr!")->cdr = args[1];
  return Void{};
}

static Obj builtin_string_length(const std::vector<Obj> &args,
                                 Evaluator &evaluator) {
  check_arity(args, "string-length", 1, 1);
  return Number::exact(
      static_cast<int64_t>(as_string(args[0], "string-length")->data.size()),
      evaluator);
}

static Obj builtin_string_ref(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "string-ref", 2, 2);
  const std::string &s = as_string(args[0], "string-ref")->data;
  size_t index = as_index(args[1], "string-ref");
  if (index >= s.size()) {
    throw SchemeError("string-ref: index out of range");
  }
  return s[index];
}

static Obj builtin_substring(const std::vector<Obj> &args,
                             Evaluator &evaluator) {
  check_arity(args, "substring", 2, 3);
  const std::string &s = as_string(args[0], "substring")->data;
  size_t start = as_index(args[1], "substring");
  size_t end = s.size();
  if (args.size() == 3) {
    end = as_index(args[2], "substring");
  }
  if (start > end || end > s.size()) {
    throw SchemeError("substring: index out of range");
  }
  return evaluator.alloc<String>(s.substr(start, end - start));
}

static Obj builtin_string_append(const std::vector<Obj> &args,
                                 Evaluator &evaluator) {
  return evaluator.alloc<String>(std::ranges::to<std::string>(
      args | std::views::transform([](Obj a) -> const std::string & {
        return as_string(a, "string-append")->data;
      }) |
      std::views::join));
}

static Obj builtin_string_eq(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "string=?", 2, 2);
  return as_string(args[0], "string=?")->data ==
         as_string(args[1], "string=?")->data;
}

static Obj builtin_is_char(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "char?", 1, 1);
  return args[0].is_char();
}

static Obj builtin_char_eq(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "char=?", 2, 2);
  return as_char(args[0], "char=?") == as_char(args[1], "char=?");
}

static Obj builtin_char_to_integer(const std::vector<Obj> &args,
                                   Evaluator &evaluator) {
  check_arity(args, "char->integer", 1, 1);
  return Number::exact(static_cast<int64_t>(static_cast<unsigned char>(
                           as_char(args[0], "char->integer"))),
                       evaluator);
}

static Obj builtin_integer_to_char(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "integer->char", 1, 1);
  size_t value = as_index(args[0], "integer->char");
  if (value > std::numeric_limits<unsigned char>::max()) {
    throw SchemeError("integer->char: value out of range");
  }
  return static_cast<char>(static_cast<unsigned char>(value));
}

static Obj builtin_string_to_list(const std::vector<Obj> &args,
                                  Evaluator &evaluator) {
  check_arity(args, "string->list", 1, 1);
  return list_from(as_string(args[0], "string->list")->data, evaluator);
}

static Obj builtin_list_to_string(const std::vector<Obj> &args,
                                  Evaluator &evaluator) {
  check_arity(args, "list->string", 1, 1);
  ListView list{args[0]};
  if (!list.tail().is_null()) {
    throw SchemeError("list->string: expected proper list");
  }
  return evaluator.alloc<String>(std::ranges::to<std::string>(
      list |
      std::views::transform([](Obj c) { return as_char(c, "list->string"); })));
}

static Obj builtin_number_to_string(const std::vector<Obj> &args,
                                    Evaluator &evaluator) {
  check_arity(args, "number->string", 1, 1);
  return evaluator.alloc<String>(
      as_num(args[0], "number->string").to_string());
}

static Obj builtin_string_to_number(const std::vector<Obj> &args,
                                    Evaluator &evaluator) {
  check_arity(args, "string->number", 1, 1);
  const std::string &s = as_string(args[0], "string->number")->data;
  try {
    return Number::parse(s, evaluator.runtime());
  } catch (const SchemeError &) {
    return false;
  }
}

static Obj builtin_symbol_to_string(const std::vector<Obj> &args,
                                    Evaluator &evaluator) {
  check_arity(args, "symbol->string", 1, 1);
  check_type(args[0], &Obj::is_symbol, "symbol", "symbol->string");
  return evaluator.alloc<String>(args[0].as_symbol().name());
}

static Obj builtin_string_to_symbol(const std::vector<Obj> &args,
                                    Evaluator &evaluator) {
  check_arity(args, "string->symbol", 1, 1);
  return evaluator.intern(as_string(args[0], "string->symbol")->data);
}

static Obj builtin_display(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "display", 1, 1);
  evaluator.output(args[0].to_display());
  return Void{};
}

static Obj builtin_write(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "write", 1, 1);
  evaluator.output(args[0].to_write());
  return Void{};
}

static Obj builtin_newline(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "newline", 0, 0);
  evaluator.output("\n");
  return Void{};
}

static Obj builtin_read(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "read", 0, 0);
  std::string input;
  while (true) {
    std::string line;
    if (!std::getline(std::cin, line)) {
      throw SchemeError("read: unexpected end of input");
    }
    if (!input.empty()) {
      input += '\n';
    }
    input += line;

    auto result = lex(input);
    if (result && !result->tokens.empty()) {
      return parse(result->tokens, evaluator.runtime());
    }
  }
}

static Obj builtin_is_vector(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "vector?", 1, 1);
  return args[0].is_vector();
}

static Obj builtin_vector(const std::vector<Obj> &args, Evaluator &evaluator) {
  return evaluator.alloc<Vector>(args);
}

static Obj builtin_make_vector(const std::vector<Obj> &args,
                               Evaluator &evaluator) {
  check_arity(args, "make-vector", 1, 2);
  size_t n = as_index(args[0], "make-vector");
  Obj fill = args.size() > 1 ? args[1] : Obj(Number::exact(0, evaluator));
  return evaluator.alloc<Vector>(std::vector<Obj>(n, fill));
}

static Obj builtin_vector_ref(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "vector-ref", 2, 2);
  Vector *v = as_vector(args[0], "vector-ref");
  size_t i = as_index(args[1], "vector-ref");
  if (i >= v->data.size()) {
    throw SchemeError("vector-ref: index out of range");
  }
  return v->data[i];
}

static Obj builtin_vector_set(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "vector-set!", 3, 3);
  Vector *v = as_vector(args[0], "vector-set!");
  size_t i = as_index(args[1], "vector-set!");
  if (i >= v->data.size()) {
    throw SchemeError("vector-set!: index out of range");
  }
  v->data[i] = args[2];
  return Void{};
}

static Obj builtin_vector_length(const std::vector<Obj> &args,
                                 Evaluator &evaluator) {
  check_arity(args, "vector-length", 1, 1);
  return Number::exact(
      static_cast<int64_t>(as_vector(args[0], "vector-length")->data.size()),
      evaluator);
}

static Obj builtin_vector_to_list(const std::vector<Obj> &args,
                                  Evaluator &evaluator) {
  check_arity(args, "vector->list", 1, 1);
  return list_from(as_vector(args[0], "vector->list")->data, evaluator);
}

static Obj builtin_list_to_vector(const std::vector<Obj> &args,
                                  Evaluator &evaluator) {
  check_arity(args, "list->vector", 1, 1);
  ListView list{args[0]};
  if (!list.tail().is_null()) {
    throw SchemeError("list->vector: expected proper list");
  }
  return evaluator.alloc<Vector>(std::ranges::to<std::vector>(list));
}

static Obj builtin_force(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "force", 1, 1);
  if (!args[0].is_promise()) {
    return args[0];
  }
  return args[0].as_promise()->force(evaluator);
}

static Obj builtin_error(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "error", 1, SIZE_MAX);
  Error *err = evaluator.alloc<Error>(
      args[0].to_display(), list_from(args | std::views::drop(1), evaluator));
  throw SchemeError::raised(err);
}

static Obj builtin_raise(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "raise", 1, 1);
  throw SchemeError::raised(args[0]);
}

static Obj builtin_is_error_object(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "error-object?", 1, 1);
  return args[0].is_error();
}

static Obj builtin_error_object_message(const std::vector<Obj> &args,
                                        Evaluator &evaluator) {
  check_arity(args, "error-object-message", 1, 1);
  check_type(args[0], &Obj::is_error, "error object", "error-object-message");
  return evaluator.alloc<String>(args[0].as_error()->message);
}

static Obj builtin_error_object_irritants(const std::vector<Obj> &args,
                                          Evaluator &) {
  check_arity(args, "error-object-irritants", 1, 1);
  check_type(args[0], &Obj::is_error, "error object", "error-object-irritants");
  return args[0].as_error()->irritants;
}

static Obj builtin_eval(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "eval", 1, 1);
  return evaluator.eval(args[0], evaluator.global_env());
}

static Obj builtin_load(const std::vector<Obj> &args, Evaluator &evaluator) {
  check_arity(args, "load", 1, 1);
  const std::string &path = as_string(args[0], "load")->data;

  std::ifstream file(path);
  if (!file) {
    throw SchemeError("load: could not open " + path);
  }
  std::ostringstream buf;
  buf << file.rdbuf();
  std::string source = buf.str();

  evaluator.execute(source, ResultMode::Suppress);

  return Obj(Void{});
}

static Obj builtin_file_exists(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "file-exists?", 1, 1);
  return Obj(std::ifstream(as_string(args[0], "file-exists?")->data).good());
}

static Obj builtin_exit(const std::vector<Obj> &args, Evaluator &) {
  check_arity(args, "exit", 0, 1);
  int code = 0;
  if (!args.empty()) {
    code = static_cast<int>(as_index(args[0], "exit"));
  }
  std::exit(code);
}

void install_builtins(Runtime &runtime) {
  Env &env = runtime.global_env;
  auto install = [&](std::string_view name, Builtin::Fn fn) {
    env.define(runtime.intern(name), runtime.alloc<Builtin>(fn));
  };

  install("+", builtin_add);
  install("-", builtin_sub);
  install("*", builtin_mul);
  install("/", builtin_div);

  install("<", builtin_lt);
  install(">", builtin_gt);
  install("=", builtin_num_eq);
  install("<=", builtin_le);
  install(">=", builtin_ge);

  install("abs", builtin_abs);
  install("sqrt", builtin_sqrt);
  install("sin", builtin_sin);
  install("cos", builtin_cos);
  install("log", builtin_log);
  install("expt", builtin_expt);
  install("ceiling", builtin_ceil);
  install("floor", builtin_floor);
  install("round", builtin_round);
  install("max", builtin_max);
  install("min", builtin_min);
  install("quotient", builtin_quotient);
  install("remainder", builtin_remainder);
  install("modulo", builtin_modulo);
  install("even?", builtin_even);
  install("odd?", builtin_odd);
  install("zero?", builtin_is_zero);
  install("positive?", builtin_is_positive);
  install("negative?", builtin_is_negative);
  install("exact?", builtin_is_exact);
  install("inexact?", builtin_is_inexact);
  install("exact", builtin_exact);
  install("inexact", builtin_inexact);
  install("inexact->exact", builtin_exact);
  install("exact->inexact", builtin_inexact);

  install("null?", builtin_is_null);
  install("boolean?", builtin_is_boolean);
  install("number?", builtin_is_number);
  install("integer?", builtin_is_integer);
  install("pair?", builtin_is_pair);
  install("symbol?", builtin_is_symbol);
  install("string?", builtin_is_string);
  install("procedure?", builtin_is_procedure);
  install("list?", builtin_is_list);
  install("void?", builtin_is_void);
  install("promise?", builtin_is_promise);
  install("not", builtin_not);
  install("void", builtin_void);

  install("eq?", builtin_eq);
  install("eqv?", builtin_eq);
  install("equal?", builtin_equal);

  install("car", builtin_car);
  install("cdr", builtin_cdr);
  install("cons", builtin_cons);
  install("list", builtin_list);
  install("length", builtin_length);
  install("list-ref", builtin_list_ref);
  install("set-car!", builtin_set_car);
  install("set-cdr!", builtin_set_cdr);

  install("string-length", builtin_string_length);
  install("string-ref", builtin_string_ref);
  install("substring", builtin_substring);
  install("string-append", builtin_string_append);
  install("string=?", builtin_string_eq);

  install("char?", builtin_is_char);
  install("char=?", builtin_char_eq);
  install("char->integer", builtin_char_to_integer);
  install("integer->char", builtin_integer_to_char);
  install("string->list", builtin_string_to_list);
  install("list->string", builtin_list_to_string);

  install("number->string", builtin_number_to_string);
  install("string->number", builtin_string_to_number);
  install("symbol->string", builtin_symbol_to_string);
  install("string->symbol", builtin_string_to_symbol);

  install("vector?", builtin_is_vector);
  install("vector", builtin_vector);
  install("make-vector", builtin_make_vector);
  install("vector-ref", builtin_vector_ref);
  install("vector-set!", builtin_vector_set);
  install("vector-length", builtin_vector_length);
  install("vector->list", builtin_vector_to_list);
  install("list->vector", builtin_list_to_vector);

  install("display", builtin_display);
  install("write", builtin_write);
  install("newline", builtin_newline);
  install("read", builtin_read);

  install("force", builtin_force);

  install("error", builtin_error);
  install("raise", builtin_raise);
  install("error-object?", builtin_is_error_object);
  install("error-object-message", builtin_error_object_message);
  install("error-object-irritants", builtin_error_object_irritants);
  install("eval", builtin_eval);
  env.define(runtime.intern("apply"),
              runtime.alloc<Builtin>(Builtin::Apply{}));
  install("load", builtin_load);
  install("file-exists?", builtin_file_exists);
  install("exit", builtin_exit);
}
