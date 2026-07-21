#pragma once
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

struct BigInt;
class Number;
class Symbol;
struct String;
struct Cons;
struct Vector;
struct Builtin;
struct Procedure;
class Promise;
struct Error;
struct Null {};
struct Void {};

class Obj;
class Env;
class EvalContext;
struct HeapEntity;
struct ListProfile;

using Value =
    std::variant<bool, char, Number, Symbol, String *, Cons *, Vector *,
                 Procedure *, Builtin *, Promise *, Error *, Null, Void>;

enum class Type : size_t {
  Bool = 0,
  Char,
  Number,
  Symbol,
  String,
  Cons,
  Vector,
  Procedure,
  Builtin,
  Promise,
  Error,
  Null,
  Void
};

class Number {
  std::variant<int64_t, BigInt *, double> rep;
  explicit Number(std::variant<int64_t, BigInt *, double> rep);

public:
  static Number exact(int64_t v, EvalContext &context);
  static Number inexact(double v);
  static Number parse(std::string_view lexeme, EvalContext &context);

  bool is_exact() const;
  bool is_integer() const;
  bool is_zero() const;
  bool is_even() const;

  double to_double() const;
  std::optional<size_t> to_size() const;

  Number add(Number o, EvalContext &context) const;
  Number sub(Number o, EvalContext &context) const;
  Number mul(Number o, EvalContext &context) const;
  Number div(Number o, EvalContext &context) const;
  Number neg(EvalContext &context) const;
  Number abs(EvalContext &context) const;
  Number sqrt(EvalContext &context) const;
  Number quotient(Number o, EvalContext &context) const;
  Number remainder(Number o, EvalContext &context) const;
  Number modulo(Number o, EvalContext &context) const;
  Number expt(Number power, EvalContext &context) const;
  Number to_inexact() const;
  Number to_exact(EvalContext &context) const;

  std::partial_ordering compare(Number o) const;
  bool eqv(Number o) const;

  std::string to_string() const;
  HeapEntity *heap_entity() const;
};

class Symbol {
  const std::string *ptr;

  explicit Symbol(const std::string &);

  friend class EvalContext;
  friend struct std::hash<Symbol>;

public:
  const std::string &name() const;
  bool operator==(Symbol other) const;
};

template <> struct std::hash<Symbol> {
  size_t operator()(const Symbol &s) const {
    return std::hash<const void *>()(s.ptr);
  }
};

class Obj {
  Value data;

public:
  Obj(Value);
  Obj(bool);
  Obj(char);
  Obj(double);
  Obj(Number);
  Obj(Symbol);
  Obj(String *);
  Obj(Cons *);
  Obj(Vector *);
  Obj(Procedure *);
  Obj(Builtin *);
  Obj(Promise *);
  Obj(Error *);
  Obj(Null);
  Obj(Void);

  Type type() const;

  bool is_bool() const;
  bool is_number() const;
  bool is_char() const;
  bool is_symbol() const;
  bool is_string() const;
  bool is_cons() const;
  bool is_vector() const;
  bool is_procedure() const;
  bool is_builtin() const;
  bool is_promise() const;
  bool is_error() const;
  bool is_null() const;
  bool is_void() const;

  bool as_bool() const;
  char as_char() const;
  Number as_number() const;
  Symbol as_symbol() const;
  String *as_string() const;
  Cons *as_cons() const;
  Vector *as_vector() const;
  Procedure *as_procedure() const;
  Builtin *as_builtin() const;
  Promise *as_promise() const;
  Error *as_error() const;

  HeapEntity *heap_entity() const;

  bool is_true() const;
  bool is_false() const;

  bool same_type(Obj) const;
  bool equals(Obj) const;

  std::string to_write() const;
  std::string to_display() const;
  std::string type_name() const;

  ListProfile list_profile() const;
  bool is_list() const;

  Obj car() const;
  Obj cdr() const;
};

class ListView {
  Obj head;

public:
  explicit ListView(Obj head);

  class iterator {
    Obj cur;

  public:
    using value_type = Obj;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::input_iterator_tag;

    explicit iterator(Obj cur);

    Obj operator*() const;
    iterator &operator++();
    iterator operator++(int);

    bool operator==(std::default_sentinel_t) const;
  };

  iterator begin() const;
  std::default_sentinel_t end() const;
  Obj tail() const;
};

struct ListProfile {
  size_t size;
  Obj tail;

  bool is_proper() const { return tail.is_null(); }
};

void trace_child(Obj obj, std::vector<HeapEntity *> &worklist);

struct HeapEntity {
  virtual void trace(std::vector<HeapEntity *> &) const {}
  virtual ~HeapEntity() = default;
};

class Env : public HeapEntity {
  std::unordered_map<Symbol, Obj> bindings;
  Env *const parent;

public:
  explicit Env(Env *parent = nullptr);

  Env(const Env &) = delete;
  Env &operator=(const Env &) = delete;

  std::optional<Obj> lookup(Symbol name) const;
  void define(Symbol name, Obj value);
  bool set(Symbol name, Obj value);

  void trace(std::vector<HeapEntity *> &) const override;
};

struct String : HeapEntity {
  std::string data;

  String(std::string data);
};

struct Cons : HeapEntity {
  Obj car;
  Obj cdr;

  Cons(Obj car, Obj cdr);
  void trace(std::vector<HeapEntity *> &) const override;
};

struct Vector : HeapEntity {
  std::vector<Obj> data;

  Vector(std::vector<Obj> data);

  void trace(std::vector<HeapEntity *> &) const override;
};

struct Builtin : HeapEntity {
  using Fn =
      std::function<Obj(const std::vector<Obj> &, EvalContext &)>;
  struct Apply {};
  using Implementation = std::variant<Fn, Apply>;

  Implementation implementation;

  Builtin(Implementation implementation);
};

struct Formals {
  std::vector<Symbol> fixed;
  std::optional<Symbol> rest;

  static Formals parse(Obj formals);
  void bind(Env &env, const std::vector<Obj> &args, EvalContext &context) const;
};

enum class ProcedureKind { Function, Macro };

struct Procedure : HeapEntity {
  Formals formals;
  Obj body;
  std::reference_wrapper<Env> env;
  ProcedureKind kind;

  Procedure(Formals formals, Obj body, Env &env, ProcedureKind kind);

  void trace(std::vector<HeapEntity *> &) const override;
};

class Promise : public HeapEntity {
  struct Thunk {
    Obj body;
    std::reference_wrapper<Env> env;
  };

  std::variant<Thunk, Obj> state;

public:
  Promise(Obj body, Env &env);

  Obj force(EvalContext &context);

  void trace(std::vector<HeapEntity *> &) const override;
};

struct Error : HeapEntity {
  std::string message;
  Obj irritants;

  Error(std::string message, Obj irritants);

  std::string describe() const;

  void trace(std::vector<HeapEntity *> &) const override;
};

struct SchemeError : std::runtime_error {
  std::optional<Obj> payload;

  explicit SchemeError(const std::string &message);
  static SchemeError raised(Obj payload);

  Obj as_condition(EvalContext &context);
};

struct CallError : SchemeError {
  using SchemeError::SchemeError;
};
