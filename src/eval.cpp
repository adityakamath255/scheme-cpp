#include "eval.hpp"

#include <format>
#include <ranges>
#include <string>
#include <unordered_map>

static constexpr size_t max_eval_depth = 1000;

EvalContext::EvalContext(scheme::SessionState &state,
                         const scheme::Emit &emit)
    : state{state}, emit_event{emit}, depth{0} {}

Symbol EvalContext::intern(std::string_view name) {
  auto [it, _] = state.interned.insert(std::string(name));
  return Symbol{*it};
}

void EvalContext::install_builtin(std::string_view name,
                                  Builtin::Implementation implementation) {
  state.global_env.define(
      intern(name), alloc<Builtin>(std::move(implementation)));
}

void EvalContext::output(std::string_view text) const {
  if (emit_event) {
    emit_event(scheme::Output{std::string(text)});
  }
}

void EvalContext::result(std::string text) const {
  if (emit_event) {
    emit_event(scheme::Result{std::move(text)});
  }
}

void EvalContext::collect_if_needed() {
  if (depth == 0 && state.should_collect()) {
    state.collect();
  }
}

EvalContext::Frame::Frame(EvalContext &context) : context{context} {
  if (context.depth >= max_eval_depth) {
    throw SchemeError("recursion too deep");
  }
  context.depth += 1;
}

EvalContext::Frame::~Frame() { context.depth -= 1; }

struct TailCall {
  Obj expr;
  std::reference_wrapper<Env> env;
};

using EvalResult = std::variant<Obj, TailCall>;

void check_arity(size_t count, std::string_view name, size_t min, size_t max) {
  if (count < min || count > max) {
    throw SchemeError(std::format(
        "{}: expected {} arguments, got {}", name,
        (min == max ? std::to_string(min) : std::format("{}-{}", min, max)),
        count));
  }
}

static void check_arity(Obj rest, std::string_view name, size_t min,
                        size_t max) {
  auto profile = rest.list_profile();

  if (!profile.is_proper) {
    throw SchemeError(std::format("{}: improper argument list", name));
  }

  check_arity(profile.size, name, min, max);
}

static std::vector<Obj> eval_args(Obj list, Env &env, EvalContext &context) {
  return ListView{list} | std::views::transform([&](Obj arg) {
           return context.eval(arg, env);
         }) |
         std::ranges::to<std::vector>();
}

Formals Formals::parse(Obj formals) {
  if (formals.is_symbol()) {
    return {{}, formals.as_symbol()};
  } else {
    ListView params{formals};
    std::vector<Symbol> fixed;

    for (Obj p : params) {
      if (!p.is_symbol()) {
        throw SchemeError("parameter must be a symbol");
      }
      fixed.push_back(p.as_symbol());
    }

    Obj tail = params.tail();

    if (tail.is_null()) {
      return {std::move(fixed), std::nullopt};
    } else if (tail.is_symbol()) {
      return {std::move(fixed), tail.as_symbol()};
    } else {
      throw SchemeError("invalid parameter list");
    }
  }
}

void Formals::bind(Env &env, const std::vector<Obj> &args,
                   EvalContext &context) const {
  if (args.size() < fixed.size()) {
    throw SchemeError("too few arguments");
  }
  if (!rest && args.size() > fixed.size()) {
    throw SchemeError("too many arguments");
  }

  for (size_t i = 0; i < fixed.size(); i += 1) {
    env.define(fixed[i], args[i]);
  }
  if (rest) {
    env.define(*rest,
                list_from(args | std::views::drop(fixed.size()), context));
  }
}

static bool is_keyword(Obj obj, std::string_view name) {
  return obj.is_symbol() && obj.as_symbol().name() == name;
}

static Obj wrap_body(Obj body_list, EvalContext &context) {
  if (body_list.cdr().is_null()) {
    return body_list.car();
  }
  return context.alloc<Cons>(context.intern("begin"), body_list);
}

