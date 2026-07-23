#include "scheme/session.hpp"
#include <replxx.hxx>
#include <exception>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <unistd.h>

static void print_repl_event(const scheme::Event &event) {
  if (auto *output = std::get_if<scheme::Output>(&event)) {
    std::cout << output->text;
  }
  else {
    std::cout << std::get<scheme::Result>(event).text << "\n";
  }
}

static void print_batch_event(const scheme::Event &event) {
  if (auto *output = std::get_if<scheme::Output>(&event)) {
    std::cout << output->text;
  }
}

static std::optional<int> execute_batch(std::istream &input,
                                        scheme::Session &session) {
  std::ostringstream buffer;
  buffer << input.rdbuf();

  try {
    session.execute(buffer.str(), print_batch_event);
    return std::nullopt;
  }
  catch (const scheme::ExitRequest &request) {
    return request.status();
  }
  catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
}

static void repl(scheme::Session &session) {
  replxx::Replxx rx;
  rx.set_max_history_size(1024);
  rx.set_word_break_characters(" \t\r\n()[]'\";");

  std::string input;
  bool incomplete = false;

  rx.bind_key(
    replxx::Replxx::KEY::control('C'),
    [&rx, &input, &incomplete](char32_t code) {
      rx.invoke(replxx::Replxx::ACTION::CLEAR_SELF, code);
      input.clear();
      incomplete = false;
      return replxx::Replxx::ACTION_RESULT::RETURN;
    }
  );

  while (true) {
    try {
      auto result = session.run(input, print_repl_event);
      input.erase(0, result.consumed);
      incomplete = result.incomplete;
      if (!incomplete) {
        input.clear();
      }
    }
    catch (const scheme::ExitRequest &) {
      throw;
    }
    catch (const std::exception &e) {
      std::cerr << "error: " << e.what() << "\n";
      input.clear();
      incomplete = false;
    }

    char const *line = rx.input(incomplete ? ".. " : ">> ");
    if (!line) {
      break;
    }

    if (*line) {
      rx.history_add(line);
    }

    if (!input.empty()) {
      input += '\n';
    }
    input += line;
  }

  std::cout << "\n";
}

int main(int argc, char *argv[]) {
  bool interactive = false;
  std::optional<std::string> filename;

  for (int i = 1; i < argc; i += 1) {
    std::string_view arg = argv[i];
    if (arg == "-i" || arg == "--interactive") {
      interactive = true;
    }
    else if (!filename) {
      filename = std::string(arg);
    }
    else {
      std::cerr << "usage: scheme [-i] [file]\n";
      return 1;
    }
  }

  scheme::Session session;

  if (filename) {
    std::ifstream file(*filename);
    if (!file) {
      std::cerr << "error: could not open " << *filename << "\n";
      return 1;
    }
    if (auto status = execute_batch(file, session)) {
      return *status;
    }
  }
  else if (!isatty(STDIN_FILENO)) {
    if (auto status = execute_batch(std::cin, session)) {
      return *status;
    }
  }

  if (interactive || (!filename && isatty(STDIN_FILENO))) {
    std::cout << "Scheme - Ctrl+D to exit, Ctrl+C to clear\n\n";
    try {
      repl(session);
    }
    catch (const scheme::ExitRequest &request) {
      return request.status();
    }
  }
}
