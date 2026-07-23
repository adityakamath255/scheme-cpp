#include "types.hpp"

#include "arity.hpp"
#include "ctx.hpp"
#include "errors.hpp"
#include "expression.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <ranges>
#include <string>
#include <utility>

namespace {

struct StringEscape {
  char character;
  char escape;
};

constexpr std::array string_escapes{
    StringEscape{'\n', 'n'},
    StringEscape{'\t', 't'},
    StringEscape{'\r', 'r'},
    StringEscape{'\\', '\\'},
    StringEscape{'"', '"'},
};

struct CharacterName {
  std::string_view name;
  char character;
};

constexpr std::array character_names{
    CharacterName{"space", ' '},
    CharacterName{"newline", '\n'},
    CharacterName{"tab", '\t'},
    CharacterName{"return", '\r'},
};

template <typename T>
constexpr std::string_view type_name_for();

template <>
constexpr std::string_view type_name_for<bool>() {
  return "boolean";
}
template <>
constexpr std::string_view type_name_for<char>() {
  return "char";
}
template <>
constexpr std::string_view type_name_for<Number>() {
  return "number";
}
template <>
constexpr std::string_view type_name_for<Symbol>() {
  return "symbol";
}
template <>
constexpr std::string_view type_name_for<String *>() {
  return "string";
}
template <>
constexpr std::string_view type_name_for<Cons *>() {
  return "pair";
}
template <>
constexpr std::string_view type_name_for<Vector *>() {
  return "vector";
}
template <>
constexpr std::string_view type_name_for<Procedure *>() {
  return "procedure";
}
template <>
constexpr std::string_view type_name_for<Builtin *>() {
  return "procedure";
}
template <>
constexpr std::string_view type_name_for<Promise *>() {
  return "promise";
}
template <>
constexpr std::string_view type_name_for<Error *>() {
  return "error";
}
template <>
constexpr std::string_view type_name_for<Null>() {
  return "null";
}
template <>
constexpr std::string_view type_name_for<Void>() {
  return "void";
}

template <typename T>
std::optional<T> try_value(const Value &data) {
  if (const auto *value = std::get_if<T>(&data)) {
    return *value;
  }
  return std::nullopt;
}

template <typename T>
T try_pointer(const Value &data) {
  if (const auto *value = std::get_if<T>(&data)) {
    return *value;
  }
  return nullptr;
}

template <typename T>
T expect(const Value &data) {
  if (const auto *value = std::get_if<T>(&data)) {
    return *value;
  }
  throw SchemeError(std::format(
      "expected {}, got {}", type_name_for<T>(), Obj(data).type_name()));
}

}

std::optional<char> decode_string_escape(char escape) {
  auto found =
      std::ranges::find(string_escapes, escape, &StringEscape::escape);
  return found == string_escapes.end()
             ? std::nullopt
             : std::optional{found->character};
}

std::optional<char> encode_string_escape(char character) {
  auto found = std::ranges::find(
      string_escapes, character, &StringEscape::character);
  return found == string_escapes.end()
             ? std::nullopt
             : std::optional{found->escape};
}

std::optional<char> decode_character_name(std::string_view name) {
  auto found =
      std::ranges::find(character_names, name, &CharacterName::name);
  return found == character_names.end()
             ? std::nullopt
             : std::optional{found->character};
}

std::optional<std::string_view> encode_character_name(char character) {
  auto found = std::ranges::find(
      character_names, character, &CharacterName::character);
  return found == character_names.end()
             ? std::nullopt
             : std::optional{found->name};
}

Symbol::Symbol(const std::string &name) : ptr{&name} {}

const std::string &Symbol::name() const { return *ptr; }

bool Symbol::operator==(Symbol other) const { return ptr == other.ptr; }

bool Null::operator==(Null) const { return true; }

bool Void::operator==(Void) const { return true; }

