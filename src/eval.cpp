#include "eval.hpp"
#include "runtime.hpp"

#include <format>
#include <ranges>
#include <unordered_map>

static constexpr size_t max_eval_depth = 1000;

Evaluator::Evaluator(Runtime &runtime, const scheme::Emit &emit)
    : state{runtime}, emit_event{emit}, depth{0} {}

Runtime &Evaluator::runtime() { return state; }

Env *Evaluator::global_env() const { return state.global_env; }

Symbol Evaluator::intern(std::string_view name) { return state.intern(name); }

void Evaluator::output(std::string_view text) const {
  if (emit_event) {
    emit_event(scheme::Output{std::string(text)});
  }
}

void Evaluator::result(std::string text) const {
  if (emit_event) {
    emit_event(scheme::Result{std::move(text)});
  }
}

void Evaluator::recycle_if_needed() {
  if (depth == 0 && state.should_recycle()) {
    state.recycle();
  }
}

bool Evaluator::push() {
  if (depth >= max_eval_depth) {
    return false;
  }
  depth += 1;
  return true;
}

void Evaluator::pop() { depth -= 1; }

struct TailCall {
  Obj expr;
  Env *env;
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
  auto profile = rest.get_list_profile();

  if (!profile.is_proper) {
    throw SchemeError(std::format("{}: improper argument list", name));
  }

  check_arity(profile.size, name, min, max);
}

