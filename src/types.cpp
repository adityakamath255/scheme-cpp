#include "types.hpp"
#include "env.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <string>
#include <sstream>
#include <type_traits>

Symbol::Symbol(const std::string *ptr): ptr {ptr} {}

const std::string &Symbol::get_name() const {
  return *ptr;
}

bool Symbol::operator==(Symbol other) const {
  return ptr == other.ptr;
}

Obj::Obj(Value data): data {data} {}
Obj::Obj(bool data): data {data} {}
Obj::Obj(char data): data {data} {}
Obj::Obj(double data): data {Number::inexact(data)} {}
Obj::Obj(Number data): data {data} {}
Obj::Obj(Symbol data): data {data} {}
Obj::Obj(String *data): data {data} {}
Obj::Obj(Cons *data): data {data} {}
Obj::Obj(Vector *data): data {data} {}
Obj::Obj(Procedure *data): data {data} {}
Obj::Obj(Builtin *data): data {data} {}
Obj::Obj(Null data): data {data} {}
Obj::Obj(Void data): data {data} {}

template<Type t, typename Alt>
static constexpr bool alt_is =
  std::is_same_v<std::variant_alternative_t<static_cast<size_t>(t), Value>, Alt>;

static_assert(std::variant_size_v<Value> == static_cast<size_t>(Type::Void) + 1);
static_assert(alt_is<Type::Bool, bool>);
static_assert(alt_is<Type::Char, char>);
static_assert(alt_is<Type::Number, Number>);
static_assert(alt_is<Type::Symbol, Symbol>);
static_assert(alt_is<Type::String, String *>);
static_assert(alt_is<Type::Cons, Cons *>);
static_assert(alt_is<Type::Vector, Vector *>);
static_assert(alt_is<Type::Procedure, Procedure *>);
static_assert(alt_is<Type::Builtin, Builtin *>);
static_assert(alt_is<Type::Null, Null>);
static_assert(alt_is<Type::Void, Void>);

Type Obj::get_type() const {
  return static_cast<Type>(data.index());
}

bool Obj::is_bool() const {
  return std::holds_alternative<bool>(data);
}

bool Obj::is_char() const {
  return std::holds_alternative<char>(data);
}

bool Obj::is_number() const {
  return std::holds_alternative<Number>(data);
}

bool Obj::is_symbol() const {
  return std::holds_alternative<Symbol>(data);
}

bool Obj::is_string() const {
  return std::holds_alternative<String *>(data);
}

bool Obj::is_cons() const {
  return std::holds_alternative<Cons *>(data);
}

bool Obj::is_vector() const {
  return std::holds_alternative<Vector *>(data);
}

bool Obj::is_procedure() const {
  return std::holds_alternative<Procedure *>(data);
}

bool Obj::is_builtin() const {
  return std::holds_alternative<Builtin *>(data);
}

bool Obj::is_null() const {
  return std::holds_alternative<Null>(data);
}

bool Obj::is_void() const {
  return std::holds_alternative<Void>(data);
}

bool Obj::as_bool() const {
  return std::get<bool>(data);
}

char Obj::as_char() const {
  return get<char>(data);
}

Number Obj::as_number() const {
  return std::get<Number>(data);
}

Symbol Obj::as_symbol() const {
  return std::get<Symbol>(data);
}

String *Obj::as_string() const {
  return std::get<String *>(data);
}

Cons *Obj::as_cons() const {
  return std::get<Cons *>(data);
}

Vector *Obj::as_vector() const {
  return std::get<Vector *>(data);
}

Procedure *Obj::as_procedure() const {
  return std::get<Procedure *>(data);
}

Builtin *Obj::as_builtin() const {
  return std::get<Builtin *>(data);
}

std::optional<HeapEntity *> Obj::heap_entity() const {
  switch (get_type()) {
    case Type::Number: return as_number().heap_entity();
    case Type::String: return as_string();
    case Type::Cons: return as_cons();
    case Type::Vector: return as_vector();
    case Type::Procedure: return as_procedure();
    case Type::Builtin: return as_builtin();
    default: return std::nullopt;
  }
}

bool Obj::is_true() const {
  return !is_bool() || as_bool() == true;
}

bool Obj::is_false() const {
  return !is_true();
}

bool Obj::same_type(Obj other) const {
  return data.index() == other.data.index();
}

bool Obj::equals(Obj other) const {
  if (!same_type(other)) {
    return false;
  }

  switch (get_type()) {
    case Type::Bool: return as_bool() == other.as_bool();
    case Type::Char: return as_char() == other.as_char();
    case Type::Number: return as_number().eqv(other.as_number());
    case Type::Symbol: return as_symbol() == other.as_symbol();
    case Type::Procedure: return as_procedure() == other.as_procedure();
    case Type::Builtin: return as_builtin() == other.as_builtin();

    case Type::String: return as_string()->data == other.as_string()->data;

    case Type::Null: case Type::Void: return true;

    case Type::Cons: {
      ListView a{*this};
      ListView b{other};
      return std::ranges::equal(a, b, [](Obj x, Obj y) { return x.equals(y); })
        && a.tail().equals(b.tail());
    }

    case Type::Vector: {
      const std::vector<Obj> &vec_0 = as_vector()->data;
      const std::vector<Obj> &vec_1 = other.as_vector()->data;

      if (vec_0.size() != vec_1.size()) {
        return false;
      }
      else {
        for (size_t i = 0; i < vec_0.size(); i += 1) {
          if (!vec_0[i].equals(vec_1[i])) {
            return false;
          }
        }
        return true;
      }
    }

    default:
      return false;
  }
}

