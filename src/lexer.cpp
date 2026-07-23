#include "lexer.hpp"

#include "types.hpp"

#include <cassert>

namespace {

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

}

Lexer::Lexer(std::string_view input)
    : input{input}, start{0}, current{0} {}

bool Lexer::at_boundary() const {
  return current >= input.size() || is_boundary(input[current]);
}

bool Lexer::match(std::string_view word, bool require_boundary) {
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

void Lexer::skip_whitespace() {
  while (current < input.size() && is_space(input[current])) {
    current += 1;
  }
}

void Lexer::skip_semicolon_comment() {
  while (current < input.size() && input[current] != '\n') {
    current += 1;
  }
}

bool Lexer::skip_hash_comment() {
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

void Lexer::skip_whitespace_and_comments() {
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

Token Lexer::make_token(Token::Type type, char delimiter) {
  assert(current >= start);
  return {
      .type = type,
      .lexeme = input.substr(start, current - start),
      .delimiter = delimiter,
  };
}

Token Lexer::string_token() {
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

Token Lexer::symbol_token() {
  while (!at_boundary()) {
    current += 1;
  }
  return make_token(Token::Type::SYMBOL);
}

Token Lexer::number_token() {
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

Token Lexer::char_token() {
  if (current >= input.size()) {
    throw SchemeError("expected character after #\\");
  }
  current += 1;
  while (!at_boundary()) {
    current += 1;
  }
  return make_token(Token::Type::CHAR);
}

Token Lexer::hash_token() {
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

Token Lexer::next_token() {
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
      return make_token(Token::Type::NUMBER);
    }
    if (match("nan.0", true)) {
      return make_token(Token::Type::NUMBER);
    }
    return symbol_token();

  default:
    return symbol_token();
  }
}

std::optional<Token> Lexer::next() {
  skip_whitespace_and_comments();
  if (current >= input.size()) {
    return std::nullopt;
  }
  start = current;
  return next_token();
}

std::string_view Lexer::rest() const { return input.substr(current); }
