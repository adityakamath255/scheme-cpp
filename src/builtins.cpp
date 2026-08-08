#include "builtins.hpp"

#include "arity.hpp"
#include "ctx.hpp"
#include "errors.hpp"
#include "reader.hpp"

#include <algorithm>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <ranges>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <utility>

namespace {

using Args = const std::vector<Obj> &;

template <typename T>
T decode_object(Obj obj, T (Obj::*accessor)() const) {
  try {
    return (obj.*accessor)();
  } catch (const SchemeError &error) {
    throw UnattributedError(error.what());
  }
}

namespace arg {

Obj any(Obj obj) { return obj; }

Number number(Obj obj) {
  return decode_object(obj, &Obj::as_number);
}

char character(Obj obj) {
  return decode_object(obj, &Obj::as_char);
}

Symbol symbol(Obj obj) {
  return decode_object(obj, &Obj::as_symbol);
}

String *string(Obj obj) {
  return decode_object(obj, &Obj::as_string);
}

Cons *pair(Obj obj) {
  return decode_object(obj, &Obj::as_cons);
}

Vector *vector(Obj obj) {
  return decode_object(obj, &Obj::as_vector);
}

Error *error(Obj obj) {
  return decode_object(obj, &Obj::as_error);
}

size_t index(Obj obj) {
  auto value = number(obj).to_size();
  if (!value) {
    throw UnattributedError("expected non-negative integer");
  }
  return *value;
}

}

template <typename D>
concept Decoder = std::invocable<const D &, Obj>;

template <Decoder D>
using Decoded = std::invoke_result_t<const D &, Obj>;

void require_arity(Args raw, Arity arity) {
  if (auto error = arity.mismatch(raw.size())) {
    throw UnattributedError(*error);
  }
}

void match(Args raw) {
  require_arity(raw, Arity::exactly(0));
}

template <Decoder D>
auto match(Args raw, D decoder) -> Decoded<D> {
  require_arity(raw, Arity::exactly(1));
  return decoder(raw.front());
}

template <Decoder A, Decoder B>
auto match(Args raw, A first, B second) {
  require_arity(raw, Arity::exactly(2));
  return std::pair{first(raw[0]), second(raw[1])};
}

template <Decoder A, Decoder B, Decoder C>
auto match(Args raw, A first, B second, C third) {
  require_arity(raw, Arity::exactly(3));
  return std::tuple{first(raw[0]), second(raw[1]), third(raw[2])};
}

template <Decoder D>
auto match_optional(Args raw, D decoder) {
  require_arity(raw, Arity::between(0, 1));
  return raw.size() == 1
   ? std::optional<Decoded<D>>{decoder(raw.front())}
   : std::nullopt;
}

template <Decoder A, Decoder D>
auto match_optional(Args raw, A required, D optional) {
  require_arity(raw, Arity::between(1, 2));
  return std::pair{
    required(raw[0]),
    raw.size() == 2 
      ? std::optional<Decoded<D>>{optional(raw[1])}
      : std::nullopt
  };
}

template <Decoder A, Decoder B, Decoder D>
auto match_optional(Args raw, A first, B second, D optional) {
  require_arity(raw, Arity::between(2, 3));
  return std::tuple{
    first(raw[0]), 
    second(raw[1]),
    raw.size() == 3 
      ? std::optional<Decoded<D>>{optional(raw[2])}
      : std::nullopt
  };
}

template <Decoder D>
auto match_many(Args raw, D decoder) {
  return raw 
    | std::views::transform(decoder) 
    | std::ranges::to<std::vector<Decoded<D>>>();
}

template <Decoder D>
auto match_nonempty(Args raw, D decoder) {
  require_arity(raw, Arity::at_least(1));
  return std::pair{
    decoder(raw.front()),
    raw 
      | std::views::drop(1) 
      | std::views::transform(decoder) 
      | std::ranges::to<std::vector<Decoded<D>>>()
  };
}

template <typename Implementation>
  requires std::is_empty_v<Implementation> &&
           std::copy_constructible<Implementation> &&
           std::is_invocable_r_v<Obj, Implementation, Args, Ctx &>
void install(Ctx &ctx, std::string_view name, Implementation implementation) {
  auto adapter = [name = std::string{name}, implementation](
                     Args raw, Ctx &ctx) -> Obj {
    try {
      return implementation(raw, ctx);
    } catch (const UnattributedError &error) {
      throw SchemeError(std::format("{}: {}", name, error.what()));
    }
  };
  ctx.install_builtin(name, Builtin::Fn{std::move(adapter)});
}

}

