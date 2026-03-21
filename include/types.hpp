#pragma once
#include <vector>
#include <string>
#include <variant>
#include <optional>

struct Symbol;
struct String;
struct Cons;
struct Builtin;
struct Procedure;
struct Callable;
struct Null {};
struct Void {};

using Value = std::variant<
  bool,
  double,
  Symbol,
  String *,
  Cons *,
  Callable *,
  Null,
  Void
>;

class Obj;

class Env;
class Ctx;

struct HeapEntity;
using MarkStack = std::vector<HeapEntity *>;

struct ListProfile;

struct Symbol {
  friend struct std::hash<Symbol>;
  
  const std::string *ptr;

  Symbol(const std::string *);

  const std::string &get_name() const;
  bool operator==(Symbol other) const;
};

class Obj {
private:
  Value data;

public:
  Obj(Value data);

  bool is_bool() const;
  bool is_number() const;
  bool is_symbol() const;
  bool is_string() const;
  bool is_cons() const;
  bool is_callable() const;
  bool is_null() const;
  bool is_void() const;

  bool as_bool() const;
  double as_number() const;
  Symbol as_symbol() const;
  String *as_string() const;
  Cons *as_cons() const;
  Callable *as_callable() const;

  std::optional<HeapEntity *> heap_entity() const;

  bool is_true() const;
  bool is_false() const;

  bool same_type(Obj) const;
  bool equals(Obj) const;

  std::string stringify() const;
  std::string stringify_type() const;

  ListProfile get_list_profile() const;
  size_t get_list_size() const;
  bool is_list() const;

};

struct ListProfile {
  size_t size;
  bool is_proper;
};

struct HeapEntity {
  virtual void trace(MarkStack *) const = 0;
  virtual ~HeapEntity() = default;
};

struct String : HeapEntity {
  std::string data;

  String(std::string data);

  void trace(MarkStack *) const override;
};

struct Cons : HeapEntity {
  Obj car;
  Obj cdr;

  Cons(Obj car, Obj cdr);
  void trace(MarkStack *) const override;
};

struct Callable : HeapEntity {
  virtual Obj call(const std::vector<Obj> &, Ctx *) const = 0;
};

struct Builtin : Callable {
  using Fn = Obj(*)(const std::vector<Obj> &, Ctx *);

  Fn fn;

  Builtin(Fn fn);

  Obj call(const std::vector<Obj> &, Ctx *) const override;
  void trace(MarkStack *) const override;
};

struct Procedure : Callable {
  std::vector<Symbol> params;
  Obj body;
  Env *env;
  bool variadic;

  Procedure(
    std::vector<Symbol> params,
    Obj body,
    Env *env,
    bool variadic
  );

  Obj call(const std::vector<Obj>&, Ctx *) const override;
  void trace(MarkStack *) const override;
};

template<>
struct std::hash<Symbol> {
  size_t operator()(const Symbol &s) const {
    return std::hash<const void*>()(s.ptr);
  }
};
