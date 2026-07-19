#include "read.hpp"

#include "eval.hpp"

#include <array>
#include <cassert>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Incomplete {};

struct Token {
  enum class Type {
    LPAREN,
    RPAREN,
    NUMBER,
    SYMBOL,
    STRING,
    TRUE,
    FALSE,
    CHAR,
    DOT,
    QUOTE,
    BACKTICK,
    COMMA,
    SPLICE_COMMA,
    PLUS_INF,
    MINUS_INF,
    NAN_VAL,
    VEC_BEGIN,
  } type;

  std::string_view lexeme;
  char delimiter = '\0';
};

bool is_space(char c) {
  switch (c) {
  case ' ':
  case '\t':
  case '\r':
  case '\n':
    return true;
  default:
    return false;
  }
}

bool is_digit(char c) {
  switch (c) {
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    return true;
  default:
    return false;
  }
}

bool is_special(char c) {
  switch (c) {
  case '(':
  case ')':
  case '[':
  case ']':
  case '\'':
  case '"':
  case '`':
  case ',':
  case ';':
  case '#':
    return true;
  default:
    return false;
  }
}

bool is_boundary(char c) { return is_space(c) || is_special(c); }

class Lexer {
  std::string_view input;
  size_t start;
  size_t current;

  bool at_boundary() const {
    return current >= input.size() || is_boundary(input[current]);
  }

  bool match(std::string_view word, bool require_boundary) {
    if (current + word.size() > input.size()) {
      return false;
    }
    if (input.substr(current, word.size()) != word) {
      return false;
    }
    if (require_boundary && current + word.size() < input.size() &&
        !is_boundary(input[current + word.size()])) {
      return false;
    }
    current += word.size();
    return true;
  }

  void skip_whitespace() {
    while (current < input.size() && is_space(input[current])) {
      current += 1;
    }
  }

  void skip_semicolon_comment() {
    while (current < input.size() && input[current] != '\n') {
      current += 1;
    }
  }

  bool skip_hash_comment() {
    int depth = 1;
    while (current < input.size() && depth > 0) {
      if (match("#|", false)) {
        depth += 1;
      } else if (match("|#", false)) {
        depth -= 1;
      } else {
        current += 1;
      }
    }
    return depth == 0;
  }

  void skip_whitespace_and_comments() {
    while (true) {
      skip_whitespace();
      if (match(";", false)) {
        skip_semicolon_comment();
      } else if (match("#|", false)) {
        if (!skip_hash_comment()) {
          throw Incomplete{};
        }
      } else {
        return;
      }
    }
  }

  Token make_token(Token::Type type, char delimiter = '\0') {
    assert(current >= start);
    return {
        .type = type,
        .lexeme = input.substr(start, current - start),
        .delimiter = delimiter,
    };
  }

  Token string_token() {
    start += 1;
    while (current < input.size()) {
      if (input[current] == '\\') {
        if (current + 1 >= input.size()) {
          throw Incomplete{};
        }
        current += 2;
      } else if (input[current] == '"') {
        Token token = make_token(Token::Type::STRING);
        current += 1;
        return token;
      } else {
        current += 1;
      }
    }
    throw Incomplete{};
  }

  Token symbol_token() {
    while (!at_boundary()) {
      current += 1;
    }
    return make_token(Token::Type::SYMBOL);
  }

  Token number_token() {
    while (current < input.size() && is_digit(input[current])) {
      current += 1;
    }
    if (current < input.size() && input[current] == '.') {
      current += 1;
      while (current < input.size() && is_digit(input[current])) {
        current += 1;
      }
    }
    if (current < input.size() &&
        (input[current] == 'e' || input[current] == 'E')) {
      current += 1;
      if (current < input.size() &&
          (input[current] == '+' || input[current] == '-')) {
        current += 1;
      }
      while (current < input.size() && is_digit(input[current])) {
        current += 1;
      }
    }
    if (current < input.size() && !is_boundary(input[current])) {
      return symbol_token();
    }
    return make_token(Token::Type::NUMBER);
  }

  Token char_token() {
    if (current >= input.size()) {
      throw SchemeError("expected character after #\\");
    }
    current += 1;
    while (!at_boundary()) {
      current += 1;
    }
    return make_token(Token::Type::CHAR);
  }

  Token hash_token() {
    if (match("t", true) || match("T", true)) {
      return make_token(Token::Type::TRUE);
    }
    if (match("f", true) || match("F", true)) {
      return make_token(Token::Type::FALSE);
    }
    if (match("(", false)) {
      return make_token(Token::Type::VEC_BEGIN, ')');
    }
    if (match("\\", false)) {
      return char_token();
    }
    throw SchemeError("unidentified constant after '#'");
  }

  Token next_token() {
    char c = input[current];
    switch (c) {
    case '(':
    case '[':
      current += 1;
      return make_token(Token::Type::LPAREN, c == '(' ? ')' : ']');

    case ')':
    case ']':
      current += 1;
      return make_token(Token::Type::RPAREN, c);

    case '\'':
      current += 1;
      return make_token(Token::Type::QUOTE);

    case '`':
      current += 1;
      return make_token(Token::Type::BACKTICK);

    case ',':
      current += 1;
      return make_token(match("@", false) ? Token::Type::SPLICE_COMMA
                                          : Token::Type::COMMA);

    case '.':
      current += 1;
      return at_boundary() ? make_token(Token::Type::DOT) : number_token();

    case '#':
      current += 1;
      return hash_token();

    case '"':
      current += 1;
      return string_token();

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      current += 1;
      return number_token();

    case '+':
    case '-':
      current += 1;
      if (current < input.size() &&
          (is_digit(input[current]) || input[current] == '.')) {
        return number_token();
      }
      if (match("inf.0", true)) {
        return make_token(c == '+' ? Token::Type::PLUS_INF
                                   : Token::Type::MINUS_INF);
      }
      if (match("nan.0", true)) {
        return make_token(Token::Type::NAN_VAL);
      }
      return symbol_token();

    default:
      return symbol_token();
    }
  }

public:
  explicit Lexer(std::string_view input)
      : input{input}, start{0}, current{0} {}

  std::optional<Token> next() {
    skip_whitespace_and_comments();
    if (current >= input.size()) {
      return std::nullopt;
    }
    start = current;
    return next_token();
  }

  std::string_view rest() const { return input.substr(current); }
};

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