Obj::Obj(Value data) : data{data} {}
Obj::Obj(bool data) : data{data} {}
Obj::Obj(char data) : data{data} {}
Obj::Obj(double data) : data{Number::inexact(data)} {}
Obj::Obj(Number data) : data{data} {}
Obj::Obj(Symbol data) : data{data} {}
Obj::Obj(String *data) : data{data} {}
Obj::Obj(Cons *data) : data{data} {}
Obj::Obj(Vector *data) : data{data} {}
Obj::Obj(Procedure *data) : data{data} {}
Obj::Obj(Builtin *data) : data{data} {}
Obj::Obj(Promise *data) : data{data} {}
Obj::Obj(Error *data) : data{data} {}
Obj::Obj(Null data) : data{data} {}
Obj::Obj(Void data) : data{data} {}

bool Obj::is_bool() const { return std::holds_alternative<bool>(data); }

bool Obj::is_char() const { return std::holds_alternative<char>(data); }

bool Obj::is_number() const { return std::holds_alternative<Number>(data); }

bool Obj::is_symbol() const { return std::holds_alternative<Symbol>(data); }

bool Obj::is_string() const { return std::holds_alternative<String *>(data); }

bool Obj::is_cons() const { return std::holds_alternative<Cons *>(data); }

bool Obj::is_vector() const { return std::holds_alternative<Vector *>(data); }

bool Obj::is_procedure() const {
  return std::holds_alternative<Procedure *>(data);
}

bool Obj::is_builtin() const { return std::holds_alternative<Builtin *>(data); }

bool Obj::is_promise() const { return std::holds_alternative<Promise *>(data); }

bool Obj::is_error() const { return std::holds_alternative<Error *>(data); }

bool Obj::is_null() const { return std::holds_alternative<Null>(data); }

bool Obj::is_void() const { return std::holds_alternative<Void>(data); }

std::optional<bool> Obj::try_as_bool() const {
  return try_value<bool>(data);
}

std::optional<char> Obj::try_as_char() const {
  return try_value<char>(data);
}

std::optional<Number> Obj::try_as_number() const {
  return try_value<Number>(data);
}

std::optional<Symbol> Obj::try_as_symbol() const {
  return try_value<Symbol>(data);
}

String *Obj::try_as_string() const {
  return try_pointer<String *>(data);
}

Cons *Obj::try_as_cons() const {
  return try_pointer<Cons *>(data);
}

Vector *Obj::try_as_vector() const {
  return try_pointer<Vector *>(data);
}

Procedure *Obj::try_as_procedure() const {
  return try_pointer<Procedure *>(data);
}

Builtin *Obj::try_as_builtin() const {
  return try_pointer<Builtin *>(data);
}

Promise *Obj::try_as_promise() const {
  return try_pointer<Promise *>(data);
}

Error *Obj::try_as_error() const {
  return try_pointer<Error *>(data);
}

bool Obj::as_bool() const { return expect<bool>(data); }

char Obj::as_char() const { return expect<char>(data); }

Number Obj::as_number() const { return expect<Number>(data); }

Symbol Obj::as_symbol() const { return expect<Symbol>(data); }

String *Obj::as_string() const { return expect<String *>(data); }

Cons *Obj::as_cons() const { return expect<Cons *>(data); }

Vector *Obj::as_vector() const { return expect<Vector *>(data); }

Procedure *Obj::as_procedure() const {
  return expect<Procedure *>(data);
}

Builtin *Obj::as_builtin() const {
  return expect<Builtin *>(data);
}

Promise *Obj::as_promise() const {
  return expect<Promise *>(data);
}

Error *Obj::as_error() const { return expect<Error *>(data); }

HeapEntity *Obj::heap_entity() const {
  return visit(overloaded{
      [](Number number) { return number.heap_entity(); },
      [](String *string) -> HeapEntity * { return string; },
      [](Cons *cons) -> HeapEntity * { return cons; },
      [](Vector *vector) -> HeapEntity * { return vector; },
      [](Procedure *procedure) -> HeapEntity * { return procedure; },
      [](Builtin *builtin) -> HeapEntity * { return builtin; },
      [](Promise *promise) -> HeapEntity * { return promise; },
      [](Error *error) -> HeapEntity * { return error; },
      [](auto) -> HeapEntity * { return nullptr; },
  });
}