std::string Obj::stringify(bool quote) const {
  switch (get_type()) {
    case Type::Bool:
      return as_bool() ? "#t" : "#f";

    case Type::Number:
      return as_number().to_string();

    case Type::Char: {
      char c = as_char();
      switch (c) {
        case ' ': return "#\\space";
        case '\n': return "#\\newline";
        case '\t': return "#\\tab";
        case '\r': return "#\\return";
        default: return std::string("#\\") + c;
      }
    }

    case Type::Symbol:
      return as_symbol().get_name();

    case Type::String:
      if (!quote) {
        return as_string()->data;
      }
      else {
        std::string res = "\"";
        for (char c : as_string()->data) {
          switch (c) {
            case '"': res += "\\\""; break;
            case '\\': res += "\\\\"; break;
            case '\n': res += "\\n"; break;
            case '\t': res += "\\t"; break;
            default: res += c; break;
          }
        }
        res += '"';
        return res;
      }

    case Type::Cons: {
      ListView list{*this};

      std::string body = std::ranges::to<std::string>(
        list
        | std::views::transform([quote](Obj x) { return x.stringify(quote); })
        | std::views::join_with(' ')
      );

      Obj tail = list.tail();
      std::string dotted = tail.is_null() ? "" : " . " + tail.stringify(quote);

      return "(" + body + dotted + ")";
    }

    case Type::Vector: {
      std::ostringstream res;
      res << "#(";
      const std::vector<Obj> &data = as_vector()->data;
      
      for (size_t i = 0; i < data.size(); i += 1) {
        if (i > 0) {
          res << " ";
        }
        res << data[i].stringify(quote);
      }

      res << ")";

      return res.str();
    }

    case Type::Procedure:
      if (as_procedure()->macro) {
        return std::format(
          "<macro at {}>",
          static_cast<const void *>(as_procedure())
        );
      }
      else {
        return std::format(
          "<procedure at {}>",
          static_cast<const void *>(as_procedure())
        );
      }

    case Type::Builtin:
      return std::format(
        "<procedure at {}>",
        static_cast<const void *>(as_builtin())
      );

    case Type::Null:
      return "()";

    case Type::Void:
      return "#<void>";
      
    default:
      return "???";
  }
}

std::string Obj::stringify_type() const {
  switch (get_type()) {
    case Type::Bool: return "boolean";
    case Type::Number: return "number";
    case Type::Char: return "char";
    case Type::Symbol: return "symbol";
    case Type::String: return "string";
    case Type::Cons: return "cons";
    case Type::Vector: return "vector";
    case Type::Procedure:
    case Type::Builtin: return "procedure";
    case Type::Null: return "null";
    case Type::Void: return "void";
    default: return "???";
  }
}

Obj Obj::car() const {
  return as_cons()->car;
}

Obj Obj::cdr() const {
  return as_cons()->cdr;
}

ListView::ListView(Obj head): head {head} {}

ListView::iterator::iterator(Obj cur): cur {cur} {}

Obj ListView::iterator::operator*() const {
  return cur.car();
}

ListView::iterator &ListView::iterator::operator++() {
  cur = cur.cdr();
  return *this;
}

ListView::iterator ListView::iterator::operator++(int) {
  iterator tmp = *this;
  ++*this;
  return tmp;
}

bool ListView::iterator::operator==(std::default_sentinel_t) const {
  return !cur.is_cons();
}

ListView::iterator ListView::begin() const {
  return iterator {head};
}

std::default_sentinel_t ListView::end() const {
  return {};
}

Obj ListView::tail() const {
  Obj cur = head;
  while (cur.is_cons()) {
    cur = cur.cdr();
  }
  return cur;
}

ListProfile Obj::get_list_profile() const {
  size_t len = 0;
  Obj curr = *this;
  while (curr.is_cons()) {
    len += 1;
    curr = curr.cdr();
  }
  return {
    .size = len, 
    .is_proper = curr.is_null()
  };
}

size_t Obj::get_list_size() const {
  return get_list_profile().size;
}

bool Obj::is_list() const {
  return get_list_profile().is_proper;
}

String::String(std::string data): data {std::move(data)} {}

Cons::Cons(Obj car, Obj cdr): car {car}, cdr {cdr} {}

void Cons::trace(std::vector<HeapEntity *> *worklist) const {
  if (auto entity = car.heap_entity()) {
    worklist->push_back(*entity);
  }
  if (auto entity = cdr.heap_entity()) {
    worklist->push_back(*entity);
  }
}

Vector::Vector(std::vector<Obj> data): data {std::move(data)} {}

void Vector::trace(std::vector<HeapEntity *> *worklist) const {
  for (auto obj : data) {
    if (auto entity = obj.heap_entity()) {
      worklist->push_back(*entity);
    }
  }
}

Builtin::Builtin(Builtin::Fn fn): fn {fn} {}

Procedure::Procedure(
  std::vector<Symbol> params,
  Obj body,
  Env *env,
  bool variadic,
  bool macro
): 
  params {std::move(params)}, 
  body {body}, 
  env {env}, 
  variadic {variadic},
  macro {macro}
{}

void Procedure::trace(std::vector<HeapEntity *> *worklist) const {
  if (auto entity = body.heap_entity()) {
    worklist->push_back(*entity);
  }
  worklist->push_back(env);
}
