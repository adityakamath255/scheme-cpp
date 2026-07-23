#include "parser.hpp"

#include "arity.hpp"
#include "ctx.hpp"
#include "errors.hpp"

#include <format>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

static constexpr size_t max_macro_expansions = 1000;

namespace {

static bool is_keyword(Obj obj, std::string_view name) {
  auto symbol = obj.try_as_symbol();
  return symbol && symbol->name() == name;
}

static std::vector<Obj> list_elements(Obj list, std::string_view name) {
  List parts{list};
  if (!parts.proper()) {
    throw SchemeError(std::format("{}: expected proper list", name));
  }
  return std::move(parts.elements);
}

static std::vector<Obj> form_arguments(Obj rest, std::string_view name,
                                       Arity arity) {
  List arguments{rest};
  if (!arguments.proper()) {
    throw SchemeError(std::format("{}: improper argument list", name));
  }
  if (auto error = arity.mismatch(arguments.elements.size())) {
    throw SchemeError(std::format("{}: {}", name, *error));
  }
  return std::move(arguments.elements);
}

}

Parser::Parser(Ctx &context) : context{context} {}

Parser::FormParser Parser::form_parser(std::string_view name) const {
  static const std::unordered_map<std::string_view, FormParser> forms = {
      {"quote", &Parser::parse_quote},
      {"if", &Parser::parse_if},
      {"define", &Parser::parse_define},
      {"set!", &Parser::parse_set},
      {"lambda", &Parser::parse_lambda},
      {"begin", &Parser::parse_begin},
      {"let", &Parser::parse_let},
      {"let*", &Parser::parse_let_star},
      {"letrec", &Parser::parse_let_rec},
      {"when", &Parser::parse_when},
      {"unless", &Parser::parse_unless},
      {"case", &Parser::parse_case},
      {"cond", &Parser::parse_cond},
      {"and", &Parser::parse_and},
      {"or", &Parser::parse_or},
      {"quasiquote", &Parser::parse_quasiquote},
      {"guard", &Parser::parse_guard},
      {"delay", &Parser::parse_delay},
      {"cons-stream", &Parser::parse_cons_stream},
      {"define-macro", &Parser::parse_define_macro},
  };
  auto parser = forms.find(name);
  return parser == forms.end() ? nullptr : parser->second;
}

Obj Parser::expand_macro(Obj arguments, Symbol name, Procedure *macro) {
  std::vector<Obj> elements = list_elements(arguments, name.name());
  Env &env = *context.alloc<Env>(&macro->env.get());
  macro->code->formals.bind(env, elements, context);
  return context.eval(macro->code->body, env);
}

Obj Parser::expand_head(Obj expression) {
  for (size_t expansions = 0; expansions < max_macro_expansions;
       expansions += 1) {
    Cons *form = expression.try_as_cons();
    if (!form) {
      return expression;
    }
    auto name = form->car.try_as_symbol();
    if (!name) {
      return expression;
    }
    if (form_parser(name->name())) {
      return expression;
    }
    Procedure *macro = context.lookup_macro(*name);
    if (!macro) {
      return expression;
    }
    expression = expand_macro(form->cdr, *name, macro);
  }
  throw SchemeError("macro expansion too deep");
}

const Expr *Parser::parse(Obj datum) {
  Ctx::DepthGuard guard{context};
  if (auto symbol = datum.try_as_symbol()) {
    return context.alloc<ReferenceExpr>(*symbol);
  }
  Cons *form = datum.try_as_cons();
  if (!form) {
    return context.alloc<LiteralExpr>(datum);
  }

  Obj head = form->car;
  if (auto name = head.try_as_symbol()) {
    if (auto parser = form_parser(name->name())) {
      return (this->*parser)(form->cdr);
    }
    if (Procedure *macro = context.lookup_macro(*name)) {
      return parse(expand_macro(form->cdr, *name, macro));
    }
  }

  std::vector<Obj> raw_arguments =
      list_elements(form->cdr, "procedure call");
  const Expr *procedure = parse(head);
  std::vector<const Expr *> arguments;
  arguments.reserve(raw_arguments.size());
  for (Obj argument : raw_arguments) {
    arguments.push_back(parse(argument));
  }
  return context.alloc<CallExpr>(procedure, std::move(arguments));
}

