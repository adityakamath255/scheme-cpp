#include "eval.hpp"
#include "ctx.hpp"
#include <stdexcept>
#include <format>

// --- internal types ---

struct TailCall {
  Obj expr;
  Env *env;
};

class EvalResult {
  std::variant<Obj, TailCall> data;

public:
  EvalResult(Obj data): data {data} {}
  EvalResult(TailCall data): data {std::move(data)} {}

  bool is_obj() const {
    return holds_alternative<Obj>(data);
  }

  bool is_tail_call() const {
    return holds_alternative<TailCall>(data);
  }

  Obj as_obj() const {
    return get<Obj>(data);
  }

  TailCall as_tail_call() const {
    return get<TailCall>(data);
  }
};

// --- arity checking ---

static void check_arity(
  Obj rest,
  std::string_view name,
  size_t min,
  size_t max
) {
  auto profile = rest.get_list_profile();

  if (!profile.is_proper) {
    throw std::runtime_error(
      std::format("{}: improper argument list", name)
    );
  }

  if (profile.size < min || profile.size > max) {
    throw std::runtime_error(
      std::format(
        "{}: expected {} arguments, got {}",
        name,
        (
          min == max
          ? std::to_string(min)
          : std::format("{}-{}", min, max)
        ),
        profile.size
      )
    );
  }
}

// --- helpers ---

static std::vector<Obj> eval_args(Obj list, Env *env, Ctx *ctx) {
  std::vector<Obj> res;
  while (list.is_cons()) {
    res.push_back(eval(list.car(), env, ctx));
    list = list.cdr();
  }
  return res;
}

struct Params {
  std::vector<Symbol> names;
  bool variadic;
};

static Params extract_params(Obj formals) {
  std::vector<Symbol> names;
  bool variadic;

  // (lambda x body)
  if (formals.is_symbol()) {
    names.push_back(formals.as_symbol());
    variadic = true;
  }

  // (lambda (a b) body) or (lambda (a . b) body)
  else {
    while (formals.is_cons()) {
      if (!formals.car().is_symbol()) {
        throw std::runtime_error("parameter must be a symbol");
      }
      names.push_back(formals.car().as_symbol());
      formals = formals.cdr();
    }

    if (formals.is_null()) {
      variadic = false;
    }

    else if (formals.is_symbol()) {
      names.push_back(formals.as_symbol());
      variadic = true;
    }

    else {
      throw std::runtime_error("invalid parameter list");
    }
  }

  return {names, variadic};
}

static void bind_args(
  Env *env,
  const std::vector<Symbol> &params,
  const std::vector<Obj> &args,
  bool variadic,
  Ctx *ctx
) {
  if (variadic) {
    if (args.size() + 1 < params.size()) {
      throw std::runtime_error("too few arguments");
    }
    for (size_t i = 0; i + 1 < params.size(); i += 1) {
      env->define(params[i], args[i]);
    }
    Obj rest = Null{};
    for (size_t i = args.size(); i > params.size() - 1; ) {
      i -= 1;
      rest = ctx->alloc<Cons>(args[i], rest);
    }
    env->define(params.back(), rest);
  }
  else {
    if (args.size() != params.size()) {
      throw std::runtime_error("wrong number of arguments");
    }
    for (size_t i = 0; i < params.size(); i += 1) {
      env->define(params[i], args[i]);
    }
  }
}

static Obj wrap_body(Obj body_list, Ctx *ctx) {
  if (body_list.cdr().is_null()) {
    return body_list.car();
  }
  return ctx->alloc<Cons>(
    ctx->intern("begin"),
    body_list
  );
}

static EvalResult eval_body(Obj list, Env *env, Ctx *ctx) {
  while (list.cdr().is_cons()) {
    eval(list.car(), env, ctx);
    list = list.cdr();
  }
  return TailCall{list.car(), env};
}

static Obj eval_quasiquote(Obj obj, Env *env, Ctx *ctx) {
  if (!obj.is_cons()) {
    return obj;
  }

  else if (
    obj.car().is_symbol() 
    && obj.car().as_symbol().get_name() == "unquote"
  ) {
    return eval(obj.cdr().car(), env, ctx);
  }

  else {
    std::vector<Obj> elements;
    Obj tail = obj;
    while (tail.is_cons()) {
      elements.push_back(eval_quasiquote(tail.car(), env, ctx));
      tail = tail.cdr();
    }

    Obj result = (
      tail.is_null() 
      ? Null{}
      : eval_quasiquote(tail, env, ctx)
    );

    for (size_t i = elements.size(); i > 0; ) {
      i -= 1;
      result = ctx->alloc<Cons>(elements[i], result);
    }

    return result;
  }
}

