#include "expression.hpp"

#include "ctx.hpp"

#include <ranges>
#include <span>
#include <utility>
#include <variant>

namespace {

void trace_expression(const Expr *expression,
                      std::vector<const HeapEntity *> &worklist) {
  worklist.push_back(expression);
}

void trace_expressions(std::span<const Expr *const> expressions,
                       std::vector<const HeapEntity *> &worklist) {
  worklist.append_range(expressions);
}

EvalResult apply_procedure(Obj, std::vector<Obj>, Ctx &);

EvalResult eval_sequence(std::span<const Expr *const> expressions,
                         Env &env, Ctx &context) {
  if (expressions.empty()) {
    return Obj(Void{});
  }
  for (const Expr *expression : expressions.first(expressions.size() - 1)) {
    context.eval(expression, env);
  }
  return TailCall{expressions.back(), env};
}

std::pair<Obj, std::vector<Obj>>
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

EvalResult apply_procedure(Obj procedure, std::vector<Obj> arguments,
                           Ctx &context) {
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

LiteralExpr::LiteralExpr(Obj value) : value{value} {}

EvalResult LiteralExpr::eval(Env &, Ctx &) const { return value; }

void LiteralExpr::trace(
    std::vector<const HeapEntity *> &worklist) const {
  trace_child(value, worklist);
}

ReferenceExpr::ReferenceExpr(Symbol name) : name{name} {}

EvalResult ReferenceExpr::eval(Env &env, Ctx &) const {
  if (auto value = env.lookup(name)) {
    return *value;
  }
  throw SchemeError("undefined variable: " + name.name());
}

IfExpr::IfExpr(const Expr *predicate, const Expr *consequent,
               const Expr *alternative)
    : predicate{predicate}, consequent{consequent},
      alternative{alternative} {}

EvalResult IfExpr::eval(Env &env, Ctx &context) const {
  const Expr *branch = context.eval(predicate, env).is_true()
                           ? consequent
                           : alternative;
  return TailCall{branch, env};
}

void IfExpr::trace(std::vector<const HeapEntity *> &worklist) const {
  trace_expression(predicate, worklist);
  trace_expression(consequent, worklist);
  trace_expression(alternative, worklist);
}

BeginExpr::BeginExpr(std::vector<const Expr *> expressions)
    : expressions{std::move(expressions)} {}

EvalResult BeginExpr::eval(Env &env, Ctx &context) const {
  return eval_sequence(expressions, env, context);
}

void BeginExpr::trace(std::vector<const HeapEntity *> &worklist) const {
  trace_expressions(expressions, worklist);
}

LambdaExpr::LambdaExpr(Formals formals, const Expr *body)
    : formals{std::move(formals)}, body{body} {}

EvalResult LambdaExpr::eval(Env &env, Ctx &context) const {
  return Obj(context.alloc<Procedure>(formals, body, env));
}

void LambdaExpr::trace(std::vector<const HeapEntity *> &worklist) const {
  trace_expression(body, worklist);
}

DefineExpr::DefineExpr(Symbol name, const Expr *initializer)
    : name{name}, initializer{initializer} {}

EvalResult DefineExpr::eval(Env &env, Ctx &context) const {
  env.define(name, context.eval(initializer, env));
  return Obj(Void{});
}

void DefineExpr::trace(std::vector<const HeapEntity *> &worklist) const {
  trace_expression(initializer, worklist);
}

SetExpr::SetExpr(Symbol name, const Expr *value)
    : name{name}, value{value} {}

EvalResult SetExpr::eval(Env &env, Ctx &context) const {
  if (!env.set(name, context.eval(value, env))) {
    throw SchemeError("set!: undefined variable " + name.name());
  }
  return Obj(Void{});
}

void SetExpr::trace(std::vector<const HeapEntity *> &worklist) const {
  trace_expression(value, worklist);
}

CallExpr::CallExpr(const Expr *procedure,
                   std::vector<const Expr *> arguments)
    : procedure{procedure}, arguments{std::move(arguments)} {}

EvalResult CallExpr::eval(Env &env, Ctx &context) const {
  Obj callable = context.eval(procedure, env);
  std::vector<Obj> values;
  values.reserve(arguments.size());
  for (const Expr *argument : arguments) {
    values.push_back(context.eval(argument, env));
  }
  return apply_procedure(callable, std::move(values), context);
}

void CallExpr::trace(std::vector<const HeapEntity *> &worklist) const {
  trace_expression(procedure, worklist);
  trace_expressions(arguments, worklist);
}

LetExpr::LetExpr(LetKind kind, std::vector<Binding> bindings,
                 const Expr *body)
    : kind{kind}, bindings{std::move(bindings)}, body{body} {}

EvalResult LetExpr::eval(Env &env, Ctx &context) const {
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

void LetExpr::trace(std::vector<const HeapEntity *> &worklist) const {
  for (const auto &binding : bindings) {
    trace_expression(binding.initializer, worklist);
  }
  trace_expression(body, worklist);
}

NamedLetExpr::NamedLetExpr(Symbol name, std::vector<Binding> bindings,
                           const Expr *body)
    : name{name}, bindings{std::move(bindings)}, body{body} {}

EvalResult NamedLetExpr::eval(Env &env, Ctx &context) const {
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

void NamedLetExpr::trace(
    std::vector<const HeapEntity *> &worklist) const {
  for (const auto &binding : bindings) {
    trace_expression(binding.initializer, worklist);
  }
  trace_expression(body, worklist);
}

LogicalExpr::LogicalExpr(LogicalKind kind,
                         std::vector<const Expr *> operands)
    : kind{kind}, operands{std::move(operands)} {}

EvalResult LogicalExpr::eval(Env &env, Ctx &context) const {
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

void LogicalExpr::trace(
    std::vector<const HeapEntity *> &worklist) const {
  trace_expressions(operands, worklist);
}

CondExpr::CondExpr(std::vector<CondClause> clauses)
    : clauses{std::move(clauses)} {}

std::optional<EvalResult> CondExpr::try_eval(
    Env &env, Ctx &context) const {
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

EvalResult CondExpr::eval(Env &env, Ctx &context) const {
  return try_eval(env, context).value_or(EvalResult{Obj(Void{})});
}

void CondExpr::trace(std::vector<const HeapEntity *> &worklist) const {
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

CaseExpr::CaseExpr(const Expr *key, std::vector<CaseClause> clauses)
    : key{key}, clauses{std::move(clauses)} {}

EvalResult CaseExpr::eval(Env &env, Ctx &context) const {
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

void CaseExpr::trace(std::vector<const HeapEntity *> &worklist) const {
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

GuardExpr::GuardExpr(Symbol variable, const CondExpr *handler,
                     const Expr *body)
    : variable{variable}, handler{handler}, body{body} {}

EvalResult GuardExpr::eval(Env &env, Ctx &context) const {
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

void GuardExpr::trace(std::vector<const HeapEntity *> &worklist) const {
  trace_expression(handler, worklist);
  trace_expression(body, worklist);
}

DelayExpr::DelayExpr(const Expr *body) : body{body} {}

EvalResult DelayExpr::eval(Env &env, Ctx &context) const {
  return Obj(context.alloc<Promise>(body, env));
}

void DelayExpr::trace(std::vector<const HeapEntity *> &worklist) const {
  trace_expression(body, worklist);
}

ConsStreamExpr::ConsStreamExpr(const Expr *head, const Expr *tail)
    : head{head}, tail{tail} {}

EvalResult ConsStreamExpr::eval(Env &env, Ctx &context) const {
  Obj value = context.eval(head, env);
  return Obj(context.alloc<Cons>(
      value, context.alloc<Promise>(tail, env)));
}

void ConsStreamExpr::trace(
    std::vector<const HeapEntity *> &worklist) const {
  trace_expression(head, worklist);
  trace_expression(tail, worklist);
}
