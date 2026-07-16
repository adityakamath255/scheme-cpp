#include "types.hpp"
#include "env.hpp"
#include "eval.hpp"
#include "runtime.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>

Symbol::Symbol(const std::string *ptr) : ptr{ptr} {}

const std::string &Symbol::get_name() const { return *ptr; }

bool Symbol::operator==(Symbol other) const { return ptr == other.ptr; }

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

template <Type t, typename Alt>
static constexpr bool alt_is =
    std::is_same_v<std::variant_alternative_t<static_cast<size_t>(t), Value>,
                   Alt>;

static_assert(std::variant_size_v<Value> ==
              static_cast<size_t>(Type::Void) + 1);
static_assert(alt_is<Type::Bool, bool>);
static_assert(alt_is<Type::Char, char>);
static_assert(alt_is<Type::Number, Number>);
static_assert(alt_is<Type::Symbol, Symbol>);
static_assert(alt_is<Type::String, String *>);
static_assert(alt_is<Type::Cons, Cons *>);
static_assert(alt_is<Type::Vector, Vector *>);
static_assert(alt_is<Type::Procedure, Procedure *>);
static_assert(alt_is<Type::Builtin, Builtin *>);
static_assert(alt_is<Type::Promise, Promise *>);
static_assert(alt_is<Type::Error, Error *>);
static_assert(alt_is<Type::Null, Null>);
static_assert(alt_is<Type::Void, Void>);

Type Obj::get_type() const { return static_cast<Type>(data.index()); }

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

template <typename T>
static T expect(const Value &data, std::string_view wanted) {
  if (auto p = std::get_if<T>(&data)) {
    return *p;
  }
  throw SchemeError(
      std::format("expected {}, got {}", wanted, Obj(data).stringify_type()));
}

bool Obj::as_bool() const { return expect<bool>(data, "boolean"); }

char Obj::as_char() const { return expect<char>(data, "char"); }

Number Obj::as_number() const { return expect<Number>(data, "number"); }

Symbol Obj::as_symbol() const { return expect<Symbol>(data, "symbol"); }

String *Obj::as_string() const { return expect<String *>(data, "string"); }

Cons *Obj::as_cons() const { return expect<Cons *>(data, "pair"); }

Vector *Obj::as_vector() const { return expect<Vector *>(data, "vector"); }

Procedure *Obj::as_procedure() const {
  return expect<Procedure *>(data, "procedure");
}

Builtin *Obj::as_builtin() const {
  return expect<Builtin *>(data, "procedure");
}

Promise *Obj::as_promise() const { return expect<Promise *>(data, "promise"); }

Error *Obj::as_error() const { return expect<Error *>(data, "error"); }

std::optional<HeapEntity *> Obj::heap_entity() const {
  using Ret = std::optional<HeapEntity *>;
  return std::visit(overloaded{
                        [](Number n) { return n.heap_entity(); },
                        [](String *s) -> Ret { return s; },
                        [](Cons *c) -> Ret { return c; },
                        [](Vector *v) -> Ret { return v; },
                        [](Procedure *p) -> Ret { return p; },
                        [](Builtin *b) -> Ret { return b; },
                        [](Promise *p) -> Ret { return p; },
                        [](Error *e) -> Ret { return e; },
                        [](auto) -> Ret { return std::nullopt; },
                    },
                    data);
}

bool Obj::is_true() const { return !is_bool() || as_bool() == true; }

bool Obj::is_false() const { return !is_true(); }

bool Obj::same_type(Obj other) const {
  return data.index() == other.data.index();
}

bool Obj::equals(Obj other) const {
  if (!same_type(other)) {
    return false;
  }

  switch (get_type()) {
  case Type::Bool:
    return as_bool() == other.as_bool();
  case Type::Char:
    return as_char() == other.as_char();
  case Type::Number:
    return as_number().eqv(other.as_number());
  case Type::Symbol:
    return as_symbol() == other.as_symbol();
  case Type::Procedure:
    return as_procedure() == other.as_procedure();
  case Type::Builtin:
    return as_builtin() == other.as_builtin();
  case Type::Promise:
    return as_promise() == other.as_promise();
  case Type::Error:
    return as_error() == other.as_error();

  case Type::String:
    return as_string()->data == other.as_string()->data;

  case Type::Null:
  case Type::Void:
    return true;

  case Type::Cons: {
    ListView a{*this};
    ListView b{other};
    return std::ranges::equal(a, b, [](Obj x, Obj y) { return x.equals(y); }) &&
           a.tail().equals(b.tail());
  }

  case Type::Vector:
    return std::ranges::equal(as_vector()->data, other.as_vector()->data,
                              [](Obj x, Obj y) { return x.equals(y); });
  }
  std::unreachable();
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
  switch (obj.get_type()) {
  case Type::Bool:
    return obj.as_bool() ? "#t" : "#f";

  case Type::Number:
    return obj.as_number().to_string();

  case Type::Char: {
    char c = obj.as_char();
    if (!write) {
      return std::string(1, c);
    }
    switch (c) {
    case ' ':
      return "#\\space";
    case '\n':
      return "#\\newline";
    case '\t':
      return "#\\tab";
    case '\r':
      return "#\\return";
    default:
      return std::string("#\\") + c;
    }
  }

  case Type::Symbol:
    return obj.as_symbol().get_name();

  case Type::String:
    if (!write) {
      return obj.as_string()->data;
    } else {
      std::string res = "\"";
      for (char c : obj.as_string()->data) {
        switch (c) {
        case '"':
          res += "\\\"";
          break;
        case '\\':
          res += "\\\\";
          break;
        case '\n':
          res += "\\n";
          break;
        case '\t':
          res += "\\t";
          break;
        case '\r':
          res += "\\r";
          break;
        default:
          res += c;
          break;
        }
      }
      res += '"';
      return res;
    }

  case Type::Cons: {
    ListView list{obj};
    Obj tail = list.tail();
    std::string dotted = tail.is_null() ? "" : " . " + render(tail, write);
    return "(" + join_elems(list, write) + dotted + ")";
  }

  case Type::Vector:
    return "#(" + join_elems(obj.as_vector()->data, write) + ")";

  case Type::Procedure:
    return obj.as_procedure()->kind == ProcedureKind::Macro
      ? "#<macro>"
      : "#<procedure>";

  case Type::Builtin:
    return "#<procedure>";

  case Type::Promise:
    return "#<promise>";

  case Type::Error:
    return "#<error: " + obj.as_error()->describe() + ">";

  case Type::Null:
    return "()";

  case Type::Void:
    return "#<void>";
  }
  std::unreachable();
}