const Expr *Parser::parse_sequence(std::span<const Obj> forms) {
  if (forms.empty()) {
    return context.alloc<LiteralExpr>(Void{});
  }
  if (forms.size() == 1) {
    return parse(forms.front());
  }
  std::vector<const Expr *> expressions;
  expressions.reserve(forms.size());
  for (Obj form : forms) {
    expressions.push_back(parse(form));
  }
  return context.alloc<BeginExpr>(std::move(expressions));
}

const Expr *Parser::parse_quote(Obj rest) {
  auto arguments = form_arguments(rest, "quote", Arity::exactly(1));
  return context.alloc<LiteralExpr>(arguments.front());
}

const Expr *Parser::parse_if(Obj rest) {
  auto arguments = form_arguments(rest, "if", Arity::between(2, 3));
  const Expr *predicate = parse(arguments[0]);
  const Expr *consequent = parse(arguments[1]);
  const Expr *alternative = arguments.size() == 3
                                ? parse(arguments[2])
                                : context.alloc<LiteralExpr>(Void{});
  return context.alloc<IfExpr>(predicate, consequent, alternative);
}

const Expr *Parser::parse_define(Obj rest) {
  auto arguments = form_arguments(rest, "define", Arity::at_least(1));
  Obj target = arguments.front();

  if (Cons *signature = target.try_as_cons()) {
    auto name = signature->car.try_as_symbol();
    if (!name) {
      throw SchemeError("define: procedure name must be a symbol");
    }
    if (arguments.size() < 2) {
      throw SchemeError("define: procedure body cannot be empty");
    }
    Formals formals = Formals::parse(signature->cdr);
    const Expr *body = parse_sequence(
        std::span{arguments}.subspan(1));
    const Expr *lambda =
        context.alloc<LambdaExpr>(std::move(formals), body);
    return context.alloc<DefineExpr>(*name, lambda);
  }

  auto name = target.try_as_symbol();
  if (!name) {
    throw SchemeError("define: expected symbol or list, got " +
                      target.type_name());
  }
  if (arguments.size() > 2) {
    throw SchemeError(std::format(
        "define: expected 2 arguments, got {}", arguments.size()));
  }
  const Expr *initializer = arguments.size() == 2
                                ? parse(arguments[1])
                                : context.alloc<LiteralExpr>(Void{});
  return context.alloc<DefineExpr>(*name, initializer);
}

const Expr *Parser::parse_set(Obj rest) {
  auto arguments = form_arguments(rest, "set!", Arity::exactly(2));
  auto name = arguments[0].try_as_symbol();
  if (!name) {
    throw SchemeError("set!: expected symbol, got " +
                      arguments[0].type_name());
  }
  return context.alloc<SetExpr>(*name, parse(arguments[1]));
}

const Expr *Parser::parse_lambda(Obj rest) {
  auto arguments = form_arguments(rest, "lambda", Arity::at_least(2));
  Formals formals = Formals::parse(arguments.front());
  const Expr *body = parse_sequence(std::span{arguments}.subspan(1));
  return context.alloc<LambdaExpr>(std::move(formals), body);
}

const Expr *Parser::parse_begin(Obj rest) {
  auto forms = form_arguments(rest, "begin", Arity::at_least(0));
  return parse_sequence(forms);
}

std::vector<Binding> Parser::parse_bindings(Obj datum,
                                             std::string_view name) {
  std::vector<Obj> raw_bindings = list_elements(datum, name);
  std::vector<Binding> bindings;
  bindings.reserve(raw_bindings.size());
  for (Obj raw_binding : raw_bindings) {
    std::vector<Obj> parts = list_elements(raw_binding, name);
    if (parts.size() != 2) {
      throw SchemeError(std::format(
          "{}: binding must contain a name and initializer", name));
    }
    auto binding_name = parts[0].try_as_symbol();
    if (!binding_name) {
      throw SchemeError(std::format(
          "{}: binding name must be a symbol", name));
    }
    bindings.push_back({*binding_name, parse(parts[1])});
  }
  return bindings;
}

