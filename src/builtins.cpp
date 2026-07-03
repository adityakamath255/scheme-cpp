#include "builtins.hpp"
#include "ctx.hpp"
#include "eval.hpp"
#include "lex.hpp"
#include "parse.hpp"
#include <stdexcept>
#include <format>
#include <cmath>
#include <iostream>
#include <sstream>
#include <fstream>
#include <charconv>

// --- helpers ---

static void check_arity(
  const std::vector<Obj> &args,
  std::string_view name,
  size_t min,
  size_t max
) {
  if (args.size() < min || args.size() > max) {
    throw std::runtime_error(
      std::format(
        "{}: expected {} arguments, got {}",
        name,
        (
          min == max
          ? std::to_string(min)
          : std::format("{}-{}", min, max)
        ),
        args.size()
      )
    );
  }
}

static void check_type(
  Obj obj,
  bool (Obj::*pred)() const,
  std::string_view type_name,
  std::string_view context
) {
  if (!(obj.*pred)()) {
    throw std::runtime_error(
      std::format(
        "{}: expected {}, got {}",
        context, type_name, obj.stringify_type()
      )
    );
  }
}

static double as_num(Obj obj, std::string_view context) {
  check_type(obj, &Obj::is_number, "number", context);
  return obj.as_number();
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

template<typename Comp>
static Obj numeric_compare(
  const std::vector<Obj> &args,
  std::string_view name,
  Comp comp
) {
  check_arity(args, name, 1, SIZE_MAX);
  for (size_t i = 1; i < args.size(); i += 1) {
    if (!comp(as_num(args[i - 1], name), as_num(args[i], name))) {
      return false;
    }
  }
  return true;
}

// --- arithmetic ---

static Obj builtin_add(const std::vector<Obj> &args, Ctx *) {
  double sum = 0;
  for (const auto &arg : args) {
    sum += as_num(arg, "+");
  }
  return sum;
}

static Obj builtin_sub(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "-", 1, SIZE_MAX);
  if (args.size() == 1) {
    return -as_num(args[0], "-");
  }
  else {
    double result = as_num(args[0], "-");
    for (size_t i = 1; i < args.size(); i += 1) {
      result -= as_num(args[i], "-");
    }
    return result;
  }
}

static Obj builtin_mul(const std::vector<Obj> &args, Ctx *) {
  double product = 1;
  for (const auto &arg : args) {
    product *= as_num(arg, "*");
  }
  return product;
}

static Obj builtin_div(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "/", 1, SIZE_MAX);
  if (args.size() == 1) {
    return 1.0 / as_num(args[0], "/");
  }
  else {
    double result = as_num(args[0], "/");
    for (size_t i = 1; i < args.size(); i += 1) {
      result /= as_num(args[i], "/");
    }
    return result;
  }
}

// --- comparison ---

static Obj builtin_lt(const std::vector<Obj> &args, Ctx *) {
  return numeric_compare(args, "<", [](double a, double b) { return a < b; });
}

static Obj builtin_gt(const std::vector<Obj> &args, Ctx *) {
  return numeric_compare(args, ">", [](double a, double b) { return a > b; });
}

static Obj builtin_num_eq(const std::vector<Obj> &args, Ctx *) {
  return numeric_compare(args, "=", [](double a, double b) { return a == b; });
}

static Obj builtin_le(const std::vector<Obj> &args, Ctx *) {
  return numeric_compare(args, "<=", [](double a, double b) { return a <= b; });
}

static Obj builtin_ge(const std::vector<Obj> &args, Ctx *) {
  return numeric_compare(args, ">=", [](double a, double b) { return a >= b; });
}

// --- math ---

static Obj builtin_abs(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "abs", 1, 1);
  return std::abs(as_num(args[0], "abs"));
}

static Obj builtin_sqrt(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "sqrt", 1, 1);
  return std::sqrt(as_num(args[0], "sqrt"));
}

static Obj builtin_sin(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "sin", 1, 1);
  return std::sin(as_num(args[0], "sin"));
}

static Obj builtin_cos(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "cos", 1, 1);
  return std::cos(as_num(args[0], "cos"));
}