// --- special forms ---

static EvalResult eval_quote(Obj rest) {
  check_arity(rest, "quote", 1, 1);
  return rest.car();
}

static EvalResult eval_if(Obj rest, Env *env, Ctx *ctx) {
  check_arity(rest, "if", 2, 3);
  Obj pred = eval(rest.car(), env, ctx);

  if (pred.is_true()) {
    return TailCall{rest.cdr().car(), env};
  }

  else if (rest.cdr().cdr().is_cons()) {
    return TailCall{rest.cdr().cdr().car(), env};
  }

  else {
    return Obj(Void{});
  }
}

static EvalResult eval_define(Obj rest, Env *env, Ctx *ctx) {
  check_arity(rest, "define", 1, SIZE_MAX);
  Obj target = rest.car();

  if (target.is_cons()) {
    Symbol fname = target.car().as_symbol();
    auto [params, variadic] = extract_params(target.cdr());
    Obj body = wrap_body(rest.cdr(), ctx);

    env->define(
      fname,
      ctx->alloc<Procedure>(
        std::move(params), body, env, variadic
      )
    );

    return Obj(Void{});
  }

  else if (target.is_symbol()) {
    Symbol sym = target.as_symbol();

    if (rest.cdr().is_null()) {
      env->define(sym, Void{});
    }
    else {
      check_arity(rest, "define", 2, 2);
      env->define(sym, eval(rest.cdr().car(), env, ctx));
    }
    return Obj(Void{});
  }

  else {
    throw std::runtime_error(
      "define: expected symbol or list, got "
      + target.stringify_type()
    );
  }
}

static EvalResult eval_set(Obj rest, Env *env, Ctx *ctx) {
  check_arity(rest, "set!", 2, 2);
  if (!rest.car().is_symbol()) {
    throw std::runtime_error(
      "set!: expected symbol, got "
      + rest.car().stringify_type()
    );
  }
  Symbol sym = rest.car().as_symbol();
  Obj val = eval(rest.cdr().car(), env, ctx);
  if (!env->set(sym, val)) {
    throw std::runtime_error(
      "set!: undefined variable "
      + sym.get_name()
    );
  }

  return Obj(Void{});
}

static EvalResult eval_lambda(Obj rest, Env *env, Ctx *ctx) {
  check_arity(rest, "lambda", 2, SIZE_MAX);
  auto [params, variadic] = extract_params(rest.car());
  Obj body = wrap_body(rest.cdr(), ctx);
  return Obj(ctx->alloc<Procedure>(
    std::move(params), body, env, variadic
  ));
}

static EvalResult eval_begin(Obj rest, Env *env, Ctx *ctx) {
  if (rest.is_null()) {
    return Obj(Void{});
  }
  else {
    return eval_body(rest, env, ctx);
  }
}

static EvalResult eval_let(Obj rest, Env *env, Ctx *ctx, bool sequential) {
  check_arity(rest, sequential ? "let*" : "let", 2, SIZE_MAX);
  Obj bindings = rest.car();
  Obj body_list = rest.cdr();
  Env *new_env = ctx->alloc<Env>(env);

  while (bindings.is_cons()) {
    Obj binding = bindings.car();
    if (!binding.car().is_symbol()) {
      throw std::runtime_error("let: binding name must be a symbol");
    }
    Symbol sym = binding.car().as_symbol();
    Obj val = eval(binding.cdr().car(), sequential ? new_env : env, ctx);
    new_env->define(sym, val);
    bindings = bindings.cdr();
  }

  return eval_body(body_list, new_env, ctx);
}