const LetExpr *Parser::parse_ordinary_let(Obj rest, LetKind kind,
                                          std::string_view name) {
  auto arguments = form_arguments(rest, name, Arity::at_least(2));
  std::vector<Binding> bindings = parse_bindings(arguments[0], name);
  const Expr *body = parse_sequence(std::span{arguments}.subspan(1));
  return context.alloc<LetExpr>(kind, std::move(bindings), body);
}

const Expr *Parser::parse_let(Obj rest) {
  auto arguments = form_arguments(rest, "let", Arity::at_least(2));
  auto name = arguments.front().try_as_symbol();
  if (!name) {
    std::vector<Binding> bindings =
        parse_bindings(arguments.front(), "let");
    const Expr *body = parse_sequence(std::span{arguments}.subspan(1));
    return context.alloc<LetExpr>(LetKind::Plain, std::move(bindings),
                                  body);
  }

  if (arguments.size() < 3) {
    throw SchemeError("let: expected bindings and body");
  }
  std::vector<Binding> bindings = parse_bindings(arguments[1], "let");
  const Expr *body = parse_sequence(std::span{arguments}.subspan(2));

  std::vector<Symbol> parameters;
  std::vector<const Expr *> initializers;
  parameters.reserve(bindings.size());
  initializers.reserve(bindings.size());
  for (const Binding &binding : bindings) {
    parameters.push_back(binding.name);
    initializers.push_back(binding.initializer);
  }

  const Expr *lambda = context.alloc<LambdaExpr>(
      Formals{std::move(parameters), std::nullopt}, body);
  const Expr *procedure = context.alloc<LetExpr>(
      LetKind::Rec, std::vector<Binding>{{*name, lambda}},
      context.alloc<ReferenceExpr>(*name));
  return context.alloc<CallExpr>(procedure, std::move(initializers));
}

const Expr *Parser::parse_let_star(Obj rest) {
  return parse_ordinary_let(rest, LetKind::Star, "let*");
}

const Expr *Parser::parse_let_rec(Obj rest) {
  return parse_ordinary_let(rest, LetKind::Rec, "letrec");
}

const Expr *Parser::parse_when(Obj rest) {
  auto arguments = form_arguments(rest, "when", Arity::at_least(1));
  const Expr *test = parse(arguments.front());
  const Expr *body = parse_sequence(std::span{arguments}.subspan(1));
  return context.alloc<IfExpr>(
      test, body, context.alloc<LiteralExpr>(Void{}));
}

const Expr *Parser::parse_unless(Obj rest) {
  auto arguments = form_arguments(rest, "unless", Arity::at_least(1));
  const Expr *test = parse(arguments.front());
  const Expr *body = parse_sequence(std::span{arguments}.subspan(1));
  return context.alloc<IfExpr>(
      test, context.alloc<LiteralExpr>(Void{}), body);
}

const CondExpr *Parser::parse_cond_clauses(Obj datum) {
  std::vector<Obj> raw_clauses = list_elements(datum, "cond");
  std::vector<CondClause> clauses;
  clauses.reserve(raw_clauses.size());

  for (size_t i = 0; i < raw_clauses.size(); i += 1) {
    std::vector<Obj> clause = list_elements(raw_clauses[i], "cond");
    if (clause.empty()) {
      throw SchemeError("cond: clause cannot be empty");
    }

    bool is_else = is_keyword(clause.front(), "else");
    if (is_else) {
      if (i + 1 != raw_clauses.size()) {
        throw SchemeError("cond: else must be the last clause");
      }
      if (clause.size() == 1) {
        throw SchemeError("cond: else clause must have a body");
      }
      clauses.push_back(CondElse{
          parse_sequence(std::span{clause}.subspan(1))});
      continue;
    }

    const Expr *test = parse(clause.front());
    if (clause.size() == 1) {
      clauses.push_back(CondTest{test});
    } else if (is_keyword(clause[1], "=>")) {
      if (clause.size() != 3) {
        throw SchemeError("cond: expected one receiver after =>");
      }
      clauses.push_back(CondArrow{test, parse(clause[2])});
    } else {
      clauses.push_back(CondBody{
          test, parse_sequence(std::span{clause}.subspan(1))});
    }
  }
  return context.alloc<CondExpr>(std::move(clauses));
}

const Expr *Parser::parse_cond(Obj rest) {
  return parse_cond_clauses(rest);
}