static EvalResult eval_body(Obj list, Env &env, EvalContext &context) {
  while (list.cdr().is_cons()) {
    context.eval(list.car(), env);
    list = list.cdr();
  }
  return TailCall{list.car(), env};
}

static std::pair<Obj, std::vector<Obj>>
splice_apply(const std::vector<Obj> &args) {
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

static EvalResult apply_procedure(Obj proc, std::vector<Obj> args,
                                  EvalContext &context) {
  while (true) {
    if (proc.is_procedure()) {
      Procedure *p = proc.as_procedure();
      Env &new_env = *context.alloc<Env>(&p->env.get());
      p->formals.bind(new_env, args, context);
      return TailCall{p->body, new_env};
    }

    if (proc.is_builtin()) {
      Builtin *b = proc.as_builtin();
      if (auto *fn = std::get_if<Builtin::Fn>(&b->implementation)) {
        return (*fn)(args, context);
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

static Obj eval_quasiquote(Obj obj, Env &env, EvalContext &context) {
  if (obj.is_vector()) {
    auto vec = obj.as_vector();
    std::vector<Obj> elements;
    for (auto elem : vec->data) {
      elements.push_back(eval_quasiquote(elem, env, context));
    }
    return context.alloc<Vector>(std::move(elements));
  }

  else if (!obj.is_cons()) {
    return obj;
  }

  else if (is_keyword(obj.car(), "unquote")) {
    return context.eval(obj.cdr().car(), env);
  }

  else {
    ListView items{obj};
    std::vector<Obj> elements;

    for (Obj elem : items) {
      if (elem.is_cons() && is_keyword(elem.car(), "unquote-splicing")) {
        elements.append_range(ListView{context.eval(elem.cdr().car(), env)});
      } else {
        elements.push_back(eval_quasiquote(elem, env, context));
      }
    }

    Obj raw_tail = items.tail();
    Obj tail = raw_tail.is_null() ? Obj(Null{})
                                  : eval_quasiquote(raw_tail, env, context);
    return list_from(elements, context, tail);
  }
}

static EvalResult eval_quote(Obj rest) {
  check_arity(rest, "quote", 1, 1);
  return rest.car();
}

static EvalResult eval_if(Obj rest, Env &env, EvalContext &context) {
  check_arity(rest, "if", 2, 3);
  Obj pred = context.eval(rest.car(), env);

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

static EvalResult eval_define(Obj rest, Env &env, EvalContext &context) {
  check_arity(rest, "define", 1, SIZE_MAX);
  Obj target = rest.car();

  if (target.is_cons()) {
    Symbol fname = target.car().as_symbol();
    Obj body = wrap_body(rest.cdr(), context);

    env.define(fname,
                context.alloc<Procedure>(Formals::parse(target.cdr()), body,
                                            env, ProcedureKind::Function));

    return Obj(Void{});
  }

  else if (target.is_symbol()) {
    Symbol sym = target.as_symbol();

    if (rest.cdr().is_null()) {
      env.define(sym, Void{});
    } else {
      check_arity(rest, "define", 2, 2);
      env.define(sym, context.eval(rest.cdr().car(), env));
    }
    return Obj(Void{});
  }

  else {
    throw SchemeError("define: expected symbol or list, got " +
                      target.type_name());
  }
}

static EvalResult eval_define_macro(Obj rest, Env &env, EvalContext &context) {
  check_arity(rest, "define-macro", 2, SIZE_MAX);
  Obj target = rest.car();

  if (target.is_cons()) {
    Symbol name = target.car().as_symbol();
    Obj body = wrap_body(rest.cdr(), context);

    env.define(name,
                context.alloc<Procedure>(Formals::parse(target.cdr()), body,
                                            env, ProcedureKind::Macro));
  }

  else if (target.is_symbol()) {
    check_arity(rest, "define-macro", 2, 2);
    Obj val = context.eval(rest.cdr().car(), env);

    if (!val.is_procedure()) {
      throw SchemeError("define-macro: expected procedure");
    }

    Procedure *p = val.as_procedure();

    env.define(target.as_symbol(),
                context.alloc<Procedure>(p->formals, p->body, p->env.get(),
                                            ProcedureKind::Macro));
  }

  else {
    throw SchemeError("define-macro: expected symbol or list");
  }

  return Obj(Void{});
}

static EvalResult eval_set(Obj rest, Env &env, EvalContext &context) {
  check_arity(rest, "set!", 2, 2);
  if (!rest.car().is_symbol()) {
    throw SchemeError("set!: expected symbol, got " +
                      rest.car().type_name());
  }
  Symbol sym = rest.car().as_symbol();
  Obj val = context.eval(rest.cdr().car(), env);
  if (!env.set(sym, val)) {
    throw SchemeError("set!: undefined variable " + sym.name());
  }

  return Obj(Void{});
}

static EvalResult eval_lambda(Obj rest, Env &env, EvalContext &context) {
  check_arity(rest, "lambda", 2, SIZE_MAX);
  Obj body = wrap_body(rest.cdr(), context);
  return Obj(context.alloc<Procedure>(Formals::parse(rest.car()), body, env,
                                         ProcedureKind::Function));
}

static EvalResult eval_begin(Obj rest, Env &env, EvalContext &context) {
  if (rest.is_null()) {
    return Obj(Void{});
  } else {
    return eval_body(rest, env, context);
  }
}

enum class LetKind { Plain, Star, Rec };

static EvalResult eval_let(Obj rest, Env &env, EvalContext &context,
                           LetKind kind) {
  const char *name = kind == LetKind::Star  ? "let*"
                     : kind == LetKind::Rec ? "letrec"
                                            : "let";
  check_arity(rest, name, 2, SIZE_MAX);
  Obj bindings = rest.car();
  Obj body_list = rest.cdr();
  Env &new_env = *context.alloc<Env>(&env);

  if (kind == LetKind::Rec) {
    for (Obj binding : ListView{bindings}) {
      if (!binding.car().is_symbol()) {
        throw SchemeError("letrec: binding name must be a symbol");
      }
      new_env.define(binding.car().as_symbol(), Void{});
    }
  }

  Env &init_env = kind == LetKind::Plain ? env : new_env;

  for (Obj binding : ListView{bindings}) {
    if (!binding.car().is_symbol()) {
      throw SchemeError(std::string(name) + ": binding name must be a symbol");
    }
    Symbol sym = binding.car().as_symbol();
    Obj val = context.eval(binding.cdr().car(), init_env);
    if (kind == LetKind::Rec) {
      new_env.set(sym, val);
    } else {
      new_env.define(sym, val);
    }
  }

  return eval_body(body_list, new_env, context);
}

static EvalResult eval_named_let(Obj rest, Env &env, EvalContext &context) {
  Symbol name = rest.car().as_symbol();
  Obj spec = rest.cdr();
  check_arity(spec, "let", 2, SIZE_MAX);
  Obj bindings = spec.car();
  Obj body = wrap_body(spec.cdr(), context);

  std::vector<Symbol> params;
  std::vector<Obj> args;
  for (Obj binding : ListView{bindings}) {
    if (!binding.car().is_symbol()) {
      throw SchemeError("let: binding name must be a symbol");
    }
    params.push_back(binding.car().as_symbol());
    args.push_back(context.eval(binding.cdr().car(), env));
  }

  Env &loop_env = *context.alloc<Env>(&env);
  Procedure *proc =
      context.alloc<Procedure>(Formals{std::move(params), std::nullopt},
                                  body, loop_env, ProcedureKind::Function);
  loop_env.define(name, proc);

  Env &call_env = *context.alloc<Env>(&loop_env);
  proc->formals.bind(call_env, args, context);
  return TailCall{proc->body, call_env};
}

enum class ConditionalKind {
  When,
  Unless,
};

static EvalResult eval_conditional(Obj rest, Env &env, EvalContext &context,
                                   ConditionalKind kind) {
  bool is_unless = kind == ConditionalKind::Unless;
  check_arity(rest, is_unless ? "unless" : "when", 1, SIZE_MAX);
  Obj test = context.eval(rest.car(), env);
  bool go = is_unless ? test.is_false() : test.is_true();
  Obj body = rest.cdr();

  if (!go || body.is_null()) {
    return Obj(Void{});
  }
  return eval_body(body, env, context);
}

template <class Match>
static std::optional<EvalResult>
eval_clauses(Obj clauses, std::string_view name, Match match) {
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

static std::optional<EvalResult> try_cond(Obj clauses, Env &env,
                                          EvalContext &context) {
  return eval_clauses(
      clauses, "cond",
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

        Obj test_val = is_else ? Obj(true) : context.eval(clause.car(), env);
        if (!is_else && test_val.is_false()) {
          return std::nullopt;
        }

        if (body.is_null()) {
          return EvalResult{test_val};
        }
        if (is_arrow) {
          Obj receiver = context.eval(body.cdr().car(), env);
          return apply_procedure(receiver, {test_val}, context);
        }
        return eval_body(body, env, context);
      });
}

static EvalResult eval_cond(Obj clauses, Env &env, EvalContext &context) {
  return try_cond(clauses, env, context).value_or(EvalResult{Obj(Void{})});
}

static EvalResult eval_case(Obj rest, Env &env, EvalContext &context) {
  check_arity(rest, "case", 1, SIZE_MAX);
  Obj key = context.eval(rest.car(), env);

  return eval_clauses(
             rest.cdr(), "case",
             [&](Obj clause, bool is_else) -> std::optional<EvalResult> {
               bool matched = is_else;
               for (Obj d = clause.car(); !matched && d.is_cons();
                    d = d.cdr()) {
                 matched = key.equals(d.car());
               }
               if (!matched) {
                 return std::nullopt;
               }

               Obj body = clause.cdr();
               if (body.is_null()) {
                 return EvalResult{Obj(Void{})};
               }
               return eval_body(body, env, context);
             })
      .value_or(EvalResult{Obj(Void{})});
}

enum class LogicalKind {
  And,
  Or,
};

static EvalResult eval_logical(Obj rest, Env &env, EvalContext &context,
                               LogicalKind kind) {
  bool conjunction = kind == LogicalKind::And;
  if (rest.is_null()) {
    return Obj(conjunction);
  }
  while (rest.cdr().is_cons()) {
    Obj val = context.eval(rest.car(), env);
    if (val.is_true() != conjunction) {
      return val;
    }
    rest = rest.cdr();
  }
  return TailCall{rest.car(), env};
}

static EvalResult eval_guard(Obj rest, Env &env, EvalContext &context) {
  check_arity(rest, "guard", 2, SIZE_MAX);
  Obj spec = rest.car();

  if (!spec.is_cons() || !spec.car().is_symbol()) {
    throw SchemeError("guard: expected (variable clause ...)");
  }

  std::optional<SchemeError> caught;
  try {
    return context.eval(wrap_body(rest.cdr(), context), env);
  } catch (SchemeError &e) {
    caught = std::move(e);
  }

  Env &handler_env = *context.alloc<Env>(&env);
  handler_env.define(spec.car().as_symbol(), caught->as_condition(context));

  if (auto handled = try_cond(spec.cdr(), handler_env, context)) {
    return *handled;
  }
  throw *caught;
}

static EvalResult eval_delay(Obj rest, Env &env, EvalContext &context) {
  check_arity(rest, "delay", 1, 1);
  return Obj(context.alloc<Promise>(rest.car(), env));
}

static EvalResult eval_cons_stream(Obj rest, Env &env, EvalContext &context) {
  check_arity(rest, "cons-stream", 2, 2);
  Obj head = context.eval(rest.car(), env);
  return Obj(context.alloc<Cons>(
      head, context.alloc<Promise>(rest.cdr().car(), env)));
}

static EvalResult eval_quasiquote_form(Obj rest, Env &env,
                                       EvalContext &context) {
  check_arity(rest, "quasiquote", 1, 1);
  return eval_quasiquote(rest.car(), env, context);
}

static EvalResult eval_apply(Obj head, Obj rest, Env &env,
                             EvalContext &context) {
  Obj proc = context.eval(head, env);
  return apply_procedure(proc, eval_args(rest, env, context), context);
}

static EvalResult eval_let_form(Obj rest, Env &env, EvalContext &context) {
  if (rest.is_cons() && rest.car().is_symbol()) {
    return eval_named_let(rest, env, context);
  }
  return eval_let(rest, env, context, LetKind::Plain);
}

using SpecialForm = EvalResult (*)(Obj rest, Env &env, EvalContext &context);

static const std::unordered_map<std::string_view, SpecialForm> special_forms = {
    {"quote", [](Obj r, Env &, EvalContext &) { return eval_quote(r); }},
    {"if", eval_if},
    {"define", eval_define},
    {"set!", eval_set},
    {"lambda", eval_lambda},
    {"begin", eval_begin},
    {"let", eval_let_form},
    {"let*", [](Obj r, Env &e,
                EvalContext &c) { return eval_let(r, e, c, LetKind::Star); }},
    {"letrec", [](Obj r, Env &e,
                  EvalContext &c) { return eval_let(r, e, c, LetKind::Rec); }},
    {"when",
     [](Obj r, Env &e, EvalContext &c) {
       return eval_conditional(r, e, c, ConditionalKind::When);
     }},
    {"unless",
     [](Obj r, Env &e, EvalContext &c) {
       return eval_conditional(r, e, c, ConditionalKind::Unless);
     }},
    {"case", eval_case},
    {"cond", eval_cond},
    {"and",
     [](Obj r, Env &e, EvalContext &c) {
       return eval_logical(r, e, c, LogicalKind::And);
     }},
    {"or", [](Obj r, Env &e,
              EvalContext &c) { return eval_logical(r, e, c, LogicalKind::Or); }},
    {"quasiquote", eval_quasiquote_form},
    {"guard", eval_guard},
    {"delay", eval_delay},
    {"cons-stream", eval_cons_stream},
    {"define-macro", eval_define_macro},
};

static EvalResult eval_expr(Obj expr, Env &env, EvalContext &context) {
  if (!expr.is_symbol() && !expr.is_cons()) {
    return expr;
  }

  else if (expr.is_symbol()) {
    auto result = env.lookup(expr.as_symbol());

    if (!result) {
      throw SchemeError("undefined variable: " + expr.as_symbol().name());
    }

    return *result;
  }

  else {
    Obj head = expr.car();
    Obj rest = expr.cdr();

    if (head.is_symbol()) {
      Symbol sym = head.as_symbol();

      if (auto it = special_forms.find(sym.name());
          it != special_forms.end()) {
        return it->second(rest, env, context);
      }

      auto macro_val = env.lookup(sym);

      if (macro_val && macro_val->is_procedure() &&
          macro_val->as_procedure()->kind == ProcedureKind::Macro) {
        Procedure *p = macro_val->as_procedure();
        std::vector<Obj> raw_args =
            std::ranges::to<std::vector>(ListView{rest});
        Env &macro_env = *context.alloc<Env>(&p->env.get());
        p->formals.bind(macro_env, raw_args, context);
        Obj expanded = context.eval(p->body, macro_env);
        return TailCall{expanded, env};
      }
    }

    return eval_apply(head, rest, env, context);
  }
}

Obj EvalContext::eval(Obj expr, Env &env) {
  Frame frame{*this};
  EvalResult result = eval_expr(expr, env, *this);
  while (auto *tc = std::get_if<TailCall>(&result)) {
    result = eval_expr(tc->expr, tc->env.get(), *this);
  }
  return std::get<Obj>(result);
}

Obj EvalContext::eval_global(Obj expression) {
  return eval(expression, state.global_env);
}