static std::vector<Obj> eval_args(Obj list, Env *env, Evaluator *evaluator) {
  return ListView{list} | std::views::transform([&](Obj arg) {
           return evaluator->eval(arg, env);
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

void Formals::bind(Env *env, const std::vector<Obj> &args,
                   Evaluator *evaluator) const {
  if (args.size() < fixed.size()) {
    throw SchemeError("too few arguments");
  }
  if (!rest && args.size() > fixed.size()) {
    throw SchemeError("too many arguments");
  }

  for (size_t i = 0; i < fixed.size(); i += 1) {
    env->define(fixed[i], args[i]);
  }
  if (rest) {
    env->define(*rest,
                list_from(args | std::views::drop(fixed.size()), evaluator));
  }
}

static bool is_keyword(Obj obj, std::string_view name) {
  return obj.is_symbol() && obj.as_symbol().get_name() == name;
}

static Obj wrap_body(Obj body_list, Evaluator *evaluator) {
  if (body_list.cdr().is_null()) {
    return body_list.car();
  }
  return evaluator->alloc<Cons>(evaluator->intern("begin"), body_list);
}

static EvalResult eval_body(Obj list, Env *env, Evaluator *evaluator) {
  while (list.cdr().is_cons()) {
    evaluator->eval(list.car(), env);
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
                                  Evaluator *evaluator) {
  while (true) {
    if (proc.is_procedure()) {
      Procedure *p = proc.as_procedure();
      Env *new_env = evaluator->alloc<LocalEnv>(p->env);
      p->formals.bind(new_env, args, evaluator);
      return TailCall{p->body, new_env};
    }

    if (proc.is_builtin()) {
      Builtin *b = proc.as_builtin();
      if (auto *fn = std::get_if<Builtin::Fn>(&b->implementation)) {
        return (*fn)(args, evaluator);
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

static Obj eval_quasiquote(Obj obj, Env *env, Evaluator *evaluator) {
  if (obj.is_vector()) {
    auto vec = obj.as_vector();
    std::vector<Obj> elements;
    for (auto elem : vec->data) {
      elements.push_back(eval_quasiquote(elem, env, evaluator));
    }
    return evaluator->alloc<Vector>(std::move(elements));
  }

  else if (!obj.is_cons()) {
    return obj;
  }

  else if (is_keyword(obj.car(), "unquote")) {
    return evaluator->eval(obj.cdr().car(), env);
  }

  else {
    ListView items{obj};
    std::vector<Obj> elements;

    for (Obj elem : items) {
      if (elem.is_cons() && is_keyword(elem.car(), "unquote-splicing")) {
        elements.append_range(ListView{evaluator->eval(elem.cdr().car(), env)});
      } else {
        elements.push_back(eval_quasiquote(elem, env, evaluator));
      }
    }

    Obj raw_tail = items.tail();
    Obj tail = raw_tail.is_null() ? Obj(Null{})
                                  : eval_quasiquote(raw_tail, env, evaluator);
    return list_from(elements, evaluator, tail);
  }
}

static EvalResult eval_quote(Obj rest) {
  check_arity(rest, "quote", 1, 1);
  return rest.car();
}

static EvalResult eval_if(Obj rest, Env *env, Evaluator *evaluator) {
  check_arity(rest, "if", 2, 3);
  Obj pred = evaluator->eval(rest.car(), env);

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

static EvalResult eval_define(Obj rest, Env *env, Evaluator *evaluator) {
  check_arity(rest, "define", 1, SIZE_MAX);
  Obj target = rest.car();

  if (target.is_cons()) {
    Symbol fname = target.car().as_symbol();
    Obj body = wrap_body(rest.cdr(), evaluator);

    env->define(fname,
                evaluator->alloc<Procedure>(Formals::parse(target.cdr()), body,
                                            env, ProcedureKind::Function));

    return Obj(Void{});
  }

  else if (target.is_symbol()) {
    Symbol sym = target.as_symbol();

    if (rest.cdr().is_null()) {
      env->define(sym, Void{});
    } else {
      check_arity(rest, "define", 2, 2);
      env->define(sym, evaluator->eval(rest.cdr().car(), env));
    }
    return Obj(Void{});
  }

  else {
    throw SchemeError("define: expected symbol or list, got " +
                      target.stringify_type());
  }
}

static EvalResult eval_define_macro(Obj rest, Env *env, Evaluator *evaluator) {
  check_arity(rest, "define-macro", 2, SIZE_MAX);
  Obj target = rest.car();

  if (target.is_cons()) {
    Symbol name = target.car().as_symbol();
    Obj body = wrap_body(rest.cdr(), evaluator);

    env->define(name,
                evaluator->alloc<Procedure>(Formals::parse(target.cdr()), body,
                                            env, ProcedureKind::Macro));
  }

  else if (target.is_symbol()) {
    check_arity(rest, "define-macro", 2, 2);
    Obj val = evaluator->eval(rest.cdr().car(), env);

    if (!val.is_procedure()) {
      throw SchemeError("define-macro: expected procedure");
    }

    Procedure *p = val.as_procedure();

    env->define(target.as_symbol(),
                evaluator->alloc<Procedure>(p->formals, p->body, p->env,
                                            ProcedureKind::Macro));
  }

  else {
    throw SchemeError("define-macro: expected symbol or list");
  }

  return Obj(Void{});
}

static EvalResult eval_set(Obj rest, Env *env, Evaluator *evaluator) {
  check_arity(rest, "set!", 2, 2);
  if (!rest.car().is_symbol()) {
    throw SchemeError("set!: expected symbol, got " +
                      rest.car().stringify_type());
  }
  Symbol sym = rest.car().as_symbol();
  Obj val = evaluator->eval(rest.cdr().car(), env);
  if (!env->set(sym, val)) {
    throw SchemeError("set!: undefined variable " + sym.get_name());
  }

  return Obj(Void{});
}

static EvalResult eval_lambda(Obj rest, Env *env, Evaluator *evaluator) {
  check_arity(rest, "lambda", 2, SIZE_MAX);
  Obj body = wrap_body(rest.cdr(), evaluator);
  return Obj(evaluator->alloc<Procedure>(Formals::parse(rest.car()), body, env,
                                         ProcedureKind::Function));
}

static EvalResult eval_begin(Obj rest, Env *env, Evaluator *evaluator) {
  if (rest.is_null()) {
    return Obj(Void{});
  } else {
    return eval_body(rest, env, evaluator);
  }
}

enum class LetKind { Plain, Star, Rec };

static EvalResult eval_let(Obj rest, Env *env, Evaluator *evaluator,
                           LetKind kind) {
  const char *name = kind == LetKind::Star  ? "let*"
                     : kind == LetKind::Rec ? "letrec"
                                            : "let";
  check_arity(rest, name, 2, SIZE_MAX);
  Obj bindings = rest.car();
  Obj body_list = rest.cdr();
  Env *new_env = evaluator->alloc<LocalEnv>(env);

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
      throw SchemeError(std::string(name) + ": binding name must be a symbol");
    }
    Symbol sym = binding.car().as_symbol();
    Obj val = evaluator->eval(binding.cdr().car(), init_env);
    if (kind == LetKind::Rec) {
      new_env->set(sym, val);
    } else {
      new_env->define(sym, val);
    }
  }

  return eval_body(body_list, new_env, evaluator);
}

static EvalResult eval_named_let(Obj rest, Env *env, Evaluator *evaluator) {
  Symbol name = rest.car().as_symbol();
  Obj spec = rest.cdr();
  check_arity(spec, "let", 2, SIZE_MAX);
  Obj bindings = spec.car();
  Obj body = wrap_body(spec.cdr(), evaluator);

  std::vector<Symbol> params;
  std::vector<Obj> args;
  for (Obj binding : ListView{bindings}) {
    if (!binding.car().is_symbol()) {
      throw SchemeError("let: binding name must be a symbol");
    }
    params.push_back(binding.car().as_symbol());
    args.push_back(evaluator->eval(binding.cdr().car(), env));
  }

  Env *loop_env = evaluator->alloc<LocalEnv>(env);
  Procedure *proc =
      evaluator->alloc<Procedure>(Formals{std::move(params), std::nullopt},
                                  body, loop_env, ProcedureKind::Function);
  loop_env->define(name, proc);

  Env *call_env = evaluator->alloc<LocalEnv>(loop_env);
  proc->formals.bind(call_env, args, evaluator);
  return TailCall{proc->body, call_env};
}

enum class ConditionalKind {
  When,
  Unless,
};

static EvalResult eval_conditional(Obj rest, Env *env, Evaluator *evaluator,
                                   ConditionalKind kind) {
  bool is_unless = kind == ConditionalKind::Unless;
  check_arity(rest, is_unless ? "unless" : "when", 1, SIZE_MAX);
  Obj test = evaluator->eval(rest.car(), env);
  bool go = is_unless ? test.is_false() : test.is_true();
  Obj body = rest.cdr();

  if (!go || body.is_null()) {
    return Obj(Void{});
  }
  return eval_body(body, env, evaluator);
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

static std::optional<EvalResult> try_cond(Obj clauses, Env *env,
                                          Evaluator *evaluator) {
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

        Obj test_val = is_else ? Obj(true) : evaluator->eval(clause.car(), env);
        if (!is_else && test_val.is_false()) {
          return std::nullopt;
        }

        if (body.is_null()) {
          return EvalResult{test_val};
        }
        if (is_arrow) {
          Obj receiver = evaluator->eval(body.cdr().car(), env);
          return apply_procedure(receiver, {test_val}, evaluator);
        }
        return eval_body(body, env, evaluator);
      });
}

static EvalResult eval_cond(Obj clauses, Env *env, Evaluator *evaluator) {
  return try_cond(clauses, env, evaluator).value_or(EvalResult{Obj(Void{})});
}

static EvalResult eval_case(Obj rest, Env *env, Evaluator *evaluator) {
  check_arity(rest, "case", 1, SIZE_MAX);
  Obj key = evaluator->eval(rest.car(), env);

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
               return eval_body(body, env, evaluator);
             })
      .value_or(EvalResult{Obj(Void{})});
}

enum class LogicalKind {
  And,
  Or,
};

static EvalResult eval_logical(Obj rest, Env *env, Evaluator *evaluator,
                               LogicalKind kind) {
  bool conjunction = kind == LogicalKind::And;
  if (rest.is_null()) {
    return Obj(conjunction);
  }
  while (rest.cdr().is_cons()) {
    Obj val = evaluator->eval(rest.car(), env);
    if (val.is_true() != conjunction) {
      return val;
    }
    rest = rest.cdr();
  }
  return TailCall{rest.car(), env};
}

static EvalResult eval_guard(Obj rest, Env *env, Evaluator *evaluator) {
  check_arity(rest, "guard", 2, SIZE_MAX);
  Obj spec = rest.car();

  if (!spec.is_cons() || !spec.car().is_symbol()) {
    throw SchemeError("guard: expected (variable clause ...)");
  }

  std::optional<SchemeError> caught;
  try {
    return evaluator->eval(wrap_body(rest.cdr(), evaluator), env);
  } catch (SchemeError &e) {
    caught = std::move(e);
  }

  Env *handler_env = evaluator->alloc<LocalEnv>(env);
  handler_env->define(spec.car().as_symbol(), caught->as_condition(evaluator));

  if (auto handled = try_cond(spec.cdr(), handler_env, evaluator)) {
    return *handled;
  }
  throw *caught;
}

static EvalResult eval_delay(Obj rest, Env *env, Evaluator *evaluator) {
  check_arity(rest, "delay", 1, 1);
  return Obj(evaluator->alloc<Promise>(rest.car(), env));
}

static EvalResult eval_cons_stream(Obj rest, Env *env, Evaluator *evaluator) {
  check_arity(rest, "cons-stream", 2, 2);
  Obj head = evaluator->eval(rest.car(), env);
  return Obj(evaluator->alloc<Cons>(
      head, evaluator->alloc<Promise>(rest.cdr().car(), env)));
}

static EvalResult eval_quasiquote_form(Obj rest, Env *env,
                                       Evaluator *evaluator) {
  check_arity(rest, "quasiquote", 1, 1);
  return eval_quasiquote(rest.car(), env, evaluator);
}

static EvalResult eval_apply(Obj head, Obj rest, Env *env,
                             Evaluator *evaluator) {
  Obj proc = evaluator->eval(head, env);
  return apply_procedure(proc, eval_args(rest, env, evaluator), evaluator);
}

static EvalResult eval_let_form(Obj rest, Env *env, Evaluator *evaluator) {
  if (rest.is_cons() && rest.car().is_symbol()) {
    return eval_named_let(rest, env, evaluator);
  }
  return eval_let(rest, env, evaluator, LetKind::Plain);
}

using SpecialForm = EvalResult (*)(Obj rest, Env *env, Evaluator *evaluator);

static const std::unordered_map<std::string_view, SpecialForm> special_forms = {
    {"quote", [](Obj r, Env *, Evaluator *) { return eval_quote(r); }},
    {"if", eval_if},
    {"define", eval_define},
    {"set!", eval_set},
    {"lambda", eval_lambda},
    {"begin", eval_begin},
    {"let", eval_let_form},
    {"let*", [](Obj r, Env *e,
                Evaluator *c) { return eval_let(r, e, c, LetKind::Star); }},
    {"letrec", [](Obj r, Env *e,
                  Evaluator *c) { return eval_let(r, e, c, LetKind::Rec); }},
    {"when",
     [](Obj r, Env *e, Evaluator *c) {
       return eval_conditional(r, e, c, ConditionalKind::When);
     }},
    {"unless",
     [](Obj r, Env *e, Evaluator *c) {
       return eval_conditional(r, e, c, ConditionalKind::Unless);
     }},
    {"case", eval_case},
    {"cond", eval_cond},
    {"and",
     [](Obj r, Env *e, Evaluator *c) {
       return eval_logical(r, e, c, LogicalKind::And);
     }},
    {"or", [](Obj r, Env *e,
              Evaluator *c) { return eval_logical(r, e, c, LogicalKind::Or); }},
    {"quasiquote", eval_quasiquote_form},
    {"guard", eval_guard},
    {"delay", eval_delay},
    {"cons-stream", eval_cons_stream},
    {"define-macro", eval_define_macro},
};

static EvalResult eval_expr(Obj expr, Env *env, Evaluator *evaluator) {
  if (!expr.is_symbol() && !expr.is_cons()) {
    return expr;
  }

  else if (expr.is_symbol()) {
    auto result = env->lookup(expr.as_symbol());

    if (!result) {
      throw SchemeError("undefined variable: " + expr.as_symbol().get_name());
    }

    return *result;
  }

  else {
    Obj head = expr.car();
    Obj rest = expr.cdr();

    if (head.is_symbol()) {
      Symbol sym = head.as_symbol();

      if (auto it = special_forms.find(sym.get_name());
          it != special_forms.end()) {
        return it->second(rest, env, evaluator);
      }

      auto macro_val = env->lookup(sym);

      if (macro_val && macro_val->is_procedure() &&
          macro_val->as_procedure()->kind == ProcedureKind::Macro) {
        Procedure *p = macro_val->as_procedure();
        std::vector<Obj> raw_args =
            std::ranges::to<std::vector>(ListView{rest});
        Env *macro_env = evaluator->alloc<LocalEnv>(p->env);
        p->formals.bind(macro_env, raw_args, evaluator);
        Obj expanded = evaluator->eval(p->body, macro_env);
        return TailCall{expanded, env};
      }
    }

    return eval_apply(head, rest, env, evaluator);
  }
}

struct EvalFrame {
  Evaluator *evaluator;
  EvalFrame(Evaluator *evaluator) : evaluator{evaluator} {
    if (!evaluator->push()) {
      throw SchemeError("recursion too deep");
    }
  }
  ~EvalFrame() { evaluator->pop(); }
};

Obj Evaluator::eval(Obj expr, Env *env) {
  EvalFrame frame{this};
  EvalResult result = eval_expr(expr, env, this);
  while (auto *tc = std::get_if<TailCall>(&result)) {
    result = eval_expr(tc->expr, tc->env, this);
  }
  return std::get<Obj>(result);
}