template <typename Predicate>
static bool numeric_compare(Number first, const std::vector<Number> &rest,
                            Predicate predicate) {
  Number previous = first;
  for (Number number : rest) {
    if (!predicate(previous.compare(number))) {
      return false;
    }
    previous = number;
  }
  return true;
}

static Number minmax(Number first, const std::vector<Number> &rest,
                     std::partial_ordering wanted) {
  bool inexact = !first.is_exact();
  Number best = first;
  for (Number number : rest) {
    inexact = inexact || !number.is_exact();
    if (number.compare(best) == wanted) {
      best = number;
    }
  }
  return inexact ? best.to_inexact() : best;
}

static void require_index(size_t size, size_t index) {
  if (index >= size) {
    throw UnattributedError("index out of range");
  }
}

template <auto Predicate>
static void install_predicate(Ctx &ctx, std::string_view name) {
  install(ctx, name, [](Args raw, Ctx &) {
    return (match(raw, arg::any).*Predicate)();
  });
}

static void install_numbers(Ctx &ctx) {
  install(ctx, "+", [](Args raw, Ctx &ctx) {
    auto numbers = match_many(raw, arg::number);
    return std::ranges::fold_left(
        numbers, Number::exact(0, ctx),
        [&ctx](Number sum, Number number) {
          return sum.add(number, ctx);
        });
  });

  install(ctx, "-", [](Args raw, Ctx &ctx) {
    auto [first, remaining] = match_nonempty(raw, arg::number);
    if (remaining.empty()) {
      return first.neg(ctx);
    }
    return std::ranges::fold_left(
        remaining, first, [&ctx](Number difference, Number number) {
          return difference.sub(number, ctx);
        });
  });

  install(ctx, "*", [](Args raw, Ctx &ctx) {
    auto numbers = match_many(raw, arg::number);
    return std::ranges::fold_left(
        numbers, Number::exact(1, ctx),
        [&ctx](Number product, Number number) {
          return product.mul(number, ctx);
        });
  });

  install(ctx, "/", [](Args raw, Ctx &ctx) {
    auto [first, remaining] = match_nonempty(raw, arg::number);
    if (remaining.empty()) {
      return Number::exact(1, ctx).div(first, ctx);
    }
    return std::ranges::fold_left(
        remaining, first, [&ctx](Number quotient, Number number) {
          return quotient.div(number, ctx);
        });
  });

  install(ctx, "<", [](Args raw, Ctx &) {
    auto [first, remaining] = match_nonempty(raw, arg::number);
    return numeric_compare(first, remaining, [](std::partial_ordering order) {
      return order == std::partial_ordering::less;
    });
  });
  install(ctx, ">", [](Args raw, Ctx &) {
    auto [first, remaining] = match_nonempty(raw, arg::number);
    return numeric_compare(first, remaining, [](std::partial_ordering order) {
      return order == std::partial_ordering::greater;
    });
  });
  install(ctx, "=", [](Args raw, Ctx &) {
    auto [first, remaining] = match_nonempty(raw, arg::number);
    return numeric_compare(first, remaining, [](std::partial_ordering order) {
      return order == std::partial_ordering::equivalent;
    });
  });
  install(ctx, "<=", [](Args raw, Ctx &) {
    auto [first, remaining] = match_nonempty(raw, arg::number);
    return numeric_compare(first, remaining, [](std::partial_ordering order) {
      return order == std::partial_ordering::less ||
             order == std::partial_ordering::equivalent;
    });
  });
  install(ctx, ">=", [](Args raw, Ctx &) {
    auto [first, remaining] = match_nonempty(raw, arg::number);
    return numeric_compare(first, remaining, [](std::partial_ordering order) {
      return order == std::partial_ordering::greater ||
             order == std::partial_ordering::equivalent;
    });
  });

  install(ctx, "abs", [](Args raw, Ctx &ctx) {
    return match(raw, arg::number).abs(ctx);
  });
  install(ctx, "sqrt", [](Args raw, Ctx &ctx) {
    return match(raw, arg::number).sqrt(ctx);
  });
  install(ctx, "sin", [](Args raw, Ctx &) {
    return std::sin(match(raw, arg::number).to_double());
  });
  install(ctx, "cos", [](Args raw, Ctx &) {
    return std::cos(match(raw, arg::number).to_double());
  });
  install(ctx, "log", [](Args raw, Ctx &) {
    return std::log(match(raw, arg::number).to_double());
  });
  install(ctx, "expt", [](Args raw, Ctx &ctx) {
    auto [base, power] = match(raw, arg::number, arg::number);
    return base.expt(power, ctx);
  });

  install(ctx, "ceiling", [](Args raw, Ctx &) {
    Number number = match(raw, arg::number);
    return number.is_exact()
               ? number
               : Number::inexact(std::ceil(number.to_double()));
  });
  install(ctx, "floor", [](Args raw, Ctx &) {
    Number number = match(raw, arg::number);
    return number.is_exact()
               ? number
               : Number::inexact(std::floor(number.to_double()));
  });
  install(ctx, "round", [](Args raw, Ctx &) {
    Number number = match(raw, arg::number);
    return number.is_exact()
               ? number
               : Number::inexact(std::round(number.to_double()));
  });

  install(ctx, "max", [](Args raw, Ctx &) {
    auto [first, remaining] = match_nonempty(raw, arg::number);
    return minmax(first, remaining, std::partial_ordering::greater);
  });
  install(ctx, "min", [](Args raw, Ctx &) {
    auto [first, remaining] = match_nonempty(raw, arg::number);
    return minmax(first, remaining, std::partial_ordering::less);
  });

  install(ctx, "quotient", [](Args raw, Ctx &ctx) {
    auto [dividend, divisor] = match(raw, arg::number, arg::number);
    return dividend.quotient(divisor, ctx);
  });
  install(ctx, "remainder", [](Args raw, Ctx &ctx) {
    auto [dividend, divisor] = match(raw, arg::number, arg::number);
    return dividend.remainder(divisor, ctx);
  });
  install(ctx, "modulo", [](Args raw, Ctx &ctx) {
    auto [dividend, divisor] = match(raw, arg::number, arg::number);
    return dividend.modulo(divisor, ctx);
  });

  install(ctx, "even?", [](Args raw, Ctx &) {
    return match(raw, arg::number).is_even();
  });
  install(ctx, "odd?", [](Args raw, Ctx &) {
    return !match(raw, arg::number).is_even();
  });
  install(ctx, "zero?", [](Args raw, Ctx &) {
    return match(raw, arg::number).is_zero();
  });
  install(ctx, "positive?", [](Args raw, Ctx &ctx) {
    return match(raw, arg::number).compare(Number::exact(0, ctx)) ==
           std::partial_ordering::greater;
  });
  install(ctx, "negative?", [](Args raw, Ctx &ctx) {
    return match(raw, arg::number).compare(Number::exact(0, ctx)) ==
           std::partial_ordering::less;
  });
  install(ctx, "exact?", [](Args raw, Ctx &) {
    return match(raw, arg::number).is_exact();
  });
  install(ctx, "inexact?", [](Args raw, Ctx &) {
    return !match(raw, arg::number).is_exact();
  });

  auto exact = [](Args raw, Ctx &ctx) {
    return match(raw, arg::number).to_exact(ctx);
  };
  install(ctx, "exact", exact);
  install(ctx, "inexact->exact", exact);

  auto inexact = [](Args raw, Ctx &) {
    return match(raw, arg::number).to_inexact();
  };
  install(ctx, "inexact", inexact);
  install(ctx, "exact->inexact", inexact);
}

