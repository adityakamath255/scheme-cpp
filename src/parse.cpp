#include "parse.hpp"
#include "runtime.hpp"
#include <array>
#include <cassert>
#include <string>

static std::string process_escapes(std::string_view raw) {
  std::string res;
  res.reserve(raw.size());

  for (size_t i = 0; i < raw.size(); i += 1) {
    if (raw[i] == '\\' && i + 1 < raw.size()) {
      i += 1;
      switch (raw[i]) {
      case 'n':
        res += '\n';
        break;
      case 't':
        res += '\t';
        break;
      case 'r':
        res += '\r';
        break;
      case '\\':
        res += '\\';
        break;
      case '"':
        res += '"';
        break;
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
  Runtime *runtime;

  bool match(Token::Type type) {
    if (index < tokens.size() && tokens[index].type == type) {
      index += 1;
      return true;
    } else {
      return false;
    }
  }

  Obj parse_quoted(Symbol sym) {
    return list_from(std::array<Obj, 2>{sym, parse_expr()}, runtime);
  }

  Obj parse_list() {
    std::vector<Obj> elements;

    while (!match(Token::RPAREN)) {
      if (match(Token::DOT)) {
        if (elements.empty()) {
          throw SchemeError("unexpected token");
        }
        Obj tail = parse_expr();
        if (!match(Token::RPAREN)) {
          throw SchemeError("expected ')' after dotted pair");
        }
        return list_from(elements, runtime, tail);
      }

      elements.push_back(parse_expr());
    }

    return list_from(elements, runtime);
  }

  Obj parse_expr() {
    assert(index < tokens.size());
    const auto &tok = tokens[index];
    index += 1;

    switch (tok.type) {
    case Token::LPAREN:
      return parse_list();

    case Token::QUOTE:
      return parse_quoted(runtime->intern("quote"));

    case Token::BACKTICK:
      return parse_quoted(runtime->intern("quasiquote"));

    case Token::COMMA:
      return parse_quoted(runtime->intern("unquote"));

    case Token::SPLICE_COMMA:
      return parse_quoted(runtime->intern("unquote-splicing"));

    case Token::TRUE:
      return true;

    case Token::FALSE:
      return false;

    case Token::PLUS_INF:
    case Token::MINUS_INF:
    case Token::NAN_VAL:
    case Token::NUMBER:
      return Number::parse(tok.lexeme, runtime);

    case Token::STRING: {
      auto str = process_escapes(tok.lexeme);
      return runtime->alloc<String>(std::move(str));
    }

    case Token::SYMBOL:
      return runtime->intern(tok.lexeme);

    case Token::VEC_BEGIN: {
      std::vector<Obj> elements;
      while (!match(Token::RPAREN)) {
        auto obj = parse_expr();
        elements.push_back(obj);
      }
      return runtime->alloc<Vector>(std::move(elements));
    }

    case Token::CHAR: {
      std::string_view name = tok.lexeme.substr(2);
      if (name == "space") {
        return ' ';
      } else if (name == "newline") {
        return '\n';
      } else if (name == "tab") {
        return '\t';
      } else if (name == "return") {
        return '\r';
      } else if (name.size() == 1) {
        return name.front();
      }
      throw SchemeError("unknown character name: " + std::string(name));
    }

    default:
      throw SchemeError("unexpected token");
    }
  }

public:
  Parser(const std::vector<Token> &tokens, Runtime *runtime)
      : tokens{tokens}, index{0}, runtime{runtime} {}

  Obj parse() {
    assert(!tokens.empty());
    return parse_expr();
  }
};

Obj parse(const std::vector<Token> &tokens, Runtime *runtime) {
  return Parser(tokens, runtime).parse();
}
