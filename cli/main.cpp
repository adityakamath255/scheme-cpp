#include "ctx.hpp"
#include "builtins.hpp"
#include "preamble.hpp"
#include "driver.hpp"
#include <replxx.hxx>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>

static void repl(Ctx *ctx) {
  replxx::Replxx rx;
  rx.set_max_history_size(1024);
  rx.set_word_break_characters(" \t\r\n()[]'\";");

  std::string input;

  rx.bind_key(replxx::Replxx::KEY::control('C'), [&rx, &input](char32_t code) {
    rx.invoke(replxx::Replxx::ACTION::CLEAR_SELF, code);
    input.clear();
    return replxx::Replxx::ACTION_RESULT::RETURN;
  });

  while (true) {
    try {
      ReadEval r = read_eval(input, ctx);
      if (auto *e = std::get_if<Evaluated>(&r)) {
        std::string rest(e->rest);
        if (!e->value.is_void()) {
          std::cout << e->value.stringify(true) << "\n";
        }
        input = rest;
        continue;
      }
      if (std::holds_alternative<Exhausted>(r)) {
        input.clear();
      }
    }
    catch (const std::exception &e) {
      std::cerr << "error: " << e.what() << "\n";
      input.clear();
    }

    char const *line = rx.input(input.empty() ? ">> " : ".. ");
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

  Ctx ctx;
  install_builtins(&ctx);
  run_all(preamble, &ctx);

  if (filename) {
    std::ifstream file(*filename);
    if (!file) {
      std::cerr << "error: could not open " << *filename << "\n";
      return 1;
    }
    std::ostringstream buf;
    buf << file.rdbuf();

    try {
      run_all(buf.str(), &ctx);
    }
    catch (const std::exception &e) {
      std::cerr << "error: " << e.what() << "\n";
      return 1;
    }
  }
  else if (!isatty(STDIN_FILENO)) {
    std::ostringstream buf;
    buf << std::cin.rdbuf();

    try {
      run_all(buf.str(), &ctx);
    }
    catch (const std::exception &e) {
      std::cerr << "error: " << e.what() << "\n";
      return 1;
    }
  }

  if (interactive || (!filename && isatty(STDIN_FILENO))) {
    std::cout << "Scheme - Ctrl+D to exit, Ctrl+C to clear\n\n";
    repl(&ctx);
  }
}