static void install_objects(Ctx &ctx) {
  install_predicate<&Obj::is_null>(ctx, "null?");
  install_predicate<&Obj::is_bool>(ctx, "boolean?");
  install_predicate<&Obj::is_number>(ctx, "number?");
  install(ctx, "integer?", [](Args raw, Ctx &) {
    Obj value = match(raw, arg::any);
    auto number = value.try_as_number();
    return number && number->is_integer();
  });
  install_predicate<&Obj::is_cons>(ctx, "pair?");
  install_predicate<&Obj::is_symbol>(ctx, "symbol?");
  install_predicate<&Obj::is_string>(ctx, "string?");
  install(ctx, "procedure?", [](Args raw, Ctx &) {
    Obj value = match(raw, arg::any);
    return value.is_procedure() || value.is_builtin();
  });
  install_predicate<&Obj::is_list>(ctx, "list?");
  install_predicate<&Obj::is_void>(ctx, "void?");
  install_predicate<&Obj::is_promise>(ctx, "promise?");
  install_predicate<&Obj::is_char>(ctx, "char?");
  install_predicate<&Obj::is_vector>(ctx, "vector?");
  install_predicate<&Obj::is_error>(ctx, "error-object?");
  install(ctx, "not", [](Args raw, Ctx &) {
    return match(raw, arg::any).is_false();
  });
  install(ctx, "void", [](Args raw, Ctx &) {
    match(raw);
    return Void{};
  });

  auto eq = [](Args raw, Ctx &) {
    auto [a, b] = match(raw, arg::any, arg::any);
    return a.eqv(b);
  };
  install(ctx, "eq?", eq);
  install(ctx, "eqv?", eq);

  install(ctx, "equal?", [](Args raw, Ctx &) {
    auto [a, b] = match(raw, arg::any, arg::any);
    return a.equals(b);
  });
}

