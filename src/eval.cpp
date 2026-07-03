#include "eval.hpp"
#include "ctx.hpp"

#include <stdexcept>
#include <format>
#include <ranges>

// --- internal types ---

struct TailCall {
  Obj expr;
  Env *env;
};

using EvalResult = std::variant<Obj, TailCall>;

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
  return ListView{list}
    | std::views::transform([&](Obj arg) { return eval(arg, env, ctx); })
    | std::ranges::to<std::vector>();
}

struct Params {
  std::vector<Symbol> names;
  bool variadic;
};

static Params extract_params(Obj formals) {
  // (lambda x body)
  if (formals.is_symbol()) {
    return {{formals.as_symbol()}, true};
  }

  // (lambda (a b) body) or (lambda (a . b) body)
  else {
    ListView params{formals};
    std::vector<Symbol> names;

    for (Obj p : params) {
      if (!p.is_symbol()) {
        throw std::runtime_error("parameter must be a symbol");
      }
      names.push_back(p.as_symbol());
    }

    Obj tail = params.tail();

    if (tail.is_null()) {
      return {std::move(names), false};
    }
    else if (tail.is_symbol()) {
      names.push_back(tail.as_symbol());
      return {std::move(names), true};
    }
    else {
      throw std::runtime_error("invalid parameter list");
    }
  }
}

