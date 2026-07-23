#pragma once

#include "types.hpp"

#include <functional>
#include <optional>
#include <variant>
#include <vector>

class Ctx;
class Expr;

struct TailCall {
  const Expr *expression;
  std::reference_wrapper<Env> environment;
};

using EvalResult = std::variant<Obj, TailCall>;

class Expr : public HeapEntity {
public:
  virtual EvalResult eval(Env &, Ctx &) const = 0;
};

class LiteralExpr final : public Expr {
  const Obj value;

public:
  explicit LiteralExpr(Obj value);

  EvalResult eval(Env &, Ctx &) const override;
  void trace(std::vector<const HeapEntity *> &) const override;
};

class ReferenceExpr final : public Expr {
  const Symbol name;

public:
  explicit ReferenceExpr(Symbol name);

  EvalResult eval(Env &, Ctx &) const override;
};

class IfExpr final : public Expr {
  const Expr *const predicate;
  const Expr *const consequent;
  const Expr *const alternative;

public:
  IfExpr(const Expr *predicate, const Expr *consequent,
         const Expr *alternative);

  EvalResult eval(Env &, Ctx &) const override;
  void trace(std::vector<const HeapEntity *> &) const override;
};

class BeginExpr final : public Expr {
  const std::vector<const Expr *> expressions;

public:
  explicit BeginExpr(std::vector<const Expr *> expressions);

  EvalResult eval(Env &, Ctx &) const override;
  void trace(std::vector<const HeapEntity *> &) const override;
};

class LambdaExpr final : public Expr {
public:
  const Formals formals;
  const Expr *const body;

  LambdaExpr(Formals formals, const Expr *body);

  EvalResult eval(Env &, Ctx &) const override;
  void trace(std::vector<const HeapEntity *> &) const override;
};

class DefineExpr final : public Expr {
  const Symbol name;
  const Expr *const initializer;

public:
  DefineExpr(Symbol name, const Expr *initializer);

  EvalResult eval(Env &, Ctx &) const override;
  void trace(std::vector<const HeapEntity *> &) const override;
};

class SetExpr final : public Expr {
  const Symbol name;
  const Expr *const value;

public:
  SetExpr(Symbol name, const Expr *value);

  EvalResult eval(Env &, Ctx &) const override;
  void trace(std::vector<const HeapEntity *> &) const override;
};

class CallExpr final : public Expr {
  const Expr *const procedure;
  const std::vector<const Expr *> arguments;

public:
  CallExpr(const Expr *procedure, std::vector<const Expr *> arguments);

  EvalResult eval(Env &, Ctx &) const override;
  void trace(std::vector<const HeapEntity *> &) const override;
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
  LetExpr(LetKind kind, std::vector<Binding> bindings,
          const Expr *body);

  EvalResult eval(Env &, Ctx &) const override;
  void trace(std::vector<const HeapEntity *> &) const override;
};

enum class LogicalKind { And, Or };

class LogicalExpr final : public Expr {
  const LogicalKind kind;
  const std::vector<const Expr *> operands;

public:
  LogicalExpr(LogicalKind kind, std::vector<const Expr *> operands);

  EvalResult eval(Env &, Ctx &) const override;
  void trace(std::vector<const HeapEntity *> &) const override;
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
  explicit CondExpr(std::vector<CondClause> clauses);

  std::optional<EvalResult> try_eval(Env &, Ctx &) const;
  EvalResult eval(Env &, Ctx &) const override;
  void trace(std::vector<const HeapEntity *> &) const override;
};

struct CaseClause {
  std::optional<std::vector<Obj>> datums;
  const Expr *body;
};

class CaseExpr final : public Expr {
  const Expr *const key;
  const std::vector<CaseClause> clauses;

public:
  CaseExpr(const Expr *key, std::vector<CaseClause> clauses);

  EvalResult eval(Env &, Ctx &) const override;
  void trace(std::vector<const HeapEntity *> &) const override;
};

class GuardExpr final : public Expr {
  const Symbol variable;
  const CondExpr *const handler;
  const Expr *const body;

public:
  GuardExpr(Symbol variable, const CondExpr *handler, const Expr *body);

  EvalResult eval(Env &, Ctx &) const override;
  void trace(std::vector<const HeapEntity *> &) const override;
};

class DelayExpr final : public Expr {
  const Expr *const body;

public:
  explicit DelayExpr(const Expr *body);

  EvalResult eval(Env &, Ctx &) const override;
  void trace(std::vector<const HeapEntity *> &) const override;
};

class ConsStreamExpr final : public Expr {
  const Expr *const head;
  const Expr *const tail;

public:
  ConsStreamExpr(const Expr *head, const Expr *tail);

  EvalResult eval(Env &, Ctx &) const override;
  void trace(std::vector<const HeapEntity *> &) const override;
};