static void install_lists(Ctx &ctx) {
  install(ctx, "car", [](Args raw, Ctx &) {
    return match(raw, arg::pair)->car;
  });
  install(ctx, "cdr", [](Args raw, Ctx &) {
    return match(raw, arg::pair)->cdr;
  });
  install(ctx, "cons", [](Args raw, Ctx &ctx) {
    auto [car, cdr] = match(raw, arg::any, arg::any);
    return ctx.alloc<Cons>(car, cdr);
  });
  install(ctx, "list", [](Args raw, Ctx &ctx) {
    auto values = match_many(raw, arg::any);
    return list_from(values, ctx);
  });
  install(ctx, "length", [](Args raw, Ctx &ctx) -> Obj {
    Obj list = match(raw, arg::any);
    if (!list.is_null() && !list.is_cons()) {
      throw UnattributedError("expected list, got " + list.type_name());
    }
    List parts{list};
    if (!parts.proper()) {
      throw UnattributedError("expected proper list");
    }
    return Number::exact(
        static_cast<int64_t>(parts.elements.size()), ctx);
  });
  install(ctx, "list-ref", [](Args raw, Ctx &) -> Obj {
    auto [pair, index] = match(raw, arg::pair, arg::index);
    List list{pair};
    require_index(list.elements.size(), index);
    return list.elements[index];
  });
  install(ctx, "set-car!", [](Args raw, Ctx &) {
    auto [pair, value] = match(raw, arg::pair, arg::any);
    pair->car = value;
    return Void{};
  });
  install(ctx, "set-cdr!", [](Args raw, Ctx &) {
    auto [pair, value] = match(raw, arg::pair, arg::any);
    pair->cdr = value;
    return Void{};
  });
}

