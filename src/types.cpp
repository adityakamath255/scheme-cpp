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
Obj::Obj(bool data): data {data} {}
Obj::Obj(double data): data {data} {}
Obj::Obj(Symbol data): data {data} {}
Obj::Obj(String *data): data {data} {}
Obj::Obj(Cons *data): data {data} {}
Obj::Obj(Vector *data): data {data} {}
Obj::Obj(Procedure *data): data {data} {}
Obj::Obj(Builtin *data): data {data} {}
Obj::Obj(Null data): data {data} {}
Obj::Obj(Void data): data {data} {}

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
  if (is_string()) {
    return as_string();
  }

  else if (is_cons()) {
    return as_cons();
  }

  else if (is_vector()) {
    return as_vector();
  }

  else if (is_procedure()) {
    return as_procedure();
  }

  else if (is_builtin()) {
    return as_builtin();
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

  else if (is_procedure()) {
    return as_procedure() == other.as_procedure();
  }

  else if (is_builtin()) {
    return as_builtin() == other.as_builtin();
  }

  else if (is_string()) {
    return as_string()->data == other.as_string()->data;
  }

  else if (is_cons()) {
    auto curr_0 = *this;
    auto curr_1 = other;
    while (true) {
      if (!curr_0.car().equals(curr_1.car())) {
        return false;
      }
      else if (!curr_0.cdr().is_cons() && !curr_1.cdr().is_cons()) {
        return curr_0.cdr().equals(curr_1.cdr());
      }
      else if (curr_0.cdr().is_cons() && curr_1.cdr().is_cons()) {
        curr_0 = curr_0.cdr();
        curr_1 = curr_1.cdr();
      }
      else {
        return false;
      }
    }
  }

  else if (is_vector()) {
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

  else {
    return false;
  }
}

std::string Obj::stringify(bool quote) const {
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
  }

  else if (is_cons()) {
    std::ostringstream res;
    res << "(" << car().stringify(quote);

    Obj curr = cdr();

    while (curr.is_cons()) {
      res << " " << curr.car().stringify(quote);
      curr = curr.cdr();
    }

    if (!curr.is_null()) {
      res << " . " << curr.stringify(quote);
    }

    res << ")";

    return res.str();
  }

  else if (is_vector()) {
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

  else if (is_procedure()) {
    return std::format(
      "<procedure at {}>", 
      static_cast<const void *>(as_procedure())
    );
  }

  else if (is_builtin()) {
    return std::format(
      "<procedure at {}>",
      static_cast<const void *>(as_builtin())
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

  else if (is_vector()) {
    return "vector";
  }

  else if (is_procedure() || is_builtin()) {
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

Obj Obj::car() const {
  return as_cons()->car;
}

Obj Obj::cdr() const {
  return as_cons()->cdr;
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

Vector::Vector(std::vector<Obj> data): data {std::move(data)} {}

void Vector::trace(std::vector<HeapEntity *> *worklist) const {
  for (auto obj : data) {
    if (auto entity = obj.heap_entity()) {
      worklist->push_back(*entity);
    }
  }
}

Builtin::Builtin(Builtin::Fn fn): fn {fn} {}

void Builtin::trace(std::vector<HeapEntity *> *) const {}

Procedure::Procedure(
  std::vector<Symbol> params,
  Obj body,
  Env *env,
  bool variadic
): params {std::move(params)}, body {body}, env {env}, variadic {variadic} {}

void Procedure::trace(std::vector<HeapEntity *> *worklist) const {
  if (auto entity = body.heap_entity()) {
    worklist->push_back(*entity);
  }
  worklist->push_back(env);
}