bool Obj::is_true() const { return !is_bool() || as_bool() == true; }

bool Obj::is_false() const { return !is_true(); }

bool Obj::same_type(Obj other) const {
  return data.index() == other.data.index();
}

bool Obj::eqv(Obj other) const { return data == other.data; }

bool Obj::equals(Obj other) const {
  if (eqv(other)) {
    return true;
  }
  if (!same_type(other)) {
    return false;
  }

  if (String *string = try_as_string()) {
    return string->data == other.as_string()->data;
  }

  if (Cons *cons = try_as_cons()) {
    Obj left = cons;
    Obj right = other;
    while (Cons *left_cons = left.try_as_cons()) {
      Cons *right_cons = right.try_as_cons();
      if (!right_cons || !left_cons->car.equals(right_cons->car)) {
        return false;
      }
      left = left_cons->cdr;
      right = right_cons->cdr;
    }
    return left.equals(right);
  }

  if (Vector *vector = try_as_vector()) {
    return std::ranges::equal(vector->data, other.as_vector()->data,
                              [](Obj x, Obj y) { return x.equals(y); });
  }

  return false;
}

static std::string render(Obj obj, bool write);

static std::string join_elems(std::ranges::input_range auto &&elems,
                              bool write) {
  return std::ranges::to<std::string>(
      elems |
      std::views::transform([write](Obj x) { return render(x, write); }) |
      std::views::join_with(' '));
}

std::string Obj::to_write() const { return render(*this, true); }
std::string Obj::to_display() const { return render(*this, false); }

static std::string render(Obj obj, bool write) {
  return obj.visit(overloaded{
      [](bool value) -> std::string { return value ? "#t" : "#f"; },
      [write](char value) -> std::string {
        if (!write) {
          return std::string(1, value);
        }
        if (auto name = encode_character_name(value)) {
          return "#\\" + std::string(*name);
        }
        return std::string("#\\") + value;
      },
      [](Number value) { return value.to_string(); },
      [](Symbol value) { return value.name(); },
      [write](String *value) -> std::string {
        if (!write) {
          return value->data;
        }
        std::string result = "\"";
        for (char character : value->data) {
          if (auto escape = encode_string_escape(character)) {
            result += '\\';
            result += *escape;
          } else {
            result += character;
          }
        }
        result += '"';
        return result;
      },
      [write](Cons *value) {
        List list{value};
        std::string dotted = list.proper()
                                 ? ""
                                 : " . " + render(list.tail, write);
        return "(" + join_elems(list.elements, write) + dotted + ")";
      },
      [write](Vector *value) {
        return "#(" + join_elems(value->data, write) + ")";
      },
      [](Procedure *) -> std::string { return "#<procedure>"; },
      [](Builtin *) -> std::string { return "#<procedure>"; },
      [](Promise *) -> std::string { return "#<promise>"; },
      [](Error *value) {
        return "#<error: " + value->describe() + ">";
      },
      [](Null) -> std::string { return "()"; },
      [](Void) -> std::string { return "#<void>"; },
  });
}

std::string Obj::type_name() const {
  return std::string(visit([]<typename T>(const T &) {
    return type_name_for<T>();
  }));
}

Obj Obj::car() const { return as_cons()->car; }

Obj Obj::cdr() const { return as_cons()->cdr; }

List::List(Obj value) : elements{}, tail{value} {
  while (Cons *cons = tail.try_as_cons()) {
    elements.push_back(cons->car);
    tail = cons->cdr;
  }
}

bool List::proper() const { return tail.is_null(); }

bool Obj::is_list() const { return List{*this}.proper(); }

String::String(std::string data) : data{std::move(data)} {}

