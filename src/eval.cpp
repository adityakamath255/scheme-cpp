#include "eval.hpp"
#include "ctx.hpp"
#include "builtins.hpp"

#include <format>
#include <ranges>
#include <unordered_map>

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
    throw SchemeError(
      std::format("{}: improper argument list", name)
    );
  }

  if (profile.size < min || profile.size > max) {
    throw SchemeError(
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

Formals Formals::parse(Obj formals) {
  // (lambda x body)
  if (formals.is_symbol()) {
    return {{formals.as_symbol()}, true};
  }

  // (lambda (a b) body) or (lambda (a . b) body)
  ListView params{formals};
  std::vector<Symbol> names;

  for (Obj p : params) {
    if (!p.is_symbol()) {
      throw SchemeError("parameter must be a symbol");
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
    throw SchemeError("invalid parameter list");
  }
}

void Formals::bind(Env *env, const std::vector<Obj> &args, Ctx *ctx) const {
  size_t fixed = names.size() - (variadic ? 1 : 0);

  if (args.size() < fixed) {
    throw SchemeError("too few arguments");
  }
  if (!variadic && args.size() > fixed) {
    throw SchemeError("too many arguments");
  }

  for (size_t i = 0; i < fixed; i += 1) {
    env->define(names[i], args[i]);
  }
  if (variadic) {
    env->define(names.back(), list_from(args | std::views::drop(fixed), ctx));
  }
}

static bool is_keyword(Obj obj, std::string_view name) {
  return obj.is_symbol() && obj.as_symbol().get_name() == name;
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

std::pair<Obj, std::vector<Obj>> splice_apply(const std::vector<Obj> &args) {
  if (args.size() < 2) {
    throw SchemeError("apply: expected at least 2 arguments");
  }

  ListView rest{args.back()};
  if (!rest.tail().is_null()) {
    throw SchemeError("apply: last argument must be a proper list");
  }

  std::vector<Obj> call_args(args.begin() + 1, args.end() - 1);
  call_args.append_range(rest);
  return {args.front(), std::move(call_args)};
}

static EvalResult apply_procedure(Obj proc, std::vector<Obj> args, Ctx *ctx) {
  while (true) {
    if (proc.is_procedure()) {
      Procedure *p = proc.as_procedure();
      Env *new_env = ctx->alloc<LocalEnv>(p->env);
      p->formals.bind(new_env, args, ctx);
      return TailCall{p->body, new_env};
    }

    if (proc.is_builtin()) {
      Builtin *b = proc.as_builtin();
      if (b->fn != builtin_apply) {
        return b->fn(args, ctx);
      }
      auto [next_proc, next_args] = splice_apply(args);
      proc = next_proc;
      args = std::move(next_args);
    }

    else {
      throw SchemeError("not a procedure: " + proc.to_display());
    }
  }
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

  else if (is_keyword(obj.car(), "unquote")) {
    return eval(obj.cdr().car(), env, ctx);
  }

  else {
    ListView items{obj};
    std::vector<Obj> elements;

    for (Obj elem : items) {
      if (elem.is_cons() && is_keyword(elem.car(), "unquote-splicing")) {
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
    Obj body = wrap_body(rest.cdr(), ctx);

    env->define(
      fname,
      ctx->alloc<Procedure>(
        Formals::parse(target.cdr()), body, env, false
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
    throw SchemeError(
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
    Obj body = wrap_body(rest.cdr(), ctx);

    env->define(
      name,
      ctx->alloc<Procedure>(
        Formals::parse(target.cdr()), body, env, true
      )
    );
  }

  else if (target.is_symbol()) {
    check_arity(rest, "define-macro", 2, 2);
    Obj val = eval(rest.cdr().car(), env, ctx);

    if (!val.is_procedure()) {
      throw SchemeError("define-macro: expected procedure");
    }

    Procedure *p = val.as_procedure();

    env->define(
      target.as_symbol(),
      ctx->alloc<Procedure>(p->formals, p->body, p->env, true)
    );
  }

  else {
    throw SchemeError(
      "define-macro: expected symbol or list"
    );
  }

  return Obj(Void{});
}

static EvalResult eval_set(Obj rest, Env *env, Ctx *ctx) {
  check_arity(rest, "set!", 2, 2);
  if (!rest.car().is_symbol()) {
    throw SchemeError(
      "set!: expected symbol, got "
      + rest.car().stringify_type()
    );
  }
  Symbol sym = rest.car().as_symbol();
  Obj val = eval(rest.cdr().car(), env, ctx);
  if (!env->set(sym, val)) {
    throw SchemeError(
      "set!: undefined variable "
      + sym.get_name()
    );
  }

  return Obj(Void{});
}

static EvalResult eval_lambda(Obj rest, Env *env, Ctx *ctx) {
  check_arity(rest, "lambda", 2, SIZE_MAX);
  Obj body = wrap_body(rest.cdr(), ctx);
  return Obj(ctx->alloc<Procedure>(
    Formals::parse(rest.car()), body, env, false
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
        throw SchemeError("letrec: binding name must be a symbol");
      }
      new_env->define(binding.car().as_symbol(), Void{});
    }
  }

  Env *init_env = kind == LetKind::Plain ? env : new_env;

  for (Obj binding : ListView{bindings}) {
    if (!binding.car().is_symbol()) {
      throw SchemeError(
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
      throw SchemeError("let: binding name must be a symbol");
    }
    params.push_back(binding.car().as_symbol());
    args.push_back(eval(binding.cdr().car(), env, ctx));
  }

  Env *loop_env = ctx->alloc<LocalEnv>(env);
  Procedure *proc = ctx->alloc<Procedure>(
    Formals{std::move(params), false}, body, loop_env, false
  );
  loop_env->define(name, proc);

  Env *call_env = ctx->alloc<LocalEnv>(loop_env);
  proc->formals.bind(call_env, args, ctx);
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

template<class Match>
static std::optional<EvalResult> eval_clauses(
  Obj clauses, std::string_view name, Match match
) {
  while (clauses.is_cons()) {
    Obj clause = clauses.car();
    if (!clause.is_cons()) {
      throw SchemeError(std::format("{}: clause must be a list", name));
    }

    bool is_else = is_keyword(clause.car(), "else");

    if (is_else && clauses.cdr().is_cons()) {
      throw SchemeError(std::format("{}: else must be the last clause", name));
    }

    if (auto result = match(clause, is_else)) {
      return result;
    }

    clauses = clauses.cdr();
  }

  return std::nullopt;
}

static std::optional<EvalResult> try_cond(Obj clauses, Env *env, Ctx *ctx) {
  return eval_clauses(clauses, "cond",
    [&](Obj clause, bool is_else) -> std::optional<EvalResult> {
      Obj body = clause.cdr();

      if (is_else && body.is_null()) {
        throw SchemeError("cond: else clause must have a body");
      }

      bool is_arrow =
        !is_else && body.is_cons() && is_keyword(body.car(), "=>");

      if (is_arrow && !(body.cdr().is_cons() && body.cdr().cdr().is_null())) {
        throw SchemeError("cond: expected one receiver after =>");
      }

      Obj test_val = is_else ? Obj(true) : eval(clause.car(), env, ctx);
      if (!is_else && test_val.is_false()) {
        return std::nullopt;
      }

      if (body.is_null()) {
        return EvalResult{test_val};
      }
      if (is_arrow) {
        Obj receiver = eval(body.cdr().car(), env, ctx);
        return apply_procedure(receiver, {test_val}, ctx);
      }
      return eval_body(body, env, ctx);
    });
}

static EvalResult eval_cond(Obj clauses, Env *env, Ctx *ctx) {
  return try_cond(clauses, env, ctx).value_or(EvalResult{Obj(Void{})});
}

static EvalResult eval_case(Obj rest, Env *env, Ctx *ctx) {
  check_arity(rest, "case", 1, SIZE_MAX);
  Obj key = eval(rest.car(), env, ctx);

  return eval_clauses(rest.cdr(), "case",
    [&](Obj clause, bool is_else) -> std::optional<EvalResult> {
      bool matched = is_else;
      for (Obj d = clause.car(); !matched && d.is_cons(); d = d.cdr()) {
        matched = key.equals(d.car());
      }
      if (!matched) {
        return std::nullopt;
      }

      Obj body = clause.cdr();
      if (body.is_null()) {
        return EvalResult{Obj(Void{})};
      }
      return eval_body(body, env, ctx);
    }).value_or(EvalResult{Obj(Void{})});
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

static EvalResult eval_guard(Obj rest, Env *env, Ctx *ctx) {
  check_arity(rest, "guard", 2, SIZE_MAX);
  Obj spec = rest.car();

  if (!spec.is_cons() || !spec.car().is_symbol()) {
    throw SchemeError("guard: expected (variable clause ...)");
  }

  std::optional<SchemeError> caught;
  try {
    return eval(wrap_body(rest.cdr(), ctx), env, ctx);
  }
  catch (SchemeError &e) {
    caught = std::move(e);
  }

  Env *handler_env = ctx->alloc<LocalEnv>(env);
  handler_env->define(
    spec.car().as_symbol(),
    caught->payload
      ? *caught->payload
      : Obj(ctx->alloc<Error>(caught->what(), Null{}))
  );

  if (auto handled = try_cond(spec.cdr(), handler_env, ctx)) {
    return *handled;
  }
  throw *caught;
}

static EvalResult eval_delay(Obj rest, Env *env, Ctx *ctx) {
  check_arity(rest, "delay", 1, 1);
  return Obj(ctx->alloc<Promise>(rest.car(), env));
}

static EvalResult eval_cons_stream(Obj rest, Env *env, Ctx *ctx) {
  check_arity(rest, "cons-stream", 2, 2);
  Obj head = eval(rest.car(), env, ctx);
  return Obj(ctx->alloc<Cons>(
    head,
    ctx->alloc<Promise>(rest.cdr().car(), env)
  ));
}

// --- dispatch ---

static EvalResult eval_quasiquote_form(Obj rest, Env *env, Ctx *ctx) {
  check_arity(rest, "quasiquote", 1, 1);
  return eval_quasiquote(rest.car(), env, ctx);
}

static EvalResult eval_apply(Obj head, Obj rest, Env *env, Ctx *ctx) {
  Obj proc = eval(head, env, ctx);
  return apply_procedure(proc, eval_args(rest, env, ctx), ctx);
}

static EvalResult eval_let_form(Obj rest, Env *env, Ctx *ctx) {
  if (rest.is_cons() && rest.car().is_symbol()) {
    return eval_named_let(rest, env, ctx);
  }
  return eval_let(rest, env, ctx, LetKind::Plain);
}

using SpecialForm = EvalResult (*)(Obj rest, Env *env, Ctx *ctx);

static const std::unordered_map<std::string_view, SpecialForm> special_forms = {
  {"quote",        [](Obj r, Env *, Ctx *) { return eval_quote(r); }},
  {"if",           eval_if},
  {"define",       eval_define},
  {"set!",         eval_set},
  {"lambda",       eval_lambda},
  {"begin",        eval_begin},
  {"let",          eval_let_form},
  {"let*",         [](Obj r, Env *e, Ctx *c) { return eval_let(r, e, c, LetKind::Star); }},
  {"letrec",       [](Obj r, Env *e, Ctx *c) { return eval_let(r, e, c, LetKind::Rec); }},
  {"when",         [](Obj r, Env *e, Ctx *c) { return eval_when(r, e, c, false); }},
  {"unless",       [](Obj r, Env *e, Ctx *c) { return eval_when(r, e, c, true); }},
  {"case",         eval_case},
  {"cond",         eval_cond},
  {"and",          [](Obj r, Env *e, Ctx *c) { return eval_and_or(r, e, c, true); }},
  {"or",           [](Obj r, Env *e, Ctx *c) { return eval_and_or(r, e, c, false); }},
  {"quasiquote",   eval_quasiquote_form},
  {"guard",        eval_guard},
  {"delay",        eval_delay},
  {"cons-stream",  eval_cons_stream},
  {"define-macro", eval_define_macro},
};

static EvalResult eval_expr(Obj expr, Env *env, Ctx *ctx) {
  if (!expr.is_symbol() && !expr.is_cons()) {
    return expr;
  }

  else if (expr.is_symbol()) {
    auto result = env->lookup(expr.as_symbol());

    if (!result) {
      throw SchemeError(
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

      if (auto it = special_forms.find(sym.get_name()); it != special_forms.end()) {
        return it->second(rest, env, ctx);
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
        p->formals.bind(macro_env, raw_args, ctx);
        Obj expanded = eval(p->body, macro_env, ctx);
        return TailCall{expanded, env};
      }
    }

    return eval_apply(head, rest, env, ctx);
  }
}

struct EvalFrame {
  Ctx *ctx;
  EvalFrame(Ctx *ctx): ctx {ctx} {
    if (!ctx->push_eval()) {
      throw SchemeError("recursion too deep");
    }
  }
  ~EvalFrame() { ctx->pop_eval(); }
};

Obj eval(Obj expr, Env *env, Ctx *ctx) {
  EvalFrame frame {ctx};
  EvalResult result = eval_expr(expr, env, ctx);
  while (auto *tc = std::get_if<TailCall>(&result)) {
    result = eval_expr(tc->expr, tc->env, ctx);
  }
  return std::get<Obj>(result);
}
