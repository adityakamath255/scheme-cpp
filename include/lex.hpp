#pragma once
#include <optional>
#include <string_view>
#include <vector>

struct Token {
  enum Type {
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
};

struct LexResult {
  std::vector<Token> tokens;
  std::string_view rest;
};

std::optional<LexResult> lex(std::string_view input);
