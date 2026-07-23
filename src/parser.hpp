#pragma once

#include "quasiquote.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

class Parser {
  Ctx &context;

  using FormParser = const Expr *(Parser::*)(Obj);

  FormParser form_parser(std::string_view name) const;
  Obj expand_macro(Obj arguments, Symbol name, Procedure *macro);
  Obj expand_head(Obj);
  void define_macro(Obj, Env &);

  const Expr *parse_quote(Obj);
  const Expr *parse_if(Obj);
  const Expr *parse_define(Obj);
  const Expr *parse_set(Obj);
  const Expr *parse_lambda(Obj);
  const Expr *parse_begin(Obj);
  const Expr *parse_let(Obj);
  const Expr *parse_let_star(Obj);
  const Expr *parse_let_rec(Obj);
  const Expr *parse_when(Obj);
  const Expr *parse_unless(Obj);
  const Expr *parse_case(Obj);
  const Expr *parse_cond(Obj);
  const Expr *parse_and(Obj);
  const Expr *parse_or(Obj);
  const Expr *parse_quasiquote(Obj);
  const Expr *parse_guard(Obj);
  const Expr *parse_delay(Obj);
  const Expr *parse_cons_stream(Obj);
  const Expr *parse_define_macro(Obj);

  const Expr *parse_sequence(std::span<const Obj>);
  std::vector<Binding> parse_bindings(Obj, std::string_view);
  const LetExpr *parse_ordinary_let(Obj, LetKind, std::string_view);
  const CondExpr *parse_cond_clauses(Obj);
  const QuasiquoteTemplate *compile_quasiquote(Obj, size_t);
  QuasiquoteElement compile_quasiquote_element(Obj, size_t);
  const QuasiquoteTemplate *quasiquote_form(
      Symbol, const QuasiquoteTemplate *);

public:
  explicit Parser(Ctx &);

  const Expr *parse(Obj);
  Obj top_level(Obj, Env &);
};