static void install_strings(Ctx &ctx) {
  install(ctx, "string-length", [](Args raw, Ctx &ctx) {
    auto string = match(raw, arg::string);
    return Number::exact(static_cast<int64_t>(string->data.size()), ctx);
  });
  install(ctx, "string-ref", [](Args raw, Ctx &) -> Obj {
    auto [string, index] = match(raw, arg::string, arg::index);
    require_index(string->data.size(), index);
    return string->data[index];
  });
  install(ctx, "substring", [](Args raw, Ctx &ctx) -> Obj {
    auto [string, start, requested_end] =
        match_optional(raw, arg::string, arg::index, arg::index);
    size_t end = requested_end.value_or(string->data.size());
    if (start > end || end > string->data.size()) {
      throw UnattributedError("index out of range");
    }
    return ctx.alloc<String>(string->data.substr(start, end - start));
  });
  install(ctx, "string-append", [](Args raw, Ctx &ctx) {
    auto strings = match_many(raw, arg::string);
    return ctx.alloc<String>(std::ranges::to<std::string>(
        strings | std::views::transform([](String *string)
                                            -> const std::string & {
          return string->data;
        }) |
        std::views::join));
  });
  install(ctx, "string=?", [](Args raw, Ctx &) {
    auto [a, b] = match(raw, arg::string, arg::string);
    return a->data == b->data;
  });

  install(ctx, "char=?", [](Args raw, Ctx &) {
    auto [a, b] = match(raw, arg::character, arg::character);
    return a == b;
  });
  install(ctx, "char->integer", [](Args raw, Ctx &ctx) {
    auto character =
        static_cast<unsigned char>(match(raw, arg::character));
    return Number::exact(static_cast<int64_t>(character), ctx);
  });
  install(ctx, "integer->char", [](Args raw, Ctx &) -> Obj {
    size_t value = match(raw, arg::index);
    if (value > std::numeric_limits<unsigned char>::max()) {
      throw UnattributedError("value out of range");
    }
    return static_cast<char>(static_cast<unsigned char>(value));
  });
  install(ctx, "string->list", [](Args raw, Ctx &ctx) {
    return list_from(match(raw, arg::string)->data, ctx);
  });
  install(ctx, "list->string", [](Args raw, Ctx &ctx) -> Obj {
    List list{match(raw, arg::any)};
    if (!list.proper()) {
      throw UnattributedError("expected proper list");
    }
    return ctx.alloc<String>(std::ranges::to<std::string>(
        list.elements | std::views::transform([](Obj value) {
          return arg::character(value);
        })));
  });

  install(ctx, "number->string", [](Args raw, Ctx &ctx) {
    return ctx.alloc<String>(match(raw, arg::number).to_string());
  });
  install(ctx, "string->number", [](Args raw, Ctx &ctx) -> Obj {
    auto string = match(raw, arg::string);
    try {
      return Number::parse(string->data, ctx);
    } catch (const SchemeError &) {
      return false;
    }
  });
  install(ctx, "symbol->string", [](Args raw, Ctx &ctx) {
    return ctx.alloc<String>(match(raw, arg::symbol).name());
  });
  install(ctx, "string->symbol", [](Args raw, Ctx &ctx) {
    return ctx.intern(match(raw, arg::string)->data);
  });
}

static void install_io(Ctx &ctx) {
  install(ctx, "display", [](Args raw, Ctx &ctx) {
    ctx.output(match(raw, arg::any).to_display());
    return Void{};
  });
  install(ctx, "write", [](Args raw, Ctx &ctx) {
    ctx.output(match(raw, arg::any).to_write());
    return Void{};
  });
  install(ctx, "newline", [](Args raw, Ctx &ctx) {
    match(raw);
    ctx.output("\n");
    return Void{};
  });
  install(ctx, "read", [](Args raw, Ctx &ctx) -> Obj {
    match(raw);
    std::string input;
    while (true) {
      std::string line;
      if (!std::getline(std::cin, line)) {
        throw UnattributedError("unexpected end of input");
      }
      if (!input.empty()) {
        input += '\n';
      }
      input += line;

      ReadOutcome outcome = read_one(input, ctx);
      if (auto *datum = std::get_if<ReadDatum>(&outcome)) {
        return datum->value;
      }
    }
  });
}

