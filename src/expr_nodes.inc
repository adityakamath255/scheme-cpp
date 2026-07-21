#include "eval.hpp"
#include "expr.hpp"

#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

static void trace_expression(
    const Expr *expression,
    std::vector<const HeapEntity *> &worklist) {
  worklist.push_back(expression);
}

static void trace_expressions(
    std::span<const Expr *const> expressions,
    std::vector<const HeapEntity *> &worklist) {
  worklist.append_range(expressions);
}

static EvalResult apply_procedure(Obj, std::vector<Obj>, EvalContext &);

class LiteralExpr final : public Expr {
  const Obj value;

public:
  explicit LiteralExpr(Obj value) : value{value} {}

  EvalResult eval(Env &, EvalContext &) const override { return value; }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    trace_child(value, worklist);
  }
};

class ReferenceExpr final : public Expr {
  const Symbol name;

public:
  explicit ReferenceExpr(Symbol name) : name{name} {}

  EvalResult eval(Env &env, EvalContext &) const override {
    if (auto value = env.lookup(name)) {
      return *value;
    }
    throw SchemeError("undefined variable: " + name.name());
  }
};

class IfExpr final : public Expr {
  const Expr *const predicate;
  const Expr *const consequent;
  const Expr *const alternative;

public:
  IfExpr(const Expr *predicate, const Expr *consequent,
         const Expr *alternative)
      : predicate{predicate}, consequent{consequent},
        alternative{alternative} {}

  EvalResult eval(Env &env, EvalContext &context) const override {
    const Expr *branch = context.eval(predicate, env).is_true()
                             ? consequent
                             : alternative;
    return TailCall{branch, env};
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    trace_expression(predicate, worklist);
    trace_expression(consequent, worklist);
    trace_expression(alternative, worklist);
  }
};

static EvalResult eval_sequence(std::span<const Expr *const> expressions,
                                Env &env, EvalContext &context) {
  if (expressions.empty()) {
    return Obj(Void{});
  }
  for (const Expr *expression : expressions.first(expressions.size() - 1)) {
    context.eval(expression, env);
  }
  return TailCall{expressions.back(), env};
}

class BeginExpr final : public Expr {
  const std::vector<const Expr *> expressions;

public:
  explicit BeginExpr(std::vector<const Expr *> expressions)
      : expressions{std::move(expressions)} {}

  EvalResult eval(Env &env, EvalContext &context) const override {
    return eval_sequence(expressions, env, context);
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    trace_expressions(expressions, worklist);
  }
};

class LambdaExpr final : public Expr {
  const Formals formals;
  const Expr *const body;

public:
  LambdaExpr(Formals formals, const Expr *body)
      : formals{std::move(formals)}, body{body} {}

  EvalResult eval(Env &env, EvalContext &context) const override {
    return Obj(context.alloc<Procedure>(formals, body, env));
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    trace_expression(body, worklist);
  }
};

class DefineExpr final : public Expr {
  const Symbol name;
  const Expr *const initializer;

public:
  DefineExpr(Symbol name, const Expr *initializer)
      : name{name}, initializer{initializer} {}

  EvalResult eval(Env &env, EvalContext &context) const override {
    env.define(name, context.eval(initializer, env));
    return Obj(Void{});
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    trace_expression(initializer, worklist);
  }
};

class SetExpr final : public Expr {
  const Symbol name;
  const Expr *const value;

public:
  SetExpr(Symbol name, const Expr *value) : name{name}, value{value} {}

  EvalResult eval(Env &env, EvalContext &context) const override {
    if (!env.set(name, context.eval(value, env))) {
      throw SchemeError("set!: undefined variable " + name.name());
    }
    return Obj(Void{});
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    trace_expression(value, worklist);
  }
};

class CallExpr final : public Expr {
  const Expr *const procedure;
  const std::vector<const Expr *> arguments;

public:
  CallExpr(const Expr *procedure, std::vector<const Expr *> arguments)
      : procedure{procedure}, arguments{std::move(arguments)} {}

  EvalResult eval(Env &env, EvalContext &context) const override {
    Obj callable = context.eval(procedure, env);
    std::vector<Obj> values;
    values.reserve(arguments.size());
    for (const Expr *argument : arguments) {
      values.push_back(context.eval(argument, env));
    }
    return apply_procedure(callable, std::move(values), context);
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    trace_expression(procedure, worklist);
    trace_expressions(arguments, worklist);
  }
};

enum class LetKind { Plain, Star, Rec };

struct Binding {
  Symbol name;
  const Expr *initializer;
};