static Obj builtin_log(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "log", 1, 1);
  return std::log(as_num(args[0], "log"));
}

static Obj builtin_expt(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "expt", 2, 2);
  return std::pow(as_num(args[0], "expt"), as_num(args[1], "expt"));
}

static Obj builtin_ceil(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "ceiling", 1, 1);
  return std::ceil(as_num(args[0], "ceiling"));
}

static Obj builtin_floor(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "floor", 1, 1);
  return std::floor(as_num(args[0], "floor"));
}

static Obj builtin_round(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "round", 1, 1);
  return std::round(as_num(args[0], "round"));
}

static Obj builtin_max(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "max", 1, SIZE_MAX);
  double result = as_num(args[0], "max");
  for (size_t i = 1; i < args.size(); i += 1) {
    result = std::max(result, as_num(args[i], "max"));
  }
  return result;
}

static Obj builtin_min(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "min", 1, SIZE_MAX);
  double result = as_num(args[0], "min");
  for (size_t i = 1; i < args.size(); i += 1) {
    result = std::min(result, as_num(args[i], "min"));
  }
  return result;
}

static Obj builtin_quotient(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "quotient", 2, 2);
  return std::trunc(
    as_num(args[0], "quotient") / as_num(args[1], "quotient")
  );
}

static Obj builtin_remainder(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "remainder", 2, 2);
  return std::fmod(
    as_num(args[0], "remainder"), as_num(args[1], "remainder")
  );
}

static Obj builtin_modulo(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "modulo", 2, 2);
  double a = as_num(args[0], "modulo");
  double b = as_num(args[1], "modulo");
  double r = std::fmod(a, b);
  if (r != 0 && ((r < 0) != (b < 0))) {
    r += b;
  }
  return r;
}

static Obj builtin_even(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "even?", 1, 1);
  return std::fmod(as_num(args[0], "even?"), 2) == 0;
}

static Obj builtin_odd(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "odd?", 1, 1);
  return std::fmod(as_num(args[0], "odd?"), 2) != 0;
}

// --- predicates ---

static Obj builtin_is_null(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "null?", 1, 1);
  return args[0].is_null();
}

static Obj builtin_is_boolean(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "boolean?", 1, 1);
  return args[0].is_bool();
}

static Obj builtin_is_number(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "number?", 1, 1);
  return args[0].is_number();
}

static Obj builtin_is_integer(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "integer?", 1, 1);
  return args[0].is_number()
    && std::isfinite(args[0].as_number())
    && std::trunc(args[0].as_number()) == args[0].as_number();
}

static Obj builtin_is_pair(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "pair?", 1, 1);
  return args[0].is_cons();
}

static Obj builtin_is_symbol(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "symbol?", 1, 1);
  return args[0].is_symbol();
}

static Obj builtin_is_string(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "string?", 1, 1);
  return args[0].is_string();
}

static Obj builtin_is_procedure(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "procedure?", 1, 1);
  return args[0].is_procedure() || args[0].is_builtin();
}

static Obj builtin_is_list(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "list?", 1, 1);
  return args[0].is_list();
}

static Obj builtin_is_void(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "void?", 1, 1);
  return args[0].is_void();
}

static Obj builtin_not(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "not", 1, 1);
  return args[0].is_false();
}

static Obj builtin_void(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "void", 0, 0);
  return Void{};
}

// --- equality ---

static Obj builtin_eq(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "eq?", 2, 2);
  Obj a = args[0], b = args[1];
  if (!a.same_type(b)) {
    return false;
  }
  else {
    switch (a.get_type()) {
      case Type::Bool: return a.as_bool() == b.as_bool();
      case Type::Number: return a.as_number() == b.as_number();
      case Type::Char: return a.as_char() == b.as_char();
      case Type::Symbol: return a.as_symbol() == b.as_symbol();
      case Type::String: return a.as_string() == b.as_string();
      case Type::Cons: return a.as_cons() == b.as_cons();
      case Type::Vector: return a.as_vector() == b.as_vector();
      case Type::Procedure: return a.as_procedure() == b.as_procedure();
      case Type::Builtin: return a.as_builtin() == b.as_builtin();
      case Type::Null:
      case Type::Void: return true;
      default: return false;
    }
  }
}

