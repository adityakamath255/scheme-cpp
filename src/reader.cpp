#include "reader.hpp"

#include "eval.hpp"
#include "lexer.hpp"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string process_escapes(std::string_view raw) {
  std::string result;
  result.reserve(raw.size());

  for (size_t i = 0; i < raw.size(); i += 1) {
    if (raw[i] != '\\' || i + 1 >= raw.size()) {
      result += raw[i];
      continue;
    }

    i += 1;
    switch (raw[i]) {
    case 'n':
      result += '\n';
      break;
    case 't':
      result += '\t';
      break;
    case 'r':
      result += '\r';
      break;
    case '\\':
      result += '\\';
      break;
    case '"':
      result += '"';
      break;
    default:
      throw SchemeError("invalid escape sequence");
    }
  }

  return result;
}

class Reader {
  Lexer lexer;
  EvalContext &context;
  std::optional<Token> current;

  const Token *peek() {
    if (!current) {
      current = lexer.next();
    }
    return current ? &*current : nullptr;
  }

  Token take() {
    const Token *token = peek();
    if (!token) {
      throw Incomplete{};
    }
    Token result = *token;
    current.reset();
    return result;
  }

  bool match_closing(char expected) {
    const Token *token = peek();
    if (!token) {
      throw Incomplete{};
    }
    if (token->type != Token::Type::RPAREN) {
      return false;
    }
    if (token->delimiter != expected) {
      throw SchemeError("mismatched brackets");
    }
    take();
    return true;
  }

  void require_closing(char expected) {
    if (!match_closing(expected)) {
      throw SchemeError("expected ')' after dotted pair");
    }
  }

  Obj read_quoted(std::string_view name) {
    return list_from(
        std::array<Obj, 2>{context.intern(name), read_datum()}, context);
  }

  Obj read_list(char closing) {
    std::vector<Obj> elements;
    while (!match_closing(closing)) {
      const Token *token = peek();
      if (token->type == Token::Type::DOT) {
        take();
        if (elements.empty()) {
          throw SchemeError("unexpected token");
        }
        Obj tail = read_datum();
        require_closing(closing);
        return list_from(elements, context, tail);
      }
      elements.push_back(read_datum());
    }
    return list_from(elements, context);
  }

  Obj read_vector(char closing) {
    std::vector<Obj> elements;
    while (!match_closing(closing)) {
      elements.push_back(read_datum());
    }
    return context.alloc<Vector>(std::move(elements));
  }

  Obj read_char(std::string_view lexeme) {
    std::string_view name = lexeme.substr(2);
    if (name == "space") {
      return ' ';
    }
    if (name == "newline") {
      return '\n';
    }
    if (name == "tab") {
      return '\t';
    }
    if (name == "return") {
      return '\r';
    }
    if (name.size() == 1) {
      return name.front();
    }
    throw SchemeError("unknown character name: " + std::string(name));
  }

  Obj read_datum() {
    Token token = take();
    switch (token.type) {
    case Token::Type::LPAREN:
      return read_list(token.delimiter);
    case Token::Type::RPAREN:
      throw SchemeError("mismatched brackets");
    case Token::Type::QUOTE:
      return read_quoted("quote");
    case Token::Type::BACKTICK:
      return read_quoted("quasiquote");
    case Token::Type::COMMA:
      return read_quoted("unquote");
    case Token::Type::SPLICE_COMMA:
      return read_quoted("unquote-splicing");
    case Token::Type::TRUE:
      return true;
    case Token::Type::FALSE:
      return false;
    case Token::Type::PLUS_INF:
    case Token::Type::MINUS_INF:
    case Token::Type::NAN_VAL:
    case Token::Type::NUMBER:
      return Number::parse(token.lexeme, context);
    case Token::Type::STRING:
      return context.alloc<String>(process_escapes(token.lexeme));
    case Token::Type::SYMBOL:
      return context.intern(token.lexeme);
    case Token::Type::VEC_BEGIN:
      return read_vector(token.delimiter);
    case Token::Type::CHAR:
      return read_char(token.lexeme);
    case Token::Type::DOT:
      throw SchemeError("unexpected token");
    }
    std::unreachable();
  }

public:
  Reader(std::string_view source, EvalContext &context)
      : lexer{source}, context{context}, current{} {}

  std::optional<Obj> read() {
    if (!peek()) {
      return std::nullopt;
    }
    return read_datum();
  }

  std::string_view rest() const { return lexer.rest(); }
};

}

ReadOutcome read_one(std::string_view source, EvalContext &context) {
  try {
    Reader reader{source, context};
    if (auto value = reader.read()) {
      return ReadDatum{*value, reader.rest()};
    }
    return ReadEnd{reader.rest()};
  } catch (const Incomplete &) {
    return ReadIncomplete{};
  }
}