class LetExpr final : public Expr {
  const LetKind kind;
  const std::vector<Binding> bindings;
  const Expr *const body;

public:
  LetExpr(LetKind kind, std::vector<Binding> bindings, const Expr *body)
      : kind{kind}, bindings{std::move(bindings)}, body{body} {}

  EvalResult eval(Env &env, EvalContext &context) const override {
    Env &local = *context.alloc<Env>(&env);
    if (kind == LetKind::Rec) {
      for (const auto &binding : bindings) {
        local.define(binding.name, Void{});
      }
    }

    Env &initializer_env = kind == LetKind::Plain ? env : local;
    for (const auto &binding : bindings) {
      Obj value = context.eval(binding.initializer, initializer_env);
      if (kind == LetKind::Rec) {
        local.set(binding.name, value);
      } else {
        local.define(binding.name, value);
      }
    }
    return TailCall{body, local};
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    for (const auto &binding : bindings) {
      trace_expression(binding.initializer, worklist);
    }
    trace_expression(body, worklist);
  }
};

class NamedLetExpr final : public Expr {
  const Symbol name;
  const std::vector<Binding> bindings;
  const Expr *const body;

public:
  NamedLetExpr(Symbol name, std::vector<Binding> bindings,
               const Expr *body)
      : name{name}, bindings{std::move(bindings)}, body{body} {}

  EvalResult eval(Env &env, EvalContext &context) const override {
    std::vector<Symbol> parameters;
    std::vector<Obj> arguments;
    parameters.reserve(bindings.size());
    arguments.reserve(bindings.size());
    for (const auto &binding : bindings) {
      parameters.push_back(binding.name);
      arguments.push_back(context.eval(binding.initializer, env));
    }

    Env &loop_env = *context.alloc<Env>(&env);
    auto *procedure = context.alloc<Procedure>(
        Formals{std::move(parameters), std::nullopt}, body, loop_env);
    loop_env.define(name, procedure);

    Env &call_env = *context.alloc<Env>(&loop_env);
    procedure->formals.bind(call_env, arguments, context);
    return TailCall{body, call_env};
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    for (const auto &binding : bindings) {
      trace_expression(binding.initializer, worklist);
    }
    trace_expression(body, worklist);
  }
};

enum class LogicalKind { And, Or };

class LogicalExpr final : public Expr {
  const LogicalKind kind;
  const std::vector<const Expr *> operands;

public:
  LogicalExpr(LogicalKind kind, std::vector<const Expr *> operands)
      : kind{kind}, operands{std::move(operands)} {}

  EvalResult eval(Env &env, EvalContext &context) const override {
    bool conjunction = kind == LogicalKind::And;
    if (operands.empty()) {
      return Obj(conjunction);
    }
    for (const Expr *operand :
         std::span{operands}.first(operands.size() - 1)) {
      Obj value = context.eval(operand, env);
      if (value.is_true() != conjunction) {
        return value;
      }
    }
    return TailCall{operands.back(), env};
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    trace_expressions(operands, worklist);
  }
};

struct CondTest {
  const Expr *test;
};

struct CondBody {
  const Expr *test;
  const Expr *body;
};

struct CondArrow {
  const Expr *test;
  const Expr *receiver;
};

struct CondElse {
  const Expr *body;
};

using CondClause = std::variant<CondTest, CondBody, CondArrow, CondElse>;

class CondExpr final : public Expr {
  const std::vector<CondClause> clauses;

public:
  explicit CondExpr(std::vector<CondClause> clauses)
      : clauses{std::move(clauses)} {}

  std::optional<EvalResult> try_eval(Env &env,
                                     EvalContext &context) const {
    for (const CondClause &clause : clauses) {
      auto result = std::visit(
          overloaded{
              [&](CondTest c) -> std::optional<EvalResult> {
                Obj value = context.eval(c.test, env);
                return value.is_false()
                           ? std::nullopt
                           : std::optional<EvalResult>{value};
              },
              [&](CondBody c) -> std::optional<EvalResult> {
                return context.eval(c.test, env).is_false()
                           ? std::nullopt
                           : std::optional<EvalResult>{TailCall{c.body, env}};
              },
              [&](CondArrow c) -> std::optional<EvalResult> {
                Obj value = context.eval(c.test, env);
                if (value.is_false()) {
                  return std::nullopt;
                }
                Obj receiver = context.eval(c.receiver, env);
                return apply_procedure(receiver, {value}, context);
              },
              [&](CondElse c) -> std::optional<EvalResult> {
                return TailCall{c.body, env};
              },
          },
          clause);
      if (result) {
        return result;
      }
    }
    return std::nullopt;
  }

