#pragma once
#include "types.hpp"
#include <string_view>
#include <variant>

class Ctx;

struct Evaluated  { Obj value; std::string output; std::string_view rest; };
struct Exhausted  {};
struct Incomplete {};
using ReadEval = std::variant<Evaluated, Exhausted, Incomplete>;

ReadEval read_eval(std::string_view source, Ctx *ctx);
void run_all(std::string_view source, Ctx *ctx);
