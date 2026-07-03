#pragma once
#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

struct BigInt;
class Number;
struct Symbol;
struct String;
struct Cons;
struct Vector;
struct Builtin;
struct Procedure;
struct Null {};
struct Void {};

class Obj;
class Env;
class Ctx;
struct HeapEntity;
struct ListProfile;

using Value = std::variant<
  bool,
  char,
  Number,
  Symbol,
  String *,
  Cons *,
  Vector *,
  Procedure *,
  Builtin *,
  Null,
  Void
>;

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
  Null,
  Void
};

class Number {
private:
  std::variant<int64_t, BigInt *, double> rep;
  explicit Number(std::variant<int64_t, BigInt *, double> rep);

public:
  static Number exact(int64_t v, Ctx *ctx);
  static Number inexact(double v);
  static Number parse(std::string_view lexeme, Ctx *ctx);

  bool is_exact() const;
  bool is_integer() const;
  bool is_zero() const;
  bool is_even() const;

  double to_double() const;

  Number add(Number o, Ctx *ctx) const;
  Number sub(Number o, Ctx *ctx) const;
  Number mul(Number o, Ctx *ctx) const;
  Number div(Number o, Ctx *ctx) const;
  Number neg(Ctx *ctx) const;
  Number abs(Ctx *ctx) const;
  Number sqrt(Ctx *ctx) const;
  Number quotient(Number o, Ctx *ctx) const;
  Number remainder(Number o, Ctx *ctx) const;
  Number modulo(Number o, Ctx *ctx) const;
  Number expt(Number power, Ctx *ctx) const;
  Number to_inexact() const;
  Number to_exact(Ctx *ctx) const;

  std::partial_ordering compare(Number o) const;
  bool eqv(Number o) const;

  std::string to_string() const;
  std::optional<HeapEntity *> heap_entity() const;
};

struct Symbol {
  const std::string *ptr;

  Symbol(const std::string *);

  const std::string &get_name() const;
  bool operator==(Symbol other) const;
};

class Obj {
private:
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
  Obj(Null);
  Obj(Void);

  Type get_type() const;

  bool is_bool() const;
  bool is_number() const;
  bool is_char() const;
  bool is_symbol() const;
  bool is_string() const;
  bool is_cons() const;
  bool is_vector() const;
  bool is_procedure() const;
  bool is_builtin() const;
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

  std::optional<HeapEntity *> heap_entity() const;

  bool is_true() const;
  bool is_false() const;

  bool same_type(Obj) const;
  bool equals(Obj) const;

  std::string stringify(bool) const;
  std::string stringify_type() const;

  ListProfile get_list_profile() const;
  size_t get_list_size() const;
  bool is_list() const;

  Obj car() const;
  Obj cdr() const;
};

struct ListProfile {
  size_t size;
  bool is_proper;
};

struct HeapEntity {
  virtual void trace(std::vector<HeapEntity *> *) const {}
  virtual ~HeapEntity() = default;
};

struct String : HeapEntity {
  std::string data;

  String(std::string data);
};

struct Cons : HeapEntity {
  Obj car;
  Obj cdr;

  Cons(Obj car, Obj cdr);
  void trace(std::vector<HeapEntity *> *) const override;
};

struct Vector : HeapEntity {
  std::vector<Obj> data;

  Vector(std::vector<Obj> data);

  void trace(std::vector<HeapEntity *> *) const override;
};

struct Builtin : HeapEntity {
  using Fn = Obj(*)(const std::vector<Obj> &, Ctx *);

  Fn fn;

  Builtin(Fn fn);
};

struct Procedure : HeapEntity {
  std::vector<Symbol> params;
  Obj body;
  Env *env;
  bool variadic;
  bool macro;

  Procedure(
    std::vector<Symbol> params,
    Obj body,
    Env *env,
    bool variadic,
    bool macro
  );

  void trace(std::vector<HeapEntity *> *) const override;
};

template<>
struct std::hash<Symbol> {
  size_t operator()(const Symbol &s) const {
    return std::hash<const void*>()(s.ptr);
  }
};
