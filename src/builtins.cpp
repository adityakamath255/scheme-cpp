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

static void install(
  Env *env,
  Ctx *ctx,
  std::string_view name,
  Builtin::Fn fn
) {
  env->define(ctx->intern(name), ctx->alloc<Builtin>(fn));
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
  else if (a.is_cons()) {
    return a.as_cons() == b.as_cons();
  }
  else if (a.is_string()) {
    return a.as_string() == b.as_string();
  }
  else if (a.is_procedure()) {
    return a.as_procedure() == b.as_procedure();
  }
  else if (a.is_builtin()) {
    return a.as_builtin() == b.as_builtin();
  }
  return a.equals(b);
}

static Obj builtin_equal(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "equal?", 2, 2);
  return args[0].equals(args[1]);
}

// --- data ---

static Obj builtin_car(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "car", 1, 1);
  check_type(args[0], &Obj::is_cons, "pair", "car");
  return args[0].car();
}

static Obj builtin_cdr(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "cdr", 1, 1);
  check_type(args[0], &Obj::is_cons, "pair", "cdr");
  return args[0].cdr();
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
  check_type(args[0], &Obj::is_cons, "pair", "set-car!");
  args[0].as_cons()->car = args[1];
  return Void{};
}

static Obj builtin_set_cdr(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "set-cdr!", 2, 2);
  check_type(args[0], &Obj::is_cons, "pair", "set-cdr!");
  args[0].as_cons()->cdr = args[1];
  return Void{};
}

// --- strings ---

static Obj builtin_string_length(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "string-length", 1, 1);
  check_type(args[0], &Obj::is_string, "string", "string-length");
  return static_cast<double>(args[0].as_string()->data.size());
}

static Obj builtin_string_ref(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "string-ref", 2, 2);
  check_type(args[0], &Obj::is_string, "string", "string-ref");
  const std::string &s = args[0].as_string()->data;
  double raw = as_num(args[1], "string-ref");
  if (raw < 0) {
    throw std::runtime_error("string-ref: index must be non-negative");
  }
  size_t index = static_cast<size_t>(raw);
  if (index >= s.size()) {
    throw std::runtime_error("string-ref: index out of range");
  }
  return ctx->alloc<String>(std::string(1, s[index]));
}

static Obj builtin_substring(const std::vector<Obj> &args, Ctx *ctx) {
  check_arity(args, "substring", 2, 3);
  check_type(args[0], &Obj::is_string, "string", "substring");
  const std::string &s = args[0].as_string()->data;
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
    check_type(arg, &Obj::is_string, "string", "string-append");
    result += arg.as_string()->data;
  }
  return ctx->alloc<String>(std::move(result));
}

static Obj builtin_string_eq(const std::vector<Obj> &args, Ctx *) {
  check_arity(args, "string=?", 2, 2);
  check_type(args[0], &Obj::is_string, "string", "string=?");
  check_type(args[1], &Obj::is_string, "string", "string=?");
  return args[0].as_string()->data == args[1].as_string()->data;
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
  check_type(args[0], &Obj::is_string, "string", "string->number");
  const std::string &s = args[0].as_string()->data;
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
  check_type(args[0], &Obj::is_string, "string", "string->symbol");
  return ctx->intern(args[0].as_string()->data);
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
  return eval(args[0], ctx->get_global_env(), ctx);
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
    Env *call_env = ctx->alloc<Env>(p->env);
    bind_args(call_env, p->params, call_args, p->variadic, ctx);
    return eval(p->body, call_env, ctx);
  }
}

// --- public ---

void install_builtins(Env *env, Ctx *ctx) {
  // arithmetic
  install(env, ctx, "+", builtin_add);
  install(env, ctx, "-", builtin_sub);
  install(env, ctx, "*", builtin_mul);
  install(env, ctx, "/", builtin_div);

  // comparison
  install(env, ctx, "<", builtin_lt);
  install(env, ctx, ">", builtin_gt);
  install(env, ctx, "=", builtin_num_eq);
  install(env, ctx, "<=", builtin_le);
  install(env, ctx, ">=", builtin_ge);

  // math
  install(env, ctx, "abs", builtin_abs);
  install(env, ctx, "sqrt", builtin_sqrt);
  install(env, ctx, "sin", builtin_sin);
  install(env, ctx, "cos", builtin_cos);
  install(env, ctx, "log", builtin_log);
  install(env, ctx, "expt", builtin_expt);
  install(env, ctx, "ceiling", builtin_ceil);
  install(env, ctx, "floor", builtin_floor);
  install(env, ctx, "round", builtin_round);
  install(env, ctx, "max", builtin_max);
  install(env, ctx, "min", builtin_min);
  install(env, ctx, "quotient", builtin_quotient);
  install(env, ctx, "remainder", builtin_remainder);
  install(env, ctx, "modulo", builtin_modulo);
  install(env, ctx, "even?", builtin_even);
  install(env, ctx, "odd?", builtin_odd);

  // predicates
  install(env, ctx, "null?", builtin_is_null);
  install(env, ctx, "boolean?", builtin_is_boolean);
  install(env, ctx, "number?", builtin_is_number);
  install(env, ctx, "integer?", builtin_is_integer);
  install(env, ctx, "pair?", builtin_is_pair);
  install(env, ctx, "symbol?", builtin_is_symbol);
  install(env, ctx, "string?", builtin_is_string);
  install(env, ctx, "procedure?", builtin_is_procedure);
  install(env, ctx, "list?", builtin_is_list);
  install(env, ctx, "void?", builtin_is_void);
  install(env, ctx, "not", builtin_not);
  install(env, ctx, "void", builtin_void);

  // equality
  install(env, ctx, "eq?", builtin_eq);
  install(env, ctx, "equal?", builtin_equal);

  // data
  install(env, ctx, "car", builtin_car);
  install(env, ctx, "cdr", builtin_cdr);
  install(env, ctx, "cons", builtin_cons);
  install(env, ctx, "list", builtin_list);
  install(env, ctx, "length", builtin_length);
  install(env, ctx, "list-ref", builtin_list_ref);
  install(env, ctx, "set-car!", builtin_set_car);
  install(env, ctx, "set-cdr!", builtin_set_cdr);

  // strings
  install(env, ctx, "string-length", builtin_string_length);
  install(env, ctx, "string-ref", builtin_string_ref);
  install(env, ctx, "substring", builtin_substring);
  install(env, ctx, "string-append", builtin_string_append);
  install(env, ctx, "string=?", builtin_string_eq);

  // conversion
  install(env, ctx, "number->string", builtin_number_to_string);
  install(env, ctx, "string->number", builtin_string_to_number);
  install(env, ctx, "symbol->string", builtin_symbol_to_string);
  install(env, ctx, "string->symbol", builtin_string_to_symbol);

  // i/o
  install(env, ctx, "display", builtin_display);
  install(env, ctx, "write", builtin_write);
  install(env, ctx, "newline", builtin_newline);
  install(env, ctx, "read", builtin_read);

  // misc
  install(env, ctx, "error", builtin_error);
  install(env, ctx, "eval", builtin_eval);
  install(env, ctx, "apply", builtin_apply);
}
