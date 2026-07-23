#include "builtins.hpp"

#include "arity.hpp"
#include "ctx.hpp"
#include "errors.hpp"
#include "reader.hpp"

#include <algorithm>
#include <cmath>
#include <compare>
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

enum class PatternKind { Required, Optional, Rest };

template <typename T> struct ObjectPattern {
  using Value = T;
  static constexpr PatternKind kind = PatternKind::Required;

  T (Obj::*accessor)() const;

  T decode(Obj obj) const {
    try {
      return (obj.*accessor)();
    } catch (const SchemeError &error) {
      throw UnattributedError(error.what());
    }
  }

  T parse(Args raw, size_t &position) const {
    return decode(raw[position++]);
  }
};

template <typename T>
ObjectPattern(T (Obj::*)() const) -> ObjectPattern<T>;

struct AnyPattern {
  using Value = Obj;
  static constexpr PatternKind kind = PatternKind::Required;

  Obj parse(Args raw, size_t &position) const { return raw[position++]; }
};

struct IndexPattern {
  using Value = size_t;
  static constexpr PatternKind kind = PatternKind::Required;

  size_t parse(Args raw, size_t &position) const;
};

namespace arg {

constexpr AnyPattern any;
constexpr ObjectPattern number{&Obj::as_number};
constexpr ObjectPattern character{&Obj::as_char};
constexpr ObjectPattern symbol{&Obj::as_symbol};
constexpr ObjectPattern string{&Obj::as_string};
constexpr ObjectPattern pair{&Obj::as_cons};
constexpr ObjectPattern vector{&Obj::as_vector};
constexpr ObjectPattern error{&Obj::as_error};
constexpr IndexPattern index;

}

size_t IndexPattern::parse(Args raw, size_t &position) const {
  auto value = arg::number.decode(raw[position++]).to_size();
  if (!value) {
    throw UnattributedError("expected non-negative integer");
  }
  return *value;
}

template <typename Pattern> struct OptionalPattern {
  using Value = std::optional<typename Pattern::Value>;
  static constexpr PatternKind kind = PatternKind::Optional;

  Pattern pattern;

  Value parse(Args raw, size_t &position) const {
    if (position == raw.size()) {
      return {};
    }
    return pattern.parse(raw, position);
  }
};

template <typename Pattern> struct RestPattern {
  using Value = std::vector<typename Pattern::Value>;
  static constexpr PatternKind kind = PatternKind::Rest;

  Pattern pattern;

  Value parse(Args raw, size_t &position) const {
    Value values;
    values.reserve(raw.size() - position);
    while (position < raw.size()) {
      values.push_back(pattern.parse(raw, position));
    }
    return values;
  }
};

template <typename Pattern>
OptionalPattern<Pattern> optional(Pattern pattern) {
  return {pattern};
}

template <typename Pattern> RestPattern<Pattern> rest(Pattern pattern) {
  return {pattern};
}

consteval bool valid_transition(PatternKind previous, PatternKind next) {
  switch (next) {
  case PatternKind::Required:
    return previous == PatternKind::Required;
  case PatternKind::Optional:
    return previous != PatternKind::Rest;
  case PatternKind::Rest:
    return previous == PatternKind::Required;
  }
  std::unreachable();
}

template <typename... Patterns> consteval bool valid_patterns() {
  if constexpr (sizeof...(Patterns) == 0) {
    return true;
  } else {
    PatternKind state = PatternKind::Required;
    bool valid = true;
    ((valid = valid && valid_transition(state, Patterns::kind),
      state = Patterns::kind),
     ...);
    return valid;
  }
}

template <typename... Patterns> auto match(Args raw, Patterns... patterns) {
  static_assert(valid_patterns<Patterns...>(),
                "required patterns precede optional or final rest patterns");

  constexpr size_t min =
      (size_t{0} + ... + (Patterns::kind == PatternKind::Required ? 1 : 0));
  constexpr bool unbounded =
      ((Patterns::kind == PatternKind::Rest) || ... || false);
  auto arity = unbounded ? Arity::at_least(min)
                         : Arity::between(min, sizeof...(Patterns));
  if (auto error = arity.mismatch(raw.size())) {
    throw UnattributedError(*error);
  }

  size_t position = 0;
  if constexpr (sizeof...(Patterns) == 0) {
    return;
  } else if constexpr (sizeof...(Patterns) == 1) {
    return (patterns.parse(raw, position), ...);
  } else {
    return std::tuple{patterns.parse(raw, position)...};
  }
}

