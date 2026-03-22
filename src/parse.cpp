#include "parse.hpp"
#include "ctx.hpp"
#include <stdexcept>
#include <cassert>
#include <charconv>
#include <limits>

static std::string process_escapes(std::string_view raw) {
  std::string res;
  res.reserve(raw.size());

  for (size_t i = 0; i < raw.size(); i += 1) {
    if (raw[i] == '\\' && i + 1 < raw.size()) {
      i += 1;
      switch (raw[i]) {
        case 'n': res += '\n'; break;
        case 't': res += '\t'; break;
        case 'r': res += '\r'; break;
        case '\\': res += '\\'; break;
        case '"': res += '"'; break;
        default:
          throw std::runtime_error("invalid escape sequence");
      }
    }

    else {
      res += raw[i];
    }
  }

  return res;
}

class Parser {
  const std::vector<Token> &tokens;
  size_t index;
  Ctx *ctx;

  bool match(Token::Type type) {
    if (index < tokens.size() && tokens[index].type == type) {
      index += 1;
      return true;
    }
    else {
      return false;
    }
  }

  Obj parse_number(std::string_view lexeme) {
    double val;
    auto [ptr, ec] = std::from_chars(
      lexeme.data(),
      lexeme.data() + lexeme.size(),
      val
    );
    assert(
      ec == std::errc{}
      && ptr == lexeme.data() + lexeme.size()
    );
    return Obj(val);
  }

  Obj parse_quoted(std::string_view name) {
    Symbol sym = ctx->intern(name);
    Obj quoted = parse_expr();
    return Obj(ctx->alloc<Cons>(
      Obj(sym),
      Obj(ctx->alloc<Cons>(
        quoted,
        Obj(Null{})
      ))
    ));
  }

  Obj parse_list() {
    if (match(Token::RPAREN)) {
      return Obj(Null{});
    }

    Obj first = parse_expr();
    Cons *head = ctx->alloc<Cons>(first, Obj(Null {}));
    Cons *tail = head;

    while (!match(Token::RPAREN)) {
      if (match(Token::DOT)) {
        tail->cdr = parse_expr();
        if (!match(Token::RPAREN)) {
          throw std::runtime_error("expected ')' after dotted pair");
        }
        return Obj(head);
      }

      Cons *next = ctx->alloc<Cons>(parse_expr(), Obj(Null{}));
      tail->cdr = Obj(next);
      tail = next;
    }

    return Obj(head);
  }

  Obj parse_expr() {
    assert(index < tokens.size());
    const auto &tok = tokens[index];
    index += 1;

    switch (tok.type) {
      case Token::LPAREN:
        return parse_list();

      case Token::QUOTE:
        return parse_quoted("quote");

      case Token::BACKTICK:
        return parse_quoted("quasiquote");

      case Token::COMMA:
        return parse_quoted("unquote");

      case Token::SPLICE_COMMA:
        return parse_quoted("unquote-splicing");

      case Token::TRUE:
        return Obj(true);

      case Token::FALSE:
        return Obj(false);

      case Token::PLUS_INF:
        return Obj(std::numeric_limits<double>::infinity());

      case Token::MINUS_INF:
        return Obj(-std::numeric_limits<double>::infinity());

      case Token::NAN_VAL:
        return Obj(std::numeric_limits<double>::quiet_NaN());

      case Token::NUMBER:
        return parse_number(tok.lexeme);

      case Token::STRING: {
        auto str = process_escapes(tok.lexeme);
        return Obj(ctx->alloc<String>(std::move(str)));
      }

      case Token::SYMBOL:
        return Obj(ctx->intern(tok.lexeme));

      default:
        throw std::runtime_error("unexpected token");
    }
  }

public:
  Parser(const std::vector<Token> &tokens, Ctx *ctx):
    tokens {tokens},
    index {0},
    ctx {ctx}
  {}

  Obj parse() {
    assert(!tokens.empty());
    return parse_expr();
  }
};

Obj parse(const std::vector<Token> &tokens, Ctx *ctx) {
  return Parser(tokens, ctx).parse();
}