static Obj builtin_equal(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "equal?", 2, 2);
  return args[0].equals(args[1]);
}

// --- data ---

static Obj builtin_car(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "car", 1, 1);
  return as_cons(args[0], "car")->car;
}

static Obj builtin_cdr(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "cdr", 1, 1);
  return as_cons(args[0], "cdr")->cdr;
}

static Obj builtin_cons(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "cons", 2, 2);
  return ctx->alloc<Cons>(args[0], args[1]);
}

static Obj builtin_list(const std::vector<Obj> &args, Ctx *ctx) {
  Obj result = Null{};
  for (size_t i = args.size(); i > 0; ) {
    i -= 1;
    result = ctx->alloc<Cons>(args[i], result);
  }
  return result;
}

static Obj builtin_length(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "length", 1, 1);
  if (!args[0].is_null() && !args[0].is_cons()) {
    throw std::runtime_error(
      "length: expected list, got " + args[0].stringify_type()
    );
  }
  auto profile = args[0].get_list_profile();
  if (!profile.is_proper) {
    throw std::runtime_error("length: expected proper list");
  }
  return static_cast<double>(profile.size);
}

static Obj builtin_list_ref(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "list-ref", 2, 2);
  check_type(args[0], &Obj::is_cons, "pair", "list-ref");
  double raw = as_num(args[1], "list-ref");
  if (raw < 0) {
    throw std::runtime_error("list-ref: index must be non-negative");
  }
  size_t index = static_cast<size_t>(raw);
  Obj curr = args[0];
  for (size_t i = 0; i < index; i += 1) {
    if (!curr.is_cons()) {
      throw std::runtime_error("list-ref: index out of range");
    }
    curr = curr.cdr();
  }
  if (!curr.is_cons()) {
    throw std::runtime_error("list-ref: index out of range");
  }
  return curr.car();
}

static Obj builtin_set_car(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "set-car!", 2, 2);
  as_cons(args[0], "set-car!")->car = args[1];
  return Void{};
}

static Obj builtin_set_cdr(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "set-cdr!", 2, 2);
  as_cons(args[0], "set-cdr!")->cdr = args[1];
  return Void{};
}

// --- strings ---

static Obj builtin_string_length(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "string-length", 1, 1);
  return static_cast<double>(as_string(args[0], "string-length")->data.size());
}

static Obj builtin_string_ref(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "string-ref", 2, 2);
  const std::string &s = as_string(args[0], "string-ref")->data;
  double raw = as_num(args[1], "string-ref");
  if (raw < 0) {
    throw std::runtime_error("string-ref: index must be non-negative");
  }
  size_t index = static_cast<size_t>(raw);
  if (index >= s.size()) {
    throw std::runtime_error("string-ref: index out of range");
  }
  return s[index];
}

static Obj builtin_substring(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "substring", 2, 3);
  const std::string &s = as_string(args[0], "substring")->data;
  double raw_start = as_num(args[1], "substring");
  if (raw_start < 0) {
    throw std::runtime_error("substring: index must be non-negative");
  }
  size_t start = static_cast<size_t>(raw_start);
  size_t end = s.size();
  if (args.size() == 3) {
    double raw_end = as_num(args[2], "substring");
    if (raw_end < 0) {
      throw std::runtime_error("substring: index must be non-negative");
    }
    end = static_cast<size_t>(raw_end);
  }
  if (start > end || end > s.size()) {
    throw std::runtime_error("substring: index out of range");
  }
  return ctx->alloc<String>(s.substr(start, end - start));
}

static Obj builtin_string_append(const std::vector<Obj> &args, Ctx *ctx) {
  std::string result;
  for (const auto &arg : args) {
    result += as_string(arg, "string-append")->data;
  }
  return ctx->alloc<String>(std::move(result));
}

static Obj builtin_string_eq(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "string=?", 2, 2);
  return as_string(args[0], "string=?")->data
    == as_string(args[1], "string=?")->data;
}

// --- chars ---

static Obj builtin_is_char(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "char?", 1, 1);
  return args[0].is_char();
}