std::string Obj::stringify_type() const {
  switch (get_type()) {
  case Type::Bool:
    return "boolean";
  case Type::Number:
    return "number";
  case Type::Char:
    return "char";
  case Type::Symbol:
    return "symbol";
  case Type::String:
    return "string";
  case Type::Cons:
    return "pair";
  case Type::Vector:
    return "vector";
  case Type::Procedure:
  case Type::Builtin:
    return "procedure";
  case Type::Promise:
    return "promise";
  case Type::Error:
    return "error";
  case Type::Null:
    return "null";
  case Type::Void:
    return "void";
  }
  std::unreachable();
}

Obj Obj::car() const { return as_cons()->car; }

Obj Obj::cdr() const { return as_cons()->cdr; }

ListView::ListView(Obj head) : head{head} {}

ListView::iterator::iterator(Obj cur) : cur{cur} {}

Obj ListView::iterator::operator*() const { return cur.car(); }

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

ListView::iterator ListView::begin() const { return iterator{head}; }

std::default_sentinel_t ListView::end() const { return {}; }

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
  return {.size = len, .is_proper = curr.is_null()};
}

bool Obj::is_list() const { return get_list_profile().is_proper; }

String::String(std::string data) : data{std::move(data)} {}

void trace_child(Obj obj, std::vector<HeapEntity *> *worklist) {
  if (auto entity = obj.heap_entity()) {
    worklist->push_back(*entity);
  }
}

Cons::Cons(Obj car, Obj cdr) : car{car}, cdr{cdr} {}

void Cons::trace(std::vector<HeapEntity *> *worklist) const {
  trace_child(car, worklist);
  trace_child(cdr, worklist);
}

Vector::Vector(std::vector<Obj> data) : data{std::move(data)} {}

void Vector::trace(std::vector<HeapEntity *> *worklist) const {
  for (Obj obj : data) {
    trace_child(obj, worklist);
  }
}

Builtin::Builtin(Builtin::Implementation implementation)
    : implementation{std::move(implementation)} {}

Procedure::Procedure(Formals formals, Obj body, Env *env, ProcedureKind kind)
    : formals{std::move(formals)}, body{body}, env{env}, kind{kind} {}

void Procedure::trace(std::vector<HeapEntity *> *worklist) const {
  trace_child(body, worklist);
  worklist->push_back(env);
}

Promise::Promise(Obj body, Env *env) : state{Thunk{body, env}} {}

Obj Promise::force(Evaluator *evaluator) {
  if (auto *t = std::get_if<Thunk>(&state)) {
    state = evaluator->eval(t->body, t->env);
  }
  return std::get<Obj>(state);
}

void Promise::trace(std::vector<HeapEntity *> *worklist) const {
  std::visit(overloaded{
                 [&](const Thunk &t) {
                   trace_child(t.body, worklist);
                   worklist->push_back(t.env);
                 },
                 [&](Obj value) { trace_child(value, worklist); },
             },
             state);
}

Error::Error(std::string message, Obj irritants)
    : message{std::move(message)}, irritants{irritants} {}

std::string Error::describe() const {
  return irritants.is_null()
             ? message
             : message + " " + join_elems(ListView{irritants}, true);
}

void Error::trace(std::vector<HeapEntity *> *worklist) const {
  trace_child(irritants, worklist);
}

static std::string render_condition(Obj payload) {
  return payload.is_error() ? payload.as_error()->describe()
                            : "uncaught exception: " + payload.to_write();
}

SchemeError::SchemeError(const std::string &message)
    : std::runtime_error(message), payload{} {}

SchemeError SchemeError::raised(Obj payload) {
  SchemeError e(render_condition(payload));
  e.payload = payload;
  return e;
}

Obj SchemeError::as_condition(Evaluator *evaluator) {
  return payload ? *payload : Obj(evaluator->alloc<Error>(what(), Null{}));
}
