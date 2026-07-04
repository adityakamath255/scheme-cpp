#include "parse.hpp"
#include "ctx.hpp"
#include <cassert>
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
          throw SchemeError("invalid escape sequence");
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
    return Number::parse(lexeme, ctx);
  }

  Obj parse_quoted(std::string_view name) {
    Symbol sym = ctx->intern(name);
    Obj quoted = parse_expr();
    return ctx->alloc<Cons>(
      sym,
      ctx->alloc<Cons>(
        quoted,
        Null{}
      )
    );
  }

  Obj parse_list() {
    if (match(Token::RPAREN)) {
      return Null{};
    }

    Obj first = parse_expr();
    Cons *head = ctx->alloc<Cons>(first, Null{});
    Cons *tail = head;

    while (!match(Token::RPAREN)) {
      if (match(Token::DOT)) {
        tail->cdr = parse_expr();
        if (!match(Token::RPAREN)) {
          throw SchemeError("expected ')' after dotted pair");
        }
        return head;
      }

      Cons *next = ctx->alloc<Cons>(parse_expr(), Null{});
      tail->cdr = next;
      tail = next;
    }

    return head;
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
        return true;

      case Token::FALSE:
        return false;

      case Token::PLUS_INF:
        return std::numeric_limits<double>::infinity();

      case Token::MINUS_INF:
        return -std::numeric_limits<double>::infinity();

      case Token::NAN_VAL:
        return std::numeric_limits<double>::quiet_NaN();

      case Token::NUMBER:
        return parse_number(tok.lexeme);

      case Token::STRING: {
        auto str = process_escapes(tok.lexeme);
        return ctx->alloc<String>(std::move(str));
      }

      case Token::SYMBOL:
        return ctx->intern(tok.lexeme);

      case Token::VEC_BEGIN: {
        std::vector<Obj> elements;
        while (!match(Token::RPAREN)) {
          auto obj = parse_expr();
          elements.push_back(obj);
        }
        return ctx->alloc<Vector>(std::move(elements));
      }

      case Token::CHAR: {
        std::string_view name = tok.lexeme.substr(2);
        if (name == "space") {
          return ' ';
        }
        else if (name == "newline") {
          return '\n';
        }
        else if (name == "tab") {
          return '\t';
        }
        else if (name == "return") {
          return '\r';
        }
        else {
          return name[0];
        }
      }

      default:
        throw SchemeError("unexpected token");
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