static EvalResult eval_cond(Obj clauses, Env *env, Ctx *ctx) {
  while (clauses.is_cons()) {
    Obj clause = clauses.car();
    if (!clause.is_cons()) {
      throw std::runtime_error("cond: clause must be a list");
    }

    Obj test_expr = clause.car();
    Obj body = clause.cdr();

    bool is_else = (
      test_expr.is_symbol()
      && test_expr.as_symbol().get_name() == "else"
    );

    if (is_else && clauses.cdr().is_cons()) {
      throw std::runtime_error("cond: else must be the last clause");
    }

    if (is_else && body.is_null()) {
      throw std::runtime_error("cond: else clause must have a body");
    }

    Obj test_val = is_else ? true : eval(test_expr, env, ctx);

    if (is_else || test_val.is_true()) {
      if (body.is_null()) {
        return test_val;
      }
      return eval_body(body, env, ctx);
    }

    clauses = clauses.cdr();
  }

  return Obj(Void{});
}

static EvalResult eval_and(Obj rest, Env *env, Ctx *ctx) {
  if (rest.is_null()) {
    return Obj(true);
  }
  else {
    while (rest.cdr().is_cons()) {
      Obj val = eval(rest.car(), env, ctx);
      if (val.is_false()) {
        return val;
      }
      else {
        rest = rest.cdr();
      }
    }
    return TailCall{rest.car(), env};
  }
}

static EvalResult eval_or(Obj rest, Env *env, Ctx *ctx) {
  if (rest.is_null()) {
    return Obj(false);
  }
  else {
    while (rest.cdr().is_cons()) {
      Obj val = eval(rest.car(), env, ctx);
      if (val.is_true()) {
        return val;
      }
      else {
        rest = rest.cdr();
      }
    }
    return TailCall{rest.car(), env};
  }
}

// --- dispatch ---

static EvalResult eval_quasiquote_form(Obj rest, Env *env, Ctx *ctx) {
  check_arity(rest, "quasiquote", 1, 1);
  return eval_quasiquote(rest.car(), env, ctx);
}

static EvalResult eval_apply(Obj head, Obj rest, Env *env, Ctx *ctx) {
  Obj proc = eval(head, env, ctx);
  std::vector<Obj> args = eval_args(rest, env, ctx);

  if (proc.is_procedure()) {
    Procedure *p = proc.as_procedure();
    Env *new_env = ctx->alloc<Env>(p->env);
    bind_args(new_env, p->params, args, p->variadic, ctx);
    return TailCall{p->body, new_env};
  }

  else if (proc.is_builtin()) {
    return proc.as_builtin()->fn(args, ctx);
  }

  throw std::runtime_error(
    "not a procedure: " + proc.stringify()
  );
}

static EvalResult eval_expr(Obj expr, Env *env, Ctx *ctx) {
  if (!expr.is_symbol() && !expr.is_cons()) {
    return expr;
  }

  else if (expr.is_symbol()) {
    auto result = env->lookup(expr.as_symbol());

    if (!result) {
      throw std::runtime_error(
        "undefined variable: " + expr.as_symbol().get_name()
      );
    }

    return *result;
  }

  else {
    Obj head = expr.car();
    Obj rest = expr.cdr();

    if (head.is_symbol()) {
      const std::string &name = head.as_symbol().get_name();

      if (name == "quote") {
        return eval_quote(rest);
      }
      else if (name == "if") { 
        return eval_if(rest, env, ctx);
      }
      else if (name == "define") {
        return eval_define(rest, env, ctx);
      }
      else if (name == "set!") {
        return eval_set(rest, env, ctx);
      }
      else if (name == "lambda") {
        return eval_lambda(rest, env, ctx);
      }
      else if (name == "begin") {
        return eval_begin(rest, env, ctx);
      }
      else if (name == "let") {
        return eval_let(rest, env, ctx, false);
      }
      else if (name == "let*") {
        return eval_let(rest, env, ctx, true);
      }
      else if (name == "cond") {
        return eval_cond(rest, env, ctx);
      }
      else if (name == "and") {
        return eval_and(rest, env, ctx);
      }
      else if (name == "or") {
        return eval_or(rest, env, ctx);
      }
      else if (name == "quasiquote") {
        return eval_quasiquote_form(rest, env, ctx);
      }
    }

    return eval_apply(head, rest, env, ctx);
  }
}

// --- public ---

Obj eval(Obj expr, Env *env, Ctx *ctx) {
  auto result = eval_expr(expr, env, ctx);
  while (result.is_tail_call()) {
    auto tc = result.as_tail_call();
    result = eval_expr(tc.expr, tc.env, ctx);
  }
  return result.as_obj();
}
