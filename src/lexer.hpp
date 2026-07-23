#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

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
    VEC_BEGIN,
  } type;

  std::string_view lexeme;
  char delimiter = '\0';
};

class Lexer {
  std::string_view input;
  size_t start;
  size_t current;

  bool at_boundary() const;
  bool match(std::string_view word, bool require_boundary);
  void skip_whitespace();
  void skip_semicolon_comment();
  bool skip_hash_comment();
  void skip_whitespace_and_comments();
  Token make_token(Token::Type type, char delimiter = '\0');
  Token string_token();
  Token symbol_token();
  Token number_token();
  Token char_token();
  Token hash_token();
  Token next_token();

public:
  explicit Lexer(std::string_view input);

  std::optional<Token> next();
  std::string_view rest() const;
};