const Expr *Parser::parse_case(Obj rest) {
  auto arguments = form_arguments(rest, "case", Arity::at_least(1));
  const Expr *key = parse(arguments.front());
  std::vector<CaseClause> clauses;
  clauses.reserve(arguments.size() - 1);

  for (size_t i = 1; i < arguments.size(); i += 1) {
    std::vector<Obj> clause = list_elements(arguments[i], "case");
    if (clause.empty()) {
      throw SchemeError("case: clause cannot be empty");
    }
    bool is_else = is_keyword(clause.front(), "else");
    if (is_else && i + 1 != arguments.size()) {
      throw SchemeError("case: else must be the last clause");
    }

    std::optional<std::vector<Obj>> datums;
    if (!is_else) {
      datums = list_elements(clause.front(), "case");
    }
    const Expr *body = parse_sequence(std::span{clause}.subspan(1));
    clauses.push_back({std::move(datums), body});
  }
  return context.alloc<CaseExpr>(key, std::move(clauses));
}

static std::vector<const Expr *> parse_operands(Parser &parser, Obj rest,
                                                std::string_view name) {
  std::vector<Obj> raw = form_arguments(rest, name, Arity::at_least(0));
  std::vector<const Expr *> expressions;
  expressions.reserve(raw.size());
  for (Obj operand : raw) {
    expressions.push_back(parser.parse(operand));
  }
  return expressions;
}

const Expr *Parser::parse_and(Obj rest) {
  auto operands = parse_operands(*this, rest, "and");
  if (operands.empty()) {
    return context.alloc<LiteralExpr>(true);
  }
  if (operands.size() == 1) {
    return operands.front();
  }
  return context.alloc<LogicalExpr>(
      LogicalKind::And, std::move(operands));
}

const Expr *Parser::parse_or(Obj rest) {
  auto operands = parse_operands(*this, rest, "or");
  if (operands.empty()) {
    return context.alloc<LiteralExpr>(false);
  }
  if (operands.size() == 1) {
    return operands.front();
  }
  return context.alloc<LogicalExpr>(
      LogicalKind::Or, std::move(operands));
}

const QuasiquoteTemplate *Parser::quasiquote_form(
    Symbol keyword, const QuasiquoteTemplate *argument) {
  auto *null = context.alloc<QuasiquoteTemplate>(Obj(Null{}));
  auto *tail = context.alloc<QuasiquoteTemplate>(
      QuasiquoteTemplate::Pair{argument, null});
  auto *head = context.alloc<QuasiquoteTemplate>(Obj(keyword));
  return context.alloc<QuasiquoteTemplate>(
      QuasiquoteTemplate::Pair{head, tail});
}

const QuasiquoteTemplate *Parser::compile_quasiquote(
    Obj datum, size_t quasiquote_depth) {
  if (Vector *vector = datum.try_as_vector()) {
    std::vector<QuasiquoteElement> elements;
    elements.reserve(vector->data.size());
    for (Obj element : vector->data) {
      elements.push_back(
          compile_quasiquote_element(element, quasiquote_depth));
    }
    return context.alloc<QuasiquoteTemplate>(
        QuasiquoteTemplate::VectorElements{std::move(elements)});
  }

  Cons *pair = datum.try_as_cons();
  if (!pair) {
    return context.alloc<QuasiquoteTemplate>(datum);
  }

  if (auto keyword = pair->car.try_as_symbol()) {
    bool is_quasiquote = keyword->name() == "quasiquote";
    bool is_unquote = keyword->name() == "unquote";
    bool is_splice = keyword->name() == "unquote-splicing";
    if (is_quasiquote || is_unquote || is_splice) {
      auto arguments = form_arguments(pair->cdr, keyword->name(),
                                      Arity::exactly(1));
      if (is_quasiquote) {
        return quasiquote_form(
            *keyword,
            compile_quasiquote(arguments.front(), quasiquote_depth + 1));
      }
      if (quasiquote_depth == 1) {
        if (is_splice) {
          throw SchemeError(
              "unquote-splicing: not in a list or vector");
        }
        return context.alloc<QuasiquoteTemplate>(
            QuasiquoteTemplate::Value{parse(arguments.front())});
      }
      return quasiquote_form(
          *keyword,
          compile_quasiquote(arguments.front(), quasiquote_depth - 1));
    }
  }

  QuasiquoteElement car =
      compile_quasiquote_element(pair->car, quasiquote_depth);
  const QuasiquoteTemplate *cdr =
      compile_quasiquote(pair->cdr, quasiquote_depth);
  return context.alloc<QuasiquoteTemplate>(
      QuasiquoteTemplate::Pair{car, cdr});
}

