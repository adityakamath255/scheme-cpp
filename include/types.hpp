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
struct Null {};
struct Void {};

using Value = std::variant<
  bool,
  double,
  Symbol,
  String *,
  Cons *,
  Procedure *,
  Builtin *,
  Null,
  Void
>;

class Obj;

class Env;
class Ctx;

struct HeapEntity;

struct ListProfile;

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
  Obj(double);
  Obj(Symbol);
  Obj(String *);
  Obj(Cons *);
  Obj(Procedure *);
  Obj(Builtin *);
  Obj(Null);
  Obj(Void);

  bool is_bool() const;
  bool is_number() const;
  bool is_symbol() const;
  bool is_string() const;
  bool is_cons() const;
  bool is_procedure() const;
  bool is_builtin() const;
  bool is_null() const;
  bool is_void() const;

  bool as_bool() const;
  double as_number() const;
  Symbol as_symbol() const;
  String *as_string() const;
  Cons *as_cons() const;
  Procedure *as_procedure() const;
  Builtin *as_builtin() const;

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

  Obj car() const;
  Obj cdr() const;
};

struct ListProfile {
  size_t size;
  bool is_proper;
};

struct HeapEntity {
  virtual void trace(std::vector<HeapEntity *> *worklist) const = 0;
  virtual ~HeapEntity() = default;
};

struct String : HeapEntity {
  std::string data;

  String(std::string data);

  void trace(std::vector<HeapEntity *> *) const override;
};

struct Cons : HeapEntity {
  Obj car;
  Obj cdr;

  Cons(Obj car, Obj cdr);
  void trace(std::vector<HeapEntity *> *) const override;
};

struct Builtin : HeapEntity {
  using Fn = Obj(*)(const std::vector<Obj> &, Ctx *);

  Fn fn;

  Builtin(Fn fn);

  void trace(std::vector<HeapEntity *> *) const override;
};

struct Procedure : HeapEntity {
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

  void trace(std::vector<HeapEntity *> *) const override;
};

template<>
struct std::hash<Symbol> {
  size_t operator()(const Symbol &s) const {
    return std::hash<const void*>()(s.ptr);
  }
};