class Installer {
  Ctx &context;

public:
  explicit Installer(Ctx &context) : context{context} {}

  template <typename Implementation>
  void operator()(std::string_view name, Implementation implementation) const {
    static_assert(std::is_empty_v<Implementation>,
                  "builtin implementations must not capture state");

    auto adapter = [name = std::string{name}, implementation](
                       Args raw, Ctx &context) -> Obj {
      try {
        return implementation(raw, context);
      } catch (const UnattributedError &error) {
        throw SchemeError(std::format("{}: {}", name, error.what()));
      }
    };
    context.install_builtin(name, Builtin::Fn{std::move(adapter)});
  }
};

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

template <typename Container>
static decltype(auto) element_at(Container &container, size_t index) {
  if (index >= container.size()) {
    throw UnattributedError("index out of range");
  }
  return container[index];
}

template <auto Predicate>
static void install_predicate(Installer install, std::string_view name) {
  install(name, [](Args raw, Ctx &) {
    return (match(raw, arg::any).*Predicate)();
  });
}

static void install_numbers(Installer install) {
  install("+", [](Args raw, Ctx &context) {
    auto numbers = match(raw, rest(arg::number));
    return std::ranges::fold_left(
        numbers, Number::exact(0, context),
        [&context](Number sum, Number number) {
          return sum.add(number, context);
        });
  });

  install("-", [](Args raw, Ctx &context) {
    auto [first, remaining] = match(raw, arg::number, rest(arg::number));
    if (remaining.empty()) {
      return first.neg(context);
    }
    return std::ranges::fold_left(
        remaining, first, [&context](Number difference, Number number) {
          return difference.sub(number, context);
        });
  });

  install("*", [](Args raw, Ctx &context) {
    auto numbers = match(raw, rest(arg::number));
    return std::ranges::fold_left(
        numbers, Number::exact(1, context),
        [&context](Number product, Number number) {
          return product.mul(number, context);
        });
  });

  install("/", [](Args raw, Ctx &context) {
    auto [first, remaining] = match(raw, arg::number, rest(arg::number));
    if (remaining.empty()) {
      return Number::exact(1, context).div(first, context);
    }
    return std::ranges::fold_left(
        remaining, first, [&context](Number quotient, Number number) {
          return quotient.div(number, context);
        });
  });

  install("<", [](Args raw, Ctx &) {
    auto [first, remaining] = match(raw, arg::number, rest(arg::number));
    return numeric_compare(first, remaining, [](std::partial_ordering order) {
      return order == std::partial_ordering::less;
    });
  });
  install(">", [](Args raw, Ctx &) {
    auto [first, remaining] = match(raw, arg::number, rest(arg::number));
    return numeric_compare(first, remaining, [](std::partial_ordering order) {
      return order == std::partial_ordering::greater;
    });
  });
  install("=", [](Args raw, Ctx &) {
    auto [first, remaining] = match(raw, arg::number, rest(arg::number));
    return numeric_compare(first, remaining, [](std::partial_ordering order) {
      return order == std::partial_ordering::equivalent;
    });
  });
  install("<=", [](Args raw, Ctx &) {
    auto [first, remaining] = match(raw, arg::number, rest(arg::number));
    return numeric_compare(first, remaining, [](std::partial_ordering order) {
      return order == std::partial_ordering::less ||
             order == std::partial_ordering::equivalent;
    });
  });
  install(">=", [](Args raw, Ctx &) {
    auto [first, remaining] = match(raw, arg::number, rest(arg::number));
    return numeric_compare(first, remaining, [](std::partial_ordering order) {
      return order == std::partial_ordering::greater ||
             order == std::partial_ordering::equivalent;
    });
  });

  install("abs", [](Args raw, Ctx &context) {
    return match(raw, arg::number).abs(context);
  });
  install("sqrt", [](Args raw, Ctx &context) {
    return match(raw, arg::number).sqrt(context);
  });
  install("sin", [](Args raw, Ctx &) {
    return std::sin(match(raw, arg::number).to_double());
  });
  install("cos", [](Args raw, Ctx &) {
    return std::cos(match(raw, arg::number).to_double());
  });
  install("log", [](Args raw, Ctx &) {
    return std::log(match(raw, arg::number).to_double());
  });
  install("expt", [](Args raw, Ctx &context) {
    auto [base, power] = match(raw, arg::number, arg::number);
    return base.expt(power, context);
  });

  install("ceiling", [](Args raw, Ctx &) {
    Number number = match(raw, arg::number);
    return number.is_exact()
               ? number
               : Number::inexact(std::ceil(number.to_double()));
  });
  install("floor", [](Args raw, Ctx &) {
    Number number = match(raw, arg::number);
    return number.is_exact()
               ? number
               : Number::inexact(std::floor(number.to_double()));
  });
  install("round", [](Args raw, Ctx &) {
    Number number = match(raw, arg::number);
    return number.is_exact()
               ? number
               : Number::inexact(std::round(number.to_double()));
  });

  install("max", [](Args raw, Ctx &) {
    auto [first, remaining] = match(raw, arg::number, rest(arg::number));
    return minmax(first, remaining, std::partial_ordering::greater);
  });
  install("min", [](Args raw, Ctx &) {
    auto [first, remaining] = match(raw, arg::number, rest(arg::number));
    return minmax(first, remaining, std::partial_ordering::less);
  });

  install("quotient", [](Args raw, Ctx &context) {
    auto [dividend, divisor] = match(raw, arg::number, arg::number);
    return dividend.quotient(divisor, context);
  });
  install("remainder", [](Args raw, Ctx &context) {
    auto [dividend, divisor] = match(raw, arg::number, arg::number);
    return dividend.remainder(divisor, context);
  });
  install("modulo", [](Args raw, Ctx &context) {
    auto [dividend, divisor] = match(raw, arg::number, arg::number);
    return dividend.modulo(divisor, context);
  });

  install("even?", [](Args raw, Ctx &) {
    return match(raw, arg::number).is_even();
  });
  install("odd?", [](Args raw, Ctx &) {
    return !match(raw, arg::number).is_even();
  });
  install("zero?", [](Args raw, Ctx &) {
    return match(raw, arg::number).is_zero();
  });
  install("positive?", [](Args raw, Ctx &context) {
    return match(raw, arg::number).compare(Number::exact(0, context)) ==
           std::partial_ordering::greater;
  });
  install("negative?", [](Args raw, Ctx &context) {
    return match(raw, arg::number).compare(Number::exact(0, context)) ==
           std::partial_ordering::less;
  });
  install("exact?", [](Args raw, Ctx &) {
    return match(raw, arg::number).is_exact();
  });
  install("inexact?", [](Args raw, Ctx &) {
    return !match(raw, arg::number).is_exact();
  });

  auto exact = [](Args raw, Ctx &context) {
    return match(raw, arg::number).to_exact(context);
  };
  install("exact", exact);
  install("inexact->exact", exact);

  auto inexact = [](Args raw, Ctx &) {
    return match(raw, arg::number).to_inexact();
  };
  install("inexact", inexact);
  install("exact->inexact", inexact);
}