void trace_child(Obj obj, std::vector<const HeapEntity *> &worklist) {
  if (auto *entity = obj.heap_entity()) {
    worklist.push_back(entity);
  }
}

Env::Env(Env *parent) : bindings{}, parent{parent} {}

std::optional<Obj> Env::lookup(Symbol name) const {
  auto binding = bindings.find(name);
  if (binding != bindings.end()) {
    return binding->second;
  }
  return parent ? parent->lookup(name) : std::nullopt;
}

void Env::define(Symbol name, Obj value) {
  bindings.insert_or_assign(name, value);
}

bool Env::set(Symbol name, Obj value) {
  auto binding = bindings.find(name);
  if (binding != bindings.end()) {
    binding->second = value;
    return true;
  }
  return parent && parent->set(name, value);
}

void Env::trace(std::vector<const HeapEntity *> &worklist) const {
  for (const auto &[_, value] : bindings) {
    trace_child(value, worklist);
  }
  if (parent) {
    worklist.push_back(parent);
  }
}

Cons::Cons(Obj car, Obj cdr) : car{car}, cdr{cdr} {}

void Cons::trace(std::vector<const HeapEntity *> &worklist) const {
  trace_child(car, worklist);
  trace_child(cdr, worklist);
}

Vector::Vector(std::vector<Obj> data) : data{std::move(data)} {}

void Vector::trace(std::vector<const HeapEntity *> &worklist) const {
  for (Obj obj : data) {
    trace_child(obj, worklist);
  }
}

Formals Formals::parse(Obj formals) {
  if (auto rest = formals.try_as_symbol()) {
    return {{}, *rest};
  }

  List params{formals};
  std::vector<Symbol> fixed;
  for (Obj param : params.elements) {
    auto symbol = param.try_as_symbol();
    if (!symbol) {
      throw SchemeError("parameter must be a symbol");
    }
    fixed.push_back(*symbol);
  }

  if (params.proper()) {
    return {std::move(fixed), std::nullopt};
  }
  if (auto rest = params.tail.try_as_symbol()) {
    return {std::move(fixed), *rest};
  }
  throw SchemeError("invalid parameter list");
}

void Formals::bind(Env &env, const std::vector<Obj> &args,
                   Ctx &context) const {
  auto arity = rest ? Arity::at_least(fixed.size())
                    : Arity::exactly(fixed.size());
  if (auto error = arity.mismatch(args.size())) {
    throw SchemeError(*error);
  }

  for (size_t i = 0; i < fixed.size(); i += 1) {
    env.define(fixed[i], args[i]);
  }
  if (rest) {
    env.define(*rest,
               list_from(args | std::views::drop(fixed.size()), context));
  }
}

Builtin::Builtin(Builtin::Implementation implementation)
    : implementation{std::move(implementation)} {}

Procedure::Procedure(const LambdaExpr *code, Env &env)
    : code{code}, env{env} {}

void Procedure::trace(std::vector<const HeapEntity *> &worklist) const {
  worklist.push_back(code);
  worklist.push_back(&env.get());
}

Promise::Promise(const Expr *body, Env &env) : state{Thunk{body, env}} {}

Obj Promise::force(Ctx &context) {
  if (auto *t = std::get_if<Thunk>(&state)) {
    state = context.eval(t->body, t->env.get());
  }
  return std::get<Obj>(state);
}

void Promise::trace(std::vector<const HeapEntity *> &worklist) const {
  std::visit(overloaded{
                 [&](const Thunk &t) {
                   worklist.push_back(t.body);
                   worklist.push_back(&t.env.get());
                 },
                 [&](Obj value) { trace_child(value, worklist); },
             },
             state);
}

Error::Error(std::string message, Obj irritants)
    : message{std::move(message)}, irritants{irritants} {}

std::string Error::describe() const {
  if (irritants.is_null()) {
    return message;
  }
  List list{irritants};
  return message + " " + join_elems(list.elements, true);
}

void Error::trace(std::vector<const HeapEntity *> &worklist) const {
  trace_child(irritants, worklist);
}
