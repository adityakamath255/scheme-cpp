#pragma once
#include "number.hpp"
#include "scheme/session.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

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
class Ctx;
class Expr;
class LambdaExpr;
struct HeapEntity;

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

class Symbol {
  const std::string *ptr;

  explicit Symbol(const std::string &);

  friend class Ctx;
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

  bool is_list() const;

  Obj car() const;
  Obj cdr() const;
};

struct List {
  std::vector<Obj> elements;
  Obj tail;

  explicit List(Obj value);

  bool proper() const;
};

void trace_child(Obj obj, std::vector<const HeapEntity *> &worklist);

struct HeapEntity {
  virtual void trace(std::vector<const HeapEntity *> &) const {}
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

  void trace(std::vector<const HeapEntity *> &) const override;
};

struct String : HeapEntity {
  std::string data;

  String(std::string data);
};

struct Cons : HeapEntity {
  Obj car;
  Obj cdr;

  Cons(Obj car, Obj cdr);
  void trace(std::vector<const HeapEntity *> &) const override;
};

struct Vector : HeapEntity {
  std::vector<Obj> data;

  Vector(std::vector<Obj> data);

  void trace(std::vector<const HeapEntity *> &) const override;
};

struct Builtin : HeapEntity {
  using Fn =
      std::function<Obj(const std::vector<Obj> &, Ctx &)>;
  struct Apply {};
  using Implementation = std::variant<Fn, Apply>;

  Implementation implementation;

  Builtin(Implementation implementation);
};

struct Formals {
  const std::vector<Symbol> fixed;
  const std::optional<Symbol> rest;

  static Formals parse(Obj formals);
  void bind(Env &env, const std::vector<Obj> &args, Ctx &context) const;
};

struct Procedure : HeapEntity {
  const LambdaExpr *const code;
  const std::reference_wrapper<Env> env;

  Procedure(const LambdaExpr *code, Env &env);

  void trace(std::vector<const HeapEntity *> &) const override;
};

class Promise : public HeapEntity {
  struct Thunk {
    const Expr *body;
    std::reference_wrapper<Env> env;
  };

  std::variant<Thunk, Obj> state;

public:
  Promise(const Expr *body, Env &env);

  Obj force(Ctx &context);

  void trace(std::vector<const HeapEntity *> &) const override;
};

struct Error : HeapEntity {
  std::string message;
  Obj irritants;

  Error(std::string message, Obj irritants);

  std::string describe() const;

  void trace(std::vector<const HeapEntity *> &) const override;
};

struct SchemeError : scheme::EvaluationError {
  std::optional<Obj> payload;

  explicit SchemeError(const std::string &message);
  static SchemeError raised(Obj payload);

  Obj as_condition(Ctx &context);
};

struct CallError : SchemeError {
  using SchemeError::SchemeError;
};