static void install_objects(Installer install) {
  install_predicate<&Obj::is_null>(install, "null?");
  install_predicate<&Obj::is_bool>(install, "boolean?");
  install_predicate<&Obj::is_number>(install, "number?");
  install("integer?", [](Args raw, Ctx &) {
    Obj value = match(raw, arg::any);
    auto number = value.try_as_number();
    return number && number->is_integer();
  });
  install_predicate<&Obj::is_cons>(install, "pair?");
  install_predicate<&Obj::is_symbol>(install, "symbol?");
  install_predicate<&Obj::is_string>(install, "string?");
  install("procedure?", [](Args raw, Ctx &) {
    Obj value = match(raw, arg::any);
    return value.is_procedure() || value.is_builtin();
  });
  install_predicate<&Obj::is_list>(install, "list?");
  install_predicate<&Obj::is_void>(install, "void?");
  install_predicate<&Obj::is_promise>(install, "promise?");
  install_predicate<&Obj::is_char>(install, "char?");
  install_predicate<&Obj::is_vector>(install, "vector?");
  install_predicate<&Obj::is_error>(install, "error-object?");
  install("not", [](Args raw, Ctx &) {
    return match(raw, arg::any).is_false();
  });
  install("void", [](Args raw, Ctx &) {
    match(raw);
    return Void{};
  });

  auto eq = [](Args raw, Ctx &) {
    auto [a, b] = match(raw, arg::any, arg::any);
    return a.eqv(b);
  };
  install("eq?", eq);
  install("eqv?", eq);

  install("equal?", [](Args raw, Ctx &) {
    auto [a, b] = match(raw, arg::any, arg::any);
    return a.equals(b);
  });
}

