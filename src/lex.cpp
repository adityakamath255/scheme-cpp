#include "lex.hpp"
#include <cassert>
#include <stdexcept>

static bool is_space(char c) {
  switch (c) {
    case ' ': case '\t': case '\r': case '\n':
      return true;
    default:
      return false;
  }
}

static bool is_digit(char c) {
  switch (c) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return true;
    default:
      return false;
  }
}

static bool is_special(char c) {
  switch (c) {
    case '(': case ')': case '[': case ']':
    case '\'': case '"': case '`': case ',':
    case ';': case '#':
      return true;
    default:
      return false;
  }
}

static bool is_boundary(char c) {
  return is_space(c) || is_special(c);
}

static bool is_prefix(Token::Type t) {
  return (
    t == Token::QUOTE 
    || t == Token::BACKTICK
    || t == Token::COMMA
    || t == Token::SPLICE_COMMA
  );
}

class Lexer {
  std::string_view input;
  size_t start;
  size_t curr;
  std::vector<char> bracket_stack;

  bool at_boundary() {
    return curr >= input.size() || is_boundary(input[curr]);
  }

  bool match(std::string_view word, bool require_boundary) {
    if (curr + word.size() > input.size()) {
      return false;
    }
    else if (input.substr(curr, word.size()) != word) {
      return false;
    }
    else if (
      require_boundary
      && curr + word.size() < input.size()
      && !is_boundary(input[curr + word.size()])
    ) {
      return false;
    }
    else {
      curr += word.size();
      return true;
    }
  }

  void skip_whitespace() {
    while (curr < input.size() && is_space(input[curr])) {
      curr += 1;
    }
  }

  void skip_semicolon_comment() {
    while (curr < input.size() && input[curr] != '\n') {
      curr += 1;
    }
  }

  // returns false if comment is not closed
  bool skip_hash_comment() {
    int depth = 1;
    while (curr < input.size() && depth > 0) {
      if (match("#|", false)) {
        depth += 1;
      }
      else if (match("|#", false)) {
        depth -= 1;
      }
      else {
        curr += 1;
      }
    }
    return depth == 0;
  }

  bool skip_whitespace_and_comments() {
    while (true) {
      size_t old_curr = curr;
      skip_whitespace();

      if (match(";", false)) {
        skip_semicolon_comment();
      }

      if (match("#|", false)) {
        if (!skip_hash_comment()) {
          return false;
        }
      }
      if (curr == old_curr) {
        return true;
      }
    }
  }

  Token make_token(Token::Type type) {
    assert(curr >= start);
    Token res {
      .type = type,
      .lexeme = input.substr(start, curr - start)
    };
    start = curr;
    return res;
  }

  Token hash_token() {
    if (match("t", true) || match("T", true)) {
      return make_token(Token::TRUE);
    }
    else if (match("f", true) || match("F", true)) {
      return make_token(Token::FALSE);
    }
    else {
      throw std::runtime_error("unidentified constant after '#'");
    }
  }

  // returns nullopt if string is unterminated
  std::optional<Token> string_token() {
    start += 1;
    while (curr < input.size()) {
      if (input[curr] == '\\') {
        if (curr + 1 < input.size()) {
          curr += 2;
        }
        else {
          return std::nullopt;
        }
      }
      else if (input[curr] == '"') {
        auto res = make_token(Token::STRING);
        curr += 1;
        return res;
      }
      else {
        curr += 1;
      }
    }
    return std::nullopt;
  }

  Token symbol_token() {
    while (!at_boundary()) {
      curr += 1;
    }
    return make_token(Token::SYMBOL);
  }

  Token number_token() {
    while (curr < input.size() && is_digit(input[curr])) {
      curr += 1;
    }
    if (curr < input.size() && input[curr] == '.') {
      curr += 1;
      while (curr < input.size() && is_digit(input[curr])) {
        curr += 1;
      }
    }
    if (curr < input.size() && (input[curr] == 'e' || input[curr] == 'E')) {
      curr += 1;
      if (curr < input.size() && (input[curr] == '+' || input[curr] == '-')) {
        curr += 1;
      }
      while (curr < input.size() && is_digit(input[curr])) {
        curr += 1;
      }
    }
    if (curr < input.size() && !is_boundary(input[curr])) {
      return symbol_token();
    }
    else {
      return make_token(Token::NUMBER);
    }
  }

  std::optional<Token> next_token() {
    char c = input[curr];

    switch (c) {
      case '(': case '[':
        curr += 1;
        bracket_stack.push_back(c == '(' ? ')' : ']');
        return make_token(Token::LPAREN);

      case ')': case ']':
        curr += 1;
        if (bracket_stack.empty() || bracket_stack.back() != c) {
          throw std::runtime_error("mismatched brackets");
        }
        bracket_stack.pop_back();
        return make_token(Token::RPAREN);

      case '\'':
        curr += 1;
        return make_token(Token::QUOTE);

      case '`':
        curr += 1;
        return make_token(Token::BACKTICK);

      case ',':
        curr += 1;
        if (match("@", false)) {
          return make_token(Token::SPLICE_COMMA);
        }
        else {
          return make_token(Token::COMMA);
        }

      case '.':
        curr += 1;
        if (at_boundary()) {
          return make_token(Token::DOT);
        }
        else {
          return number_token();
        }

      case '#':
        curr += 1;
        return hash_token();

      case '"':
        curr += 1;
        return string_token();

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        curr += 1;
        return number_token();
      case '+': case '-':
        curr += 1;
        if (curr < input.size() && (is_digit(input[curr]) || input[curr] == '.')) {
          return number_token();
        }
        else if (match("inf.0", true)) {
          return make_token(
            c == '+'
            ? Token::PLUS_INF
            : Token::MINUS_INF
          );
        }
        else if (match("nan.0", true)) {
          return make_token(Token::NAN_VAL);
        }
        else {
          return symbol_token();
        }

      default:
        return symbol_token();
    }
  }

public:
  Lexer(std::string_view input):
    input {input},
    start {0},
    curr {0},
    bracket_stack {}
  {}

  std::optional<LexResult> lex() {
    std::vector<Token> tokens;

    while (curr < input.size()) {
      if (!skip_whitespace_and_comments()) {
        return std::nullopt;
      }

      if (curr >= input.size()) {
        break;
      }

      start = curr;

      if (auto token_opt = next_token()) {
        tokens.push_back(*token_opt);
        if (
          bracket_stack.empty()
          && !tokens.empty()
          && !is_prefix(token_opt->type)
        ) {
          break;
        }
      }

      else {
        return std::nullopt;
      }
    }

    if (
      !bracket_stack.empty()
      || (!tokens.empty() && is_prefix(tokens.back().type))
    ) {
      return std::nullopt;
    }
    else {
      return LexResult {
        .tokens = tokens,
        .rest = input.substr(curr)
      };
    }
  }
};

std::optional<LexResult> lex(std::string_view input) {
  return Lexer(input).lex();
}
