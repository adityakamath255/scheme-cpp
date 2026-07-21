#pragma once

#include "types.hpp"

#include <string_view>
#include <variant>

class EvalContext;

struct ReadDatum {
  Obj value;
  std::string_view rest;
};

struct ReadEnd {
  std::string_view rest;
};

struct ReadIncomplete {};

using ReadOutcome = std::variant<ReadDatum, ReadEnd, ReadIncomplete>;

ReadOutcome read_one(std::string_view source, EvalContext &context);