static void install_lists(Installer install) {
  install("car", [](Args raw, Ctx &) {
    return match(raw, arg::pair)->car;
  });
  install("cdr", [](Args raw, Ctx &) {
    return match(raw, arg::pair)->cdr;
  });
  install("cons", [](Args raw, Ctx &context) {
    auto [car, cdr] = match(raw, arg::any, arg::any);
    return context.alloc<Cons>(car, cdr);
  });
  install("list", [](Args raw, Ctx &context) {
    return list_from(match(raw, rest(arg::any)), context);
  });
  install("length", [](Args raw, Ctx &context) -> Obj {
    Obj list = match(raw, arg::any);
    if (!list.is_null() && !list.is_cons()) {
      throw UnattributedError("expected list, got " + list.type_name());
    }
    List parts{list};
    if (!parts.proper()) {
      throw UnattributedError("expected proper list");
    }
    return Number::exact(
        static_cast<int64_t>(parts.elements.size()), context);
  });
  install("list-ref", [](Args raw, Ctx &) -> Obj {
    auto [pair, index] = match(raw, arg::pair, arg::index);
    List list{pair};
    return element_at(list.elements, index);
  });
  install("set-car!", [](Args raw, Ctx &) {
    auto [pair, value] = match(raw, arg::pair, arg::any);
    pair->car = value;
    return Void{};
  });
  install("set-cdr!", [](Args raw, Ctx &) {
    auto [pair, value] = match(raw, arg::pair, arg::any);
    pair->cdr = value;
    return Void{};
  });
}

static void install_strings(Installer install) {
  install("string-length", [](Args raw, Ctx &context) {
    auto string = match(raw, arg::string);
    return Number::exact(static_cast<int64_t>(string->data.size()), context);
  });
  install("string-ref", [](Args raw, Ctx &) -> Obj {
    auto [string, index] = match(raw, arg::string, arg::index);
    return element_at(string->data, index);
  });
  install("substring", [](Args raw, Ctx &context) -> Obj {
    auto [string, start, requested_end] =
        match(raw, arg::string, arg::index, optional(arg::index));
    size_t end = requested_end.value_or(string->data.size());
    if (start > end || end > string->data.size()) {
      throw UnattributedError("index out of range");
    }
    return context.alloc<String>(string->data.substr(start, end - start));
  });
  install("string-append", [](Args raw, Ctx &context) {
    auto strings = match(raw, rest(arg::string));
    return context.alloc<String>(std::ranges::to<std::string>(
        strings | std::views::transform([](String *string)
                                            -> const std::string & {
          return string->data;
        }) |
        std::views::join));
  });
  install("string=?", [](Args raw, Ctx &) {
    auto [a, b] = match(raw, arg::string, arg::string);
    return a->data == b->data;
  });

  install("char=?", [](Args raw, Ctx &) {
    auto [a, b] = match(raw, arg::character, arg::character);
    return a == b;
  });
  install("char->integer", [](Args raw, Ctx &context) {
    auto character = static_cast<unsigned char>(match(raw, arg::character));
    return Number::exact(static_cast<int64_t>(character), context);
  });
  install("integer->char", [](Args raw, Ctx &) -> Obj {
    size_t value = match(raw, arg::index);
    if (value > std::numeric_limits<unsigned char>::max()) {
      throw UnattributedError("value out of range");
    }
    return static_cast<char>(static_cast<unsigned char>(value));
  });
  install("string->list", [](Args raw, Ctx &context) {
    return list_from(match(raw, arg::string)->data, context);
  });
  install("list->string", [](Args raw, Ctx &context) -> Obj {
    List list{match(raw, arg::any)};
    if (!list.proper()) {
      throw UnattributedError("expected proper list");
    }
    return context.alloc<String>(std::ranges::to<std::string>(
        list.elements | std::views::transform([](Obj value) {
          return arg::character.decode(value);
        })));
  });

  install("number->string", [](Args raw, Ctx &context) {
    return context.alloc<String>(match(raw, arg::number).to_string());
  });
  install("string->number", [](Args raw, Ctx &context) -> Obj {
    auto string = match(raw, arg::string);
    try {
      return Number::parse(string->data, context);
    } catch (const SchemeError &) {
      return false;
    }
  });
  install("symbol->string", [](Args raw, Ctx &context) {
    return context.alloc<String>(match(raw, arg::symbol).name());
  });
  install("string->symbol", [](Args raw, Ctx &context) {
    return context.intern(match(raw, arg::string)->data);
  });
}