static Obj wrap_body(Obj body_list, Ctx *ctx) {
  if (body_list.cdr().is_null()) {
    return body_list.car();
  }
  return ctx->alloc<Cons>(
    ctx->sym_begin,
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
  if (obj.is_vector()) {
    auto vec = obj.as_vector();
    std::vector<Obj> elements;
    for (auto elem : vec->data) {
      elements.push_back(eval_quasiquote(elem, env, ctx));
    }
    return ctx->alloc<Vector>(std::move(elements));
  }

  else if (!obj.is_cons()) {
    return obj;
  }

  else if (
    obj.car().is_symbol()
    && obj.car().as_symbol() == ctx->sym_unquote
  ) {
    return eval(obj.cdr().car(), env, ctx);
  }

  else {
    ListView items{obj};
    std::vector<Obj> elements;

    for (Obj elem : items) {
      if (
        elem.is_cons()
        && elem.car().is_symbol()
        && elem.car().as_symbol() == ctx->sym_unquote_splicing
      ) {
        elements.append_range(ListView{eval(elem.cdr().car(), env, ctx)});
      }
      else {
        elements.push_back(eval_quasiquote(elem, env, ctx));
      }
    }

    Obj raw_tail = items.tail();
    Obj tail = raw_tail.is_null() ? Obj(Null{}) : eval_quasiquote(raw_tail, env, ctx);
    return list_from(elements, ctx, tail);
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
        std::move(params), body, env, variadic, false
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

static EvalResult eval_define_macro(Obj rest, Env *env, Ctx *ctx) {
  check_arity(rest, "define-macro", 2, SIZE_MAX);
  Obj target = rest.car();

  if (target.is_cons()) {
    Symbol name = target.car().as_symbol();
    auto [params, variadic] = extract_params(target.cdr());
    Obj body = wrap_body(rest.cdr(), ctx);

    env->define(
      name,
      ctx->alloc<Procedure>(
        std::move(params), body, env, variadic, true
      )
    );
  }

  else if (target.is_symbol()) {
    check_arity(rest, "define-macro", 2, 2);
    Obj val = eval(rest.cdr().car(), env, ctx);

    if (!val.is_procedure()) {
      throw std::runtime_error("define-macro: expected procedure");
    }

    Procedure *p = val.as_procedure();

    env->define(
      target.as_symbol(),
      ctx->alloc<Procedure>(p->params, p->body, p->env, p->variadic, true)
    );
  }

  else {
    throw std::runtime_error(
      "define-macro: expected symbol or list"
    );
  }

  return Obj(Void{});
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
    std::move(params), body, env, variadic, false
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

enum class LetKind { Plain, Star, Rec };

static EvalResult eval_let(Obj rest, Env *env, Ctx *ctx, LetKind kind) {
  const char *name =
    kind == LetKind::Star ? "let*"
    : kind == LetKind::Rec ? "letrec"
    : "let";
  check_arity(rest, name, 2, SIZE_MAX);
  Obj bindings = rest.car();
  Obj body_list = rest.cdr();
  Env *new_env = ctx->alloc<LocalEnv>(env);

  if (kind == LetKind::Rec) {
    for (Obj binding : ListView{bindings}) {
      if (!binding.car().is_symbol()) {
        throw std::runtime_error("letrec: binding name must be a symbol");
      }
      new_env->define(binding.car().as_symbol(), Void{});
    }
  }

  Env *init_env = kind == LetKind::Plain ? env : new_env;

  for (Obj binding : ListView{bindings}) {
    if (!binding.car().is_symbol()) {
      throw std::runtime_error(
        std::string(name) + ": binding name must be a symbol"
      );
    }
    Symbol sym = binding.car().as_symbol();
    Obj val = eval(binding.cdr().car(), init_env, ctx);
    if (kind == LetKind::Rec) {
      new_env->set(sym, val);
    } else {
      new_env->define(sym, val);
    }
  }

  return eval_body(body_list, new_env, ctx);
}

static EvalResult eval_named_let(Obj rest, Env *env, Ctx *ctx) {
  Symbol name = rest.car().as_symbol();
  Obj spec = rest.cdr();
  check_arity(spec, "let", 2, SIZE_MAX);
  Obj bindings = spec.car();
  Obj body = wrap_body(spec.cdr(), ctx);

  std::vector<Symbol> params;
  std::vector<Obj> args;
  for (Obj binding : ListView{bindings}) {
    if (!binding.car().is_symbol()) {
      throw std::runtime_error("let: binding name must be a symbol");
    }
    params.push_back(binding.car().as_symbol());
    args.push_back(eval(binding.cdr().car(), env, ctx));
  }

  Env *loop_env = ctx->alloc<LocalEnv>(env);
  Procedure *proc = ctx->alloc<Procedure>(
    std::move(params), body, loop_env, false, false
  );
  loop_env->define(name, proc);

  Env *call_env = ctx->alloc<LocalEnv>(loop_env);
  bind_args(call_env, proc->params, args, false, ctx);
  return TailCall{proc->body, call_env};
}

static EvalResult eval_when(Obj rest, Env *env, Ctx *ctx, bool negate) {
  check_arity(rest, negate ? "unless" : "when", 1, SIZE_MAX);
  Obj test = eval(rest.car(), env, ctx);
  bool go = negate ? test.is_false() : test.is_true();
  Obj body = rest.cdr();

  if (!go || body.is_null()) {
    return Obj(Void{});
  }
  return eval_body(body, env, ctx);
}

static EvalResult eval_case(Obj rest, Env *env, Ctx *ctx) {
  check_arity(rest, "case", 1, SIZE_MAX);
  Obj key = eval(rest.car(), env, ctx);
  Obj clauses = rest.cdr();

  while (clauses.is_cons()) {
    Obj clause = clauses.car();
    if (!clause.is_cons()) {
      throw std::runtime_error("case: clause must be a list");
    }

    Obj datums = clause.car();
    Obj body = clause.cdr();

    bool is_else = (
      datums.is_symbol()
      && datums.as_symbol() == ctx->sym_else
    );

    if (is_else && clauses.cdr().is_cons()) {
      throw std::runtime_error("case: else must be the last clause");
    }

    bool matched = is_else;
    for (Obj d = datums; !matched && d.is_cons(); d = d.cdr()) {
      matched = key.equals(d.car());
    }

    if (matched) {
      if (body.is_null()) {
        return Obj(Void{});
      }
      return eval_body(body, env, ctx);
    }

    clauses = clauses.cdr();
  }

  return Obj(Void{});
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
      && test_expr.as_symbol() == ctx->sym_else
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

// `and` returns the first false operand (identity #t); `or` returns the first
// true operand (identity #f). Both tail-call the last operand.
static EvalResult eval_and_or(Obj rest, Env *env, Ctx *ctx, bool conjunction) {
  if (rest.is_null()) {
    return Obj(conjunction);
  }
  while (rest.cdr().is_cons()) {
    Obj val = eval(rest.car(), env, ctx);
    if (val.is_true() != conjunction) {
      return val;
    }
    rest = rest.cdr();
  }
  return TailCall{rest.car(), env};
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
    Env *new_env = ctx->alloc<LocalEnv>(p->env);
    bind_args(new_env, p->params, args, p->variadic, ctx);
    return TailCall{p->body, new_env};
  }

  else if (proc.is_builtin()) {
    return proc.as_builtin()->fn(args, ctx);
  }

  throw std::runtime_error(
    "not a procedure: " + proc.to_display()
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
      Symbol sym = head.as_symbol();

      if (sym == ctx->sym_quote) {
        return eval_quote(rest);
      }
      else if (sym == ctx->sym_if) {
        return eval_if(rest, env, ctx);
      }
      else if (sym == ctx->sym_define) {
        return eval_define(rest, env, ctx);
      }
      else if (sym == ctx->sym_set) {
        return eval_set(rest, env, ctx);
      }
      else if (sym == ctx->sym_lambda) {
        return eval_lambda(rest, env, ctx);
      }
      else if (sym == ctx->sym_begin) {
        return eval_begin(rest, env, ctx);
      }
      else if (sym == ctx->sym_let) {
        if (rest.is_cons() && rest.car().is_symbol()) {
          return eval_named_let(rest, env, ctx);
        }
        return eval_let(rest, env, ctx, LetKind::Plain);
      }
      else if (sym == ctx->sym_letstar) {
        return eval_let(rest, env, ctx, LetKind::Star);
      }
      else if (sym == ctx->sym_letrec) {
        return eval_let(rest, env, ctx, LetKind::Rec);
      }
      else if (sym == ctx->sym_when) {
        return eval_when(rest, env, ctx, false);
      }
      else if (sym == ctx->sym_unless) {
        return eval_when(rest, env, ctx, true);
      }
      else if (sym == ctx->sym_case) {
        return eval_case(rest, env, ctx);
      }
      else if (sym == ctx->sym_cond) {
        return eval_cond(rest, env, ctx);
      }
      else if (sym == ctx->sym_and) {
        return eval_and_or(rest, env, ctx, true);
      }
      else if (sym == ctx->sym_or) {
        return eval_and_or(rest, env, ctx, false);
      }
      else if (sym == ctx->sym_quasiquote) {
        return eval_quasiquote_form(rest, env, ctx);
      }
      else if (sym == ctx->sym_define_macro) {
        return eval_define_macro(rest, env, ctx);
      }

      auto macro_val = env->lookup(sym);

      if (
        macro_val 
        && macro_val->is_procedure()
        && macro_val->as_procedure()->macro
      ) {
        Procedure *p = macro_val->as_procedure();
        std::vector<Obj> raw_args = std::ranges::to<std::vector>(ListView{rest});
        Env *macro_env = ctx->alloc<LocalEnv>(p->env);
        bind_args(macro_env, p->params, raw_args, p->variadic, ctx);
        Obj expanded = eval(p->body, macro_env, ctx);
        return TailCall{expanded, env};
      }
    }

    return eval_apply(head, rest, env, ctx);
  }
}

// --- public ---

void bind_args(
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
    env->define(
      params.back(),
      list_from(args | std::views::drop(params.size() - 1), ctx)
    );
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

Obj eval(Obj expr, Env *env, Ctx *ctx) {
  EvalResult result = eval_expr(expr, env, ctx);
  while (auto *tc = std::get_if<TailCall>(&result)) {
    result = eval_expr(tc->expr, tc->env, ctx);
  }
  return std::get<Obj>(result);
}
