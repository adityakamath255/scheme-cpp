# scheme-cpp

`scheme-cpp` is a C++23 interpreter for an R7RS-inspired subset of Scheme. It is available as a native CLI, an embeddable static library, and a [WebAssembly application](https://adityakamath255.github.io/scheme-cpp). It supports proper tail calls, arbitrary-precision integers, mark-and-sweep garbage collection, and lazy streams.

## Running locally

Building requires CMake 3.20+ and a C++23 compiler.

```bash
cmake -S . -B build/native
cmake --build build/native

./build/native/scheme              # enter the REPL directly
./build/native/scheme file.scm     # run a file and exit
./build/native/scheme -i file.scm  # run a file and then enter the REPL
```

Input can also be piped through standard input:

```bash
echo '(+ 1 2)' | ./build/native/scheme
```

The REPL supports:

- Multi-line input, with brackets tracked across lines.
- Line editing.
- History via [replxx](https://github.com/AmokHuginnsson/replxx).
- Clearing the input via Ctrl+C.
- Exiting with Ctrl+D.

## API

The interpreter is a static library whose interface is defined in `include/scheme/session.hpp`. Both the CLI and WebAssembly frontends use this library. Applications can embed the library, provide their own interface, and handle output, results, and exit requests without depending on the CLI.

For example:

```cpp
#include <scheme/session.hpp>

#include <iostream>
#include <variant>

int main() {
  scheme::Session session;
  session.execute("(display (+ 1 2))", [](const scheme::Event &event) {
    if (auto *output = std::get_if<scheme::Output>(&event)) {
      std::cout << output->text;
    }
  });
}
```

- `Session::run` accepts incomplete input and reports how many source bytes it consumed.
- `Session::execute` rejects incomplete input.
- Events are emitted synchronously, and definitions persist for the lifetime of a session.
- A session cannot be called recursively from its own event callback.
- Evaluation failures throw `scheme::EvaluationError`.
- The Scheme `exit` procedure throws `scheme::ExitRequest`, leaving process termination to the host application.

## Implementation

A `Session` owns a `Ctx`, which owns the global environment, managed heap, interned symbols, macro table, and execution state.

The CLI, WebAssembly frontend, or embedding application passes source text to a `Session`. The session delegates execution to `Ctx::run` and emits `Output` and `Result` events through the host-provided callback.

The main execution path is:

```text
source text
-> lexer
-> reader
-> datum (Obj)
-> parsing and macro expansion
-> expression (Expr)
-> evaluator and trampoline
-> value (Obj)
-> Result event (for non-void values)
```

Procedural macro expansion can re-enter the evaluator. Procedures such as `display` and `write` emit `Output` events during evaluation rather than at the end of the pipeline.

Scheme data and evaluated values are represented by `Obj`, which wraps a `std::variant`. `Number` separately wraps fixnums, bignums, and inexact doubles. A bignum never stores a value representable as a fixnum. Fixnums range from `-(2^63) + 1` through `2^63 - 1`; the exclusion of `-(2^63)` keeps negation and absolute value closed over fixnums. Exact integers outside that range use heap-allocated [LibTomMath](https://github.com/libtom/libtommath) bignums. Bignum results convert back to fixnums when they fit.

Expression evaluation returns either a value or a `TailCall`. A trampoline executes tail calls without growing the C stack.

The runtime uses non-moving mark-and-sweep garbage collection. The global environment and macro table form the root set. Collection runs between top-level forms when the managed heap exceeds its threshold. After collection, the threshold becomes twice the surviving object count.

Symbols are interned using a `std::unordered_set<std::string>` for the lifetime of the session. Equal symbol names therefore share one stored string and can be compared by identity.

## Features

### Special forms

`define`, `lambda`, `if`, `cond` (including `=>` clauses), `case`, `when`, `unless`, `let`, named `let`, `let*`, `letrec`, `set!`, `begin`, `and`, `or`, `quote`, `quasiquote`, `unquote`, `unquote-splicing`, `define-macro`, `delay`, `cons-stream`, `guard`

### Built-in procedures

- Arithmetic: `+`, `-`, `*`, `/`, `abs`, `sqrt`, `sin`, `cos`, `log`, `expt`, `ceiling`, `floor`, `round`, `max`, `min`, `quotient`, `remainder`, `modulo`,
`even?`, `odd?`, `zero?`, `positive?`, `negative?`
- Exactness: `exact?`, `inexact?`, `exact`, `inexact` (aliases `inexact->exact`, `exact->inexact`)
- Comparison: `<`, `>`, `=`, `<=`, `>=` (all accept multiple arguments and test pairwise)
- Lists: `cons`, `car`, `cdr`, `list`, `length`, `list-ref`, `set-car!`, `set-cdr!`
- Strings: `string-length`, `string-ref`, `substring`, `string-append`, `string=?`
- Vectors: `vector`, `make-vector`, `vector-ref`, `vector-set!`, `vector-length`, `vector->list`, `list->vector`
- Characters: `char=?`, `char->integer`, `integer->char`, `string->list`, `list->string`
- Type predicates: `null?`, `boolean?`, `number?`, `integer?`, `char?`, `pair?`, `symbol?`, `string?`, `procedure?`, `list?`, `vector?`, `error-object?`, `promise?`, `void?`
- Equality: `eq?`, `equal?`, `eqv?`
- Conversion: `number->string`, `string->number`, `symbol->string`, `string->symbol`
- I/O: `display`, `write`, `newline`, `read`
- Other: `not`, `error`, `raise`, `error-object-message`, `error-object-irritants`, `eval`, `apply` (variadic), `force`, `void`, `load`, `file-exists?`, `exit`

### Standard library

- Lists: `caar` through `cddddr`, `map`, `for-each`, `filter`, `reduce`, `fold-right`, `last-pair`, `list-tail`, `append`, `append!`, `reverse`, `zip`, `iota`, `memq`, `member`, `assoc`, `assv`, `any`, `every`

- Streams: `the-empty-stream`, `stream-null?`, `stream-car`, `stream-cdr`, `stream-ref`, `stream-map`, `stream-filter`, `stream-for-each`, `stream-take`, `stream->list`, `stream-enumerate-interval`

- Higher-order functions: `compose`, `partial`, `curry`

`map` and `for-each` accept multiple lists. `stream-map` and `stream-for-each` accept multiple streams. Each stops when its shortest input ends.

## Tests

After building the native target, run its API and language tests with:

```bash
ctest --test-dir build/native --output-on-failure
```

The complete suite also tests the WebAssembly module through its JavaScript interface. It requires the Emscripten SDK and Node.js:

```bash
./web/build.sh
./run_tests.sh
```

`run_tests.sh` expects both `build/native` and `build/wasm` to exist.

## Limitations

- Macros are non-hygienic. `define-syntax` and `syntax-rules` are not implemented.
- Continuations such as `call/cc` are not implemented.
- There are no ports. `read` uses standard input, while output is delivered through the session callback.
- Numbers are limited to exact integers and inexact doubles. Rational and complex numbers are not implemented.
- Deep reading, parsing, and non-tail evaluation are bounded by a recursion limit. Proper tail calls are unaffected.
- Garbage collection runs only between top-level forms, so allocations cannot be reclaimed while a top-level form is executing.