static void install_io(Installer install) {
  install("display", [](Args raw, Ctx &context) {
    context.output(match(raw, arg::any).to_display());
    return Void{};
  });
  install("write", [](Args raw, Ctx &context) {
    context.output(match(raw, arg::any).to_write());
    return Void{};
  });
  install("newline", [](Args raw, Ctx &context) {
    match(raw);
    context.output("\n");
    return Void{};
  });
  install("read", [](Args raw, Ctx &context) -> Obj {
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

      ReadOutcome outcome = read_one(input, context);
      if (auto *datum = std::get_if<ReadDatum>(&outcome)) {
        return datum->value;
      }
    }
  });
}

static void install_vectors(Installer install) {
  install("vector", [](Args raw, Ctx &context) {
    return context.alloc<Vector>(match(raw, rest(arg::any)));
  });
  install("make-vector", [](Args raw, Ctx &context) {
    auto [size, requested_fill] =
        match(raw, arg::index, optional(arg::any));
    Obj fill =
        requested_fill.value_or(Obj(Number::exact(0, context)));
    return context.alloc<Vector>(std::vector<Obj>(size, fill));
  });
  install("vector-ref", [](Args raw, Ctx &) -> Obj {
    auto [vector, index] = match(raw, arg::vector, arg::index);
    return element_at(vector->data, index);
  });
  install("vector-set!", [](Args raw, Ctx &) {
    auto [vector, index, value] =
        match(raw, arg::vector, arg::index, arg::any);
    element_at(vector->data, index) = value;
    return Void{};
  });
  install("vector-length", [](Args raw, Ctx &context) {
    auto vector = match(raw, arg::vector);
    return Number::exact(static_cast<int64_t>(vector->data.size()), context);
  });
  install("vector->list", [](Args raw, Ctx &context) {
    return list_from(match(raw, arg::vector)->data, context);
  });
  install("list->vector", [](Args raw, Ctx &context) -> Obj {
    List list{match(raw, arg::any)};
    if (!list.proper()) {
      throw UnattributedError("expected proper list");
    }
    return context.alloc<Vector>(std::move(list.elements));
  });
}

static void install_other(Installer install) {
  install("force", [](Args raw, Ctx &context) {
    Obj value = match(raw, arg::any);
    if (Promise *promise = value.try_as_promise()) {
      return promise->force(context);
    }
    return value;
  });
  install("error", [](Args raw, Ctx &context) -> Obj {
    auto [message, irritants] = match(raw, arg::any, rest(arg::any));
    auto error = context.alloc<Error>(message.to_display(),
                                      list_from(irritants, context));
    throw SchemeError::raised(error);
  });
  install("raise", [](Args raw, Ctx &) -> Obj {
    throw SchemeError::raised(match(raw, arg::any));
  });
  install("error-object-message", [](Args raw,
                                                Ctx &context) {
    return context.alloc<String>(match(raw, arg::error)->message);
  });
  install("error-object-irritants", [](Args raw, Ctx &) {
    return match(raw, arg::error)->irritants;
  });
  install("eval", [](Args raw, Ctx &context) {
    return context.eval_global(match(raw, arg::any));
  });
  install("load", [](Args raw, Ctx &context) -> Obj {
    const std::string &path = match(raw, arg::string)->data;
    std::ifstream file(path);
    if (!file) {
      throw UnattributedError("could not open " + path);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    context.execute(buffer.str());
    return Void{};
  });
  install("file-exists?", [](Args raw, Ctx &) {
    return std::ifstream(match(raw, arg::string)->data).good();
  });
  install("exit", [](Args raw, Ctx &) -> Obj {
    auto code = match(raw, optional(arg::index));
    throw scheme::ExitRequest(code ? static_cast<int>(*code) : 0);
  });
}

void install_builtins(Ctx &context) {
  Installer install{context};
  install_numbers(install);
  install_objects(install);
  install_lists(install);
  install_strings(install);
  install_vectors(install);
  install_io(install);
  install_other(install);
  context.install_builtin("apply", Builtin::Apply{});
}