static void install_vectors(Ctx &ctx) {
  install(ctx, "vector", [](Args raw, Ctx &ctx) {
    auto values = match_many(raw, arg::any);
    return ctx.alloc<Vector>(std::move(values));
  });
  install(ctx, "make-vector", [](Args raw, Ctx &ctx) {
    auto [size, requested_fill] =
        match_optional(raw, arg::index, arg::any);
    Obj fill =
        requested_fill.value_or(Obj(Number::exact(0, ctx)));
    return ctx.alloc<Vector>(std::vector<Obj>(size, fill));
  });
  install(ctx, "vector-ref", [](Args raw, Ctx &) -> Obj {
    auto [vector, index] = match(raw, arg::vector, arg::index);
    require_index(vector->data.size(), index);
    return vector->data[index];
  });
  install(ctx, "vector-set!", [](Args raw, Ctx &) {
    auto [vector, index, value] =
        match(raw, arg::vector, arg::index, arg::any);
    require_index(vector->data.size(), index);
    vector->data[index] = value;
    return Void{};
  });
  install(ctx, "vector-length", [](Args raw, Ctx &ctx) {
    auto vector = match(raw, arg::vector);
    return Number::exact(static_cast<int64_t>(vector->data.size()), ctx);
  });
  install(ctx, "vector->list", [](Args raw, Ctx &ctx) {
    return list_from(match(raw, arg::vector)->data, ctx);
  });
  install(ctx, "list->vector", [](Args raw, Ctx &ctx) -> Obj {
    List list{match(raw, arg::any)};
    if (!list.proper()) {
      throw UnattributedError("expected proper list");
    }
    return ctx.alloc<Vector>(std::move(list.elements));
  });
}

static void install_other(Ctx &ctx) {
  install(ctx, "force", [](Args raw, Ctx &ctx) {
    Obj value = match(raw, arg::any);
    if (Promise *promise = value.try_as_promise()) {
      return promise->force(ctx);
    }
    return value;
  });
  install(ctx, "error", [](Args raw, Ctx &ctx) -> Obj {
    auto [message, irritants] = match_nonempty(raw, arg::any);
    auto error = ctx.alloc<Error>(message.to_display(),
                                      list_from(irritants, ctx));
    throw SchemeError::raised(error);
  });
  install(ctx, "raise", [](Args raw, Ctx &) -> Obj {
    throw SchemeError::raised(match(raw, arg::any));
  });
  install(ctx, "error-object-message", [](Args raw, Ctx &ctx) {
    return ctx.alloc<String>(match(raw, arg::error)->message);
  });
  install(ctx, "error-object-irritants", [](Args raw, Ctx &) {
    return match(raw, arg::error)->irritants;
  });
  install(ctx, "eval", [](Args raw, Ctx &ctx) {
    return ctx.eval_global(match(raw, arg::any));
  });
  install(ctx, "load", [](Args raw, Ctx &ctx) -> Obj {
    const std::string &path = match(raw, arg::string)->data;
    std::ifstream file(path);
    if (!file) {
      throw UnattributedError("could not open " + path);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    ctx.execute(buffer.str());
    return Void{};
  });
  install(ctx, "file-exists?", [](Args raw, Ctx &) {
    return std::ifstream(match(raw, arg::string)->data).good();
  });
  install(ctx, "exit", [](Args raw, Ctx &) -> Obj {
    auto code = match_optional(raw, arg::index);
    throw scheme::ExitRequest(code ? static_cast<int>(*code) : 0);
  });
}

void install_builtins(Ctx &ctx) {
  install_numbers(ctx);
  install_objects(ctx);
  install_lists(ctx);
  install_strings(ctx);
  install_vectors(ctx);
  install_io(ctx);
  install_other(ctx);
  ctx.install_builtin("apply", Builtin::Apply{});
}