static Obj builtin_char_eq(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "char=?", 2, 2);
  return as_char(args[0], "char=?") == as_char(args[1], "char=?");
}

static Obj builtin_char_to_integer(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "char->integer", 1, 1);
  return static_cast<double>(as_char(args[0], "char->integer"));
}

static Obj builtin_integer_to_char(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "integer->char", 1, 1);
  double n = as_num(args[0], "integer->char");
  return static_cast<char>(static_cast<int>(n));
}

static Obj builtin_string_to_list(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "string->list", 1, 1);
  const std::string &s = as_string(args[0], "string->list")->data;
  Obj result = Null{};
  for (size_t i = s.size(); i > 0; ) {
    i -= 1;
    result = ctx->alloc<Cons>(s[i], result);
  }
  return result;
}

static Obj builtin_list_to_string(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "list->string", 1, 1);
  std::string result;
  Obj lst = args[0];
  while (lst.is_cons()) {
    result += as_char(lst.car(), "list->string");
    lst = lst.cdr();
  }
  if (!lst.is_null()) {
    throw std::runtime_error("list->string: expected proper list");
  }
  return ctx->alloc<String>(std::move(result));
}

// --- conversion ---

static Obj builtin_number_to_string(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "number->string", 1, 1);
  return ctx->alloc<String>(
    std::format("{}", as_num(args[0], "number->string"))
  );
}

static Obj builtin_string_to_number(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "string->number", 1, 1);
  const std::string &s = as_string(args[0], "string->number")->data;
  double val;
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
  if (ec != std::errc{} || ptr != s.data() + s.size()) {
    return false;
  }
  return val;
}

static Obj builtin_symbol_to_string(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "symbol->string", 1, 1);
  check_type(args[0], &Obj::is_symbol, "symbol", "symbol->string");
  return ctx->alloc<String>(args[0].as_symbol().get_name());
}

static Obj builtin_string_to_symbol(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "string->symbol", 1, 1);
  return ctx->intern(as_string(args[0], "string->symbol")->data);
}

// --- i/o ---

static Obj builtin_display(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "display", 1, 1);
  std::cout << args[0].stringify(false);
  std::cout.flush();
  return Void{};
}

static Obj builtin_write(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "write", 1, 1);
  std::cout << args[0].stringify(true);
  std::cout.flush();
  return Void{};
}

static Obj builtin_newline(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "newline", 0, 0);
  std::cout << '\n';
  std::cout.flush();
  return Void{};
}

static Obj builtin_read(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "read", 0, 0);
  std::string input;
  while (true) {
    std::string line;
    if (!std::getline(std::cin, line)) {
      throw std::runtime_error("read: unexpected end of input");
    }
    if (!input.empty()) {
      input += '\n';
    }
    input += line;

    auto result = lex(input);
    if (result && !result->tokens.empty()) {
      return parse(result->tokens, ctx);
    }
  }
}

// --- vectors ---

static Obj builtin_is_vector(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "vector?", 1, 1);
  return args[0].is_vector();
}

static Obj builtin_vector(const std::vector<Obj> &args, Ctx *ctx) {
  return ctx->alloc<Vector>(args);
}

static Obj builtin_make_vector(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "make-vector", 1, 2);
  double n = as_num(args[0], "make-vector");
  if (n < 0 || n != std::floor(n)) {
    throw std::runtime_error("make-vector: expected non-negative integer");
  }
  Obj fill = args.size() > 1 ? args[1] : Obj(0.0);
  return ctx->alloc<Vector>(std::vector<Obj>(static_cast<size_t>(n), fill));
}

static Obj builtin_vector_ref(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "vector-ref", 2, 2);
  Vector *v = as_vector(args[0], "vector-ref");
  double i = as_num(args[1], "vector-ref");
  if (i < 0 || i != std::floor(i) || static_cast<size_t>(i) >= v->data.size()) {
    throw std::runtime_error("vector-ref: index out of range");
  }
  return v->data[static_cast<size_t>(i)];
}