  EvalResult eval(Env &env, EvalContext &context) const override {
    return try_eval(env, context).value_or(EvalResult{Obj(Void{})});
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    for (const CondClause &clause : clauses) {
      std::visit(overloaded{
                     [&](CondTest c) {
                       trace_expression(c.test, worklist);
                     },
                     [&](CondBody c) {
                       trace_expression(c.test, worklist);
                       trace_expression(c.body, worklist);
                     },
                     [&](CondArrow c) {
                       trace_expression(c.test, worklist);
                       trace_expression(c.receiver, worklist);
                     },
                     [&](CondElse c) {
                       trace_expression(c.body, worklist);
                     },
                 },
                 clause);
    }
  }
};

struct CaseClause {
  std::optional<std::vector<Obj>> datums;
  const Expr *body;
};

class CaseExpr final : public Expr {
  const Expr *const key;
  const std::vector<CaseClause> clauses;

public:
  CaseExpr(const Expr *key, std::vector<CaseClause> clauses)
      : key{key}, clauses{std::move(clauses)} {}

  EvalResult eval(Env &env, EvalContext &context) const override {
    Obj value = context.eval(key, env);
    for (const auto &clause : clauses) {
      bool matched = !clause.datums;
      if (clause.datums) {
        matched = std::ranges::any_of(
            *clause.datums,
            [&](Obj datum) { return value.equals(datum); });
      }
      if (matched) {
        return TailCall{clause.body, env};
      }
    }
    return Obj(Void{});
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    trace_expression(key, worklist);
    for (const auto &clause : clauses) {
      if (clause.datums) {
        for (Obj datum : *clause.datums) {
          trace_child(datum, worklist);
        }
      }
      trace_expression(clause.body, worklist);
    }
  }
};

class GuardExpr final : public Expr {
  const Symbol variable;
  const CondExpr *const handler;
  const Expr *const body;

public:
  GuardExpr(Symbol variable, const CondExpr *handler, const Expr *body)
      : variable{variable}, handler{handler}, body{body} {}

  EvalResult eval(Env &env, EvalContext &context) const override {
    try {
      return context.eval(body, env);
    } catch (SchemeError &error) {
      Env &handler_env = *context.alloc<Env>(&env);
      handler_env.define(variable, error.as_condition(context));
      if (auto handled = handler->try_eval(handler_env, context)) {
        return *handled;
      }
      throw;
    }
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    trace_expression(handler, worklist);
    trace_expression(body, worklist);
  }
};

class DelayExpr final : public Expr {
  const Expr *const body;

public:
  explicit DelayExpr(const Expr *body) : body{body} {}

  EvalResult eval(Env &env, EvalContext &context) const override {
    return Obj(context.alloc<Promise>(body, env));
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    trace_expression(body, worklist);
  }
};

class ConsStreamExpr final : public Expr {
  const Expr *const head;
  const Expr *const tail;

public:
  ConsStreamExpr(const Expr *head, const Expr *tail)
      : head{head}, tail{tail} {}

  EvalResult eval(Env &env, EvalContext &context) const override {
    Obj value = context.eval(head, env);
    return Obj(context.alloc<Cons>(
        value, context.alloc<Promise>(tail, env)));
  }

  void trace(std::vector<const HeapEntity *> &worklist) const override {
    trace_expression(head, worklist);
    trace_expression(tail, worklist);
  }
};

static std::pair<Obj, std::vector<Obj>>
splice_apply(const std::vector<Obj> &arguments) {
  if (arguments.size() < 2) {
    throw SchemeError("apply: expected at least 2 arguments");
  }

  ListView rest{arguments.back()};
  if (!rest.tail().is_null()) {
    throw SchemeError("apply: last argument must be a proper list");
  }

  std::vector<Obj> call_arguments(arguments.begin() + 1,
                                  arguments.end() - 1);
  call_arguments.append_range(rest);
  return {arguments.front(), std::move(call_arguments)};
}

static EvalResult apply_procedure(Obj procedure, std::vector<Obj> arguments,
                                  EvalContext &context) {
  while (true) {
    if (procedure.is_procedure()) {
      Procedure *callable = procedure.as_procedure();
      Env &env = *context.alloc<Env>(&callable->env.get());
      callable->formals.bind(env, arguments, context);
      return TailCall{callable->body, env};
    }

    if (procedure.is_builtin()) {
      Builtin *builtin = procedure.as_builtin();
      if (auto *function = std::get_if<Builtin::Fn>(
              &builtin->implementation)) {
        return (*function)(arguments, context);
      }
      auto [next_procedure, next_arguments] = splice_apply(arguments);
      procedure = next_procedure;
      arguments = std::move(next_arguments);
      continue;
    }

    throw SchemeError("not a procedure: " + procedure.to_display());
  }
}

}