QuasiquoteElement Parser::compile_quasiquote_element(
    Obj datum, size_t quasiquote_depth) {
  if (quasiquote_depth == 1) {
    if (Cons *pair = datum.try_as_cons();
        pair && is_keyword(pair->car, "unquote-splicing")) {
      auto arguments = form_arguments(
          pair->cdr, "unquote-splicing", Arity::exactly(1));
      return QuasiquoteSplice{parse(arguments.front())};
    }
  }
  return compile_quasiquote(datum, quasiquote_depth);
}

const Expr *Parser::parse_quasiquote(Obj rest) {
  auto arguments =
      form_arguments(rest, "quasiquote", Arity::exactly(1));
  return context.alloc<QuasiquoteExpr>(
      compile_quasiquote(arguments.front(), 1));
}

const Expr *Parser::parse_guard(Obj rest) {
  auto arguments = form_arguments(rest, "guard", Arity::at_least(2));
  Obj spec = arguments.front();
  Cons *guard = spec.try_as_cons();
  auto variable = guard ? guard->car.try_as_symbol() : std::nullopt;
  if (!variable) {
    throw SchemeError("guard: expected (variable clause ...)");
  }
  const CondExpr *handler = parse_cond_clauses(guard->cdr);
  const Expr *body = parse_sequence(std::span{arguments}.subspan(1));
  return context.alloc<GuardExpr>(*variable, handler, body);
}

const Expr *Parser::parse_delay(Obj rest) {
  auto arguments = form_arguments(rest, "delay", Arity::exactly(1));
  return context.alloc<DelayExpr>(parse(arguments.front()));
}

const Expr *Parser::parse_cons_stream(Obj rest) {
  auto arguments =
      form_arguments(rest, "cons-stream", Arity::exactly(2));
  const Expr *head = parse(arguments[0]);
  const Expr *tail = parse(arguments[1]);
  return context.alloc<ConsStreamExpr>(head, tail);
}

const Expr *Parser::parse_define_macro(Obj) {
  throw SchemeError("define-macro: only allowed at top level");
}

void Parser::define_macro(Obj rest, Env &env) {
  auto arguments =
      form_arguments(rest, "define-macro", Arity::at_least(2));
  Obj target = arguments.front();

  if (Cons *signature = target.try_as_cons()) {
    auto name = signature->car.try_as_symbol();
    if (!name) {
      throw SchemeError("define-macro: name must be a symbol");
    }
    Formals formals = Formals::parse(signature->cdr);
    const Expr *body = parse_sequence(std::span{arguments}.subspan(1));
    const auto *code =
        context.alloc<LambdaExpr>(std::move(formals), body);
    context.define_macro(
        *name, context.alloc<Procedure>(code, env));
    return;
  }

  auto name = target.try_as_symbol();
  if (!name) {
    throw SchemeError("define-macro: expected symbol or list");
  }
  if (arguments.size() != 2) {
    throw SchemeError(std::format(
        "define-macro: expected 2 arguments, got {}", arguments.size()));
  }
  Obj value = context.eval(parse(arguments[1]), env);
  Procedure *macro = value.try_as_procedure();
  if (!macro) {
    throw SchemeError("define-macro: expected procedure");
  }
  context.define_macro(*name, macro);
}

// top-level forms
Obj Parser::top_level(Obj datum, Env &env) {
  datum = expand_head(datum);
  if (Cons *form = datum.try_as_cons()) {
    if (is_keyword(form->car, "begin")) {
      std::vector<Obj> forms =
          form_arguments(form->cdr, "begin", Arity::at_least(0));
      Obj result = Void{};
      for (Obj child : forms) {
        result = top_level(child, env);
      }
      return result;
    }

    if (is_keyword(form->car, "define-macro")) {
      define_macro(form->cdr, env);
      return Void{};
    }
  }
  return context.eval(parse(datum), env);
}