static Obj builtin_vector_set(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "vector-set!", 3, 3);
  Vector *v = as_vector(args[0], "vector-set!");
  double i = as_num(args[1], "vector-set!");
  if (i < 0 || i != std::floor(i) || static_cast<size_t>(i) >= v->data.size()) {
    throw std::runtime_error("vector-set!: index out of range");
  }
  v->data[static_cast<size_t>(i)] = args[2];
  return Void{};
}

static Obj builtin_vector_length(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "vector-length", 1, 1);
  return static_cast<double>(as_vector(args[0], "vector-length")->data.size());
}

static Obj builtin_vector_to_list(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "vector->list", 1, 1);
  Vector *v = as_vector(args[0], "vector->list");
  Obj result = Null{};
  for (size_t i = v->data.size(); i > 0; ) {
    i -= 1;
    result = ctx->alloc<Cons>(v->data[i], result);
  }
  return result;
}

static Obj builtin_list_to_vector(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "list->vector", 1, 1);
  std::vector<Obj> elements;
  Obj lst = args[0];
  while (lst.is_cons()) {
    elements.push_back(lst.car());
    lst = lst.cdr();
  }
  if (!lst.is_null()) {
    throw std::runtime_error("list->vector: expected proper list");
  }
  return ctx->alloc<Vector>(std::move(elements));
}

// --- misc ---

static Obj builtin_error(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "error", 1, SIZE_MAX);
  std::ostringstream msg;
  msg << args[0].stringify(false);
  for (size_t i = 1; i < args.size(); i += 1) {
    msg << " " << args[i].stringify(true);
  }
  throw std::runtime_error(msg.str());
}

static Obj builtin_eval(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "eval", 1, 1);
  return eval(args[0], ctx->global_env, ctx);
}

static Obj builtin_load(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "load", 1, 1);
  const std::string &path = as_string(args[0], "load")->data;

  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("load: could not open " + path);
  }
  std::ostringstream buf;
  buf << file.rdbuf();
  std::string source = buf.str();

  Env *env = ctx->global_env;
  while (true) {
    auto result = lex(source);
    if (!result || result->tokens.empty()) break;
    Obj expr = parse(result->tokens, ctx);
    eval(expr, env, ctx);
    source = result->rest;
    if (ctx->should_recycle()) ctx->recycle();
  }

  return Obj(Void{});
}

static Obj builtin_file_exists(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "file-exists?", 1, 1);
  return Obj(std::ifstream(as_string(args[0], "file-exists?")->data).good());
}

static Obj builtin_exit(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "exit", 0, 1);
  int code = 0;
  if (!args.empty()) {
    code = static_cast<int>(as_num(args[0], "exit"));
  }
  std::exit(code);
}

static Obj builtin_apply(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "apply", 2, SIZE_MAX);

  Obj proc = args[0];
  if (!proc.is_procedure() && !proc.is_builtin()) {
    throw std::runtime_error(
      "apply: expected procedure, got " + proc.stringify_type()
    );
  }

  std::vector<Obj> call_args;
  for (size_t i = 1; i + 1 < args.size(); i += 1) {
    call_args.push_back(args[i]);
  }

  Obj tail = args.back();
  while (tail.is_cons()) {
    call_args.push_back(tail.car());
    tail = tail.cdr();
  }

  if (!tail.is_null()) {
    throw std::runtime_error("apply: last argument must be a proper list");
  }

  if (proc.is_builtin()) {
    return proc.as_builtin()->fn(call_args, ctx);
  }

  else {
    Procedure *p = proc.as_procedure();
    Env *call_env = ctx->alloc<LocalEnv>(p->env);
    bind_args(call_env, p->params, call_args, p->variadic, ctx);
    return eval(p->body, call_env, ctx);
  }
}

// --- public ---

void install_builtins(Ctx *ctx) {
  Env *env = ctx->global_env;
  auto install = [&](std::string_view name, Builtin::Fn fn) {
    env->define(ctx->intern(name), ctx->alloc<Builtin>(fn));
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
  install("not", builtin_not);
  install("void", builtin_void);

  install("eq?", builtin_eq);
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

  install("error", builtin_error);
  install("eval", builtin_eval);
  install("apply", builtin_apply);
  install("load", builtin_load);
  install("file-exists?", builtin_file_exists);
  install("exit", builtin_exit);
}
