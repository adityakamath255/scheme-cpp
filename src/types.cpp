#include "types.hpp"
#include "env.hpp"

#include <string>
#include <sstream>
#include <format>

Symbol::Symbol(const std::string *ptr): ptr {ptr} {}

const std::string &Symbol::get_name() const {
  return *ptr;
}

bool Symbol::operator==(Symbol other) const {
  return ptr == other.ptr;
}

Obj::Obj(Value data): data {data} {}

bool Obj::is_bool() const {
  return std::holds_alternative<bool>(data);
}

bool Obj::is_number() const {
  return std::holds_alternative<double>(data);
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

bool Obj::is_callable() const {
  return std::holds_alternative<Callable *>(data);
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

double Obj::as_number() const {
  return std::get<double>(data);
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

Callable *Obj::as_callable() const {
  return std::get<Callable *>(data);
}

std::optional<HeapEntity *> Obj::heap_entity() const {
  if (is_string()) {
    return as_string();
  }

  else if (is_cons()) {
    return as_cons();
  }

  else if (is_callable()) {
    return as_callable();
  }

  else {
    return std::nullopt;
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

  else if (is_null() || is_void()) {
    return true;
  }

  else if (is_bool()) {
    return as_bool() == other.as_bool();
  }
  else if (is_number()) {
    return as_number() == other.as_number();
  }

  else if (is_symbol()) {
    return as_symbol() == other.as_symbol();
  }

  else if (is_callable()) {
    return as_callable() == other.as_callable();
  }

  else if (is_string()) {
    return as_string()->data == other.as_string()->data;
  }

  else if (is_cons()) {
    auto curr_0 = as_cons();
    auto curr_1 = other.as_cons();
    while (true) {
      if (!curr_0->car.equals(curr_1->car)) {
        return false;
      }
      else if (!curr_0->cdr.is_cons() && !curr_1->cdr.is_cons()) {
        return curr_0->cdr.equals(curr_1->cdr);
      }
      else if (curr_0->cdr.is_cons() && curr_1->cdr.is_cons()) {
        curr_0 = curr_0->cdr.as_cons();
        curr_1 = curr_1->cdr.as_cons();
      }
      else {
        return false;
      }
    }
  }

  else {
    return false;
  }
}

std::string Obj::stringify() const {
  if (is_bool()) {
    return as_bool() ? "#t" : "#f";
  }

  else if (is_number()) {
    return std::format("{}", as_number());
  }

  else if (is_symbol()) {
    return as_symbol().get_name();
  }

  else if (is_string()) {
    return as_string()->data;
  }

  else if (is_cons()) {
    std::ostringstream res;
    res << "(" << as_cons()->car.stringify();

    Obj curr = as_cons()->cdr;

    while (curr.is_cons()) {
      res << " " << curr.as_cons()->car.stringify();
      curr = curr.as_cons()->cdr;
    }

    if (!curr.is_null()) {
      res << " . " << curr.stringify();
    }

    res << ")";

    return res.str();
  }

  else if (is_callable()) {
    return std::format(
      "<procedure at {}>", 
      static_cast<const void *>(as_callable())
    );
  }

  else if (is_null()) {
    return "()";
  }
  
  else if (is_void()) {
    return "#<void>";
  }

  else {
    return "???";
  }
}

std::string Obj::stringify_type() const {
  if (is_bool()) {
    return "boolean";
  }

  else if (is_number()) {
    return "number";
  }

  else if (is_symbol()) {
    return "symbol";
  }

  else if (is_string()) {
    return "string";
  }

  else if (is_cons()) {
    return "cons";
  }

  else if (is_callable()) {
    return "procedure";
  }

  else if (is_null()) {
    return "null";
  }

  else if (is_void()) {
    return "void";
  }
  
  else {
    return "???";
  }
}

ListProfile Obj::get_list_profile() const {
  size_t len = 0;
  Obj curr = *this;
  while (curr.is_cons()) {
    len += 1;
    curr = curr.as_cons()->cdr;
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

void String::trace(std::vector<HeapEntity *> *) const {}

Cons::Cons(Obj car, Obj cdr): car {car}, cdr {cdr} {}

void Cons::trace(std::vector<HeapEntity *> *worklist) const {
  if (auto entity = car.heap_entity()) {
    worklist->push_back(*entity);
  }
  if (auto entity = cdr.heap_entity()) {
    worklist->push_back(*entity);
  }
}

Builtin::Builtin(Builtin::Fn fn): fn {fn} {}

Obj Builtin::call(const std::vector<Obj> &args, Ctx *ctx) const {
  return fn(args, ctx);
}

void Builtin::trace(std::vector<HeapEntity *> *) const {}

Procedure::Procedure(
  std::vector<Symbol> params,
  Obj body,
  Env *env,
  bool variadic
): params {std::move(params)}, body {body}, env {env}, variadic {variadic} {}

Obj Procedure::call(const std::vector<Obj> &, Ctx *) const { 
  return Obj(Void{}); 
}

void Procedure::trace(std::vector<HeapEntity *> *worklist) const {
  if (auto entity = body.heap_entity()) {
    worklist->push_back(*entity);
  }
  worklist->push_back(env);
}
