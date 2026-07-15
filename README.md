# scheme-cpp

A Scheme interpreter in C++20. Evaluates S-expressions directly, with tail call optimization via trampolining, a mark-and-sweep garbage collector, and symbol interning.

## Building

Requires CMake 3.16+ and a C++20 compiler (GCC 12+, Clang 15+).

```bash
cmake -S cli -B cli/build
cmake --build cli/build
```

The binary is `cli/build/scheme`.

## Usage

```bash
./cli/build/scheme              # start the REPL
./cli/build/scheme file.scm     # run a file and exit
./cli/build/scheme -i file.scm  # run a file, then enter the REPL
```

With no file on a terminal it starts the REPL; piped on stdin (`echo '(+ 1 2)' | ./cli/build/scheme`) it runs the input and exits. `-i` (or `--interactive`) keeps the REPL open after a file.

The REPL supports multi-line input (brackets are tracked across lines), line editing, and history via [replxx](https://github.com/AmokHuginnsson/replxx). Ctrl+D exits. Ctrl+C clears the current input.

## Web

The same interpreter runs in the browser, compiled to WebAssembly: <https://adityakamath255.github.io/scheme-cpp/>

The `src/` sources compile to wasm via Embind. `web/main.cpp` wraps the same
`scheme::Session` API as the terminal. `web/index.html` is a two-pane
playground: a code editor on the left, results on the right, run with the
button or Ctrl/Cmd+Enter. The program persists in `localStorage`, the split
between the panes is draggable, "Copy link" encodes the program into a
shareable URL, and each run reports its evaluation time. A session keeps its
definitions across runs; "Reset session" starts fresh. A GitHub Actions
workflow (`.github/workflows/pages.yml`) builds with emscripten and deploys to
GitHub Pages on every push to `main`.

Building the wasm locally needs the [emscripten SDK](https://emscripten.org):

```bash
emcmake cmake -S web -B web/build
cmake --build web/build
```

## What's implemented

### Special forms

`define`, `lambda`, `if`, `cond`, `case`, `when`, `unless`, `let`, `let*`, `letrec`, `set!`, `begin`, `and`, `or`, `quote`, `quasiquote`, `unquote`, `unquote-splicing`, `define-macro`, `delay`, `cons-stream`, `guard`

`let` also supports the named form (`(let loop ((i 0)) ...)`) for tail-recursive iteration.

`and` and `or` return the deciding value, not a boolean: `(and 1 2 3)` returns `3`, `(or #f 42)` returns `42`. `cond` clauses with no body return the test value: `(cond (5))` returns `5`. `cond` supports `=>` clauses, which pass the test value to a receiver procedure: `(cond ((assv 'b alist) => cadr))`. The receiver call is a tail call. Quasiquote works inside vectors: `` `#(1 ,x 3) ``, and `unquote-splicing` (`,@`) splices a list into the surrounding form: `` `(1 ,@'(2 3) 4) `` returns `(1 2 3 4)`.

### Macros

`define-macro` defines non-hygienic, Lisp-style macros (as in Common Lisp's `defmacro`, not `syntax-rules`). The macro receives its arguments unevaluated, its body runs to produce an expansion, and that expansion is evaluated in the caller's environment. Quasiquote is the usual way to build the expansion:

```scheme
(define-macro (when test . body)
  `(if ,test (begin ,@body) (void)))

(define-macro (swap! a b)
  `(let ((tmp ,a)) (set! ,a ,b) (set! ,b tmp)))
```

A symbol target binds an existing procedure as a macro: `(define-macro my-if (lambda (c t e) ...))`. Expansion is not hygienic, so identifiers introduced by the expansion (like `tmp` above) can capture or be captured by names at the use site.

### Built-in procedures

Arithmetic: `+`, `-`, `*`, `/`, `abs`, `sqrt`, `sin`, `cos`, `log`, `expt`, `ceiling`, `floor`, `round`, `max`, `min`, `quotient`, `remainder`, `modulo`, `even?`, `odd?`, `zero?`, `positive?`, `negative?`

Exactness: `exact?`, `inexact?`, `exact`, `inexact` (aliases `inexact->exact`, `exact->inexact`)

Comparison: `<`, `>`, `=`, `<=`, `>=` (all accept multiple arguments and test pairwise)

Lists: `cons`, `car`, `cdr`, `list`, `length`, `list-ref`, `set-car!`, `set-cdr!`

Strings: `string-length`, `string-ref`, `substring`, `string-append`, `string=?`

Vectors: `vector`, `make-vector`, `vector-ref`, `vector-set!`, `vector-length`, `vector->list`, `list->vector`

Characters: `char?`, `char=?`, `char->integer`, `integer->char`, `string->list`, `list->string`. `write` prints a char as `#\a` (or `#\space`, `#\newline`, `#\tab`, `#\return`); `display` prints the bare character.

Type predicates: `null?`, `boolean?`, `number?`, `integer?`, `char?`, `pair?`, `symbol?`, `string?`, `procedure?`, `list?`, `vector?`, `promise?`, `void?`, `not`

Equality: `eq?` (identity), `equal?` (structural, iterates along cdrs to avoid stack overflow on long lists)

Conversion: `number->string`, `string->number`, `symbol->string`, `string->symbol`

I/O: `display` (unquoted), `write` (quoted), `newline`, `read`

Other: `error`, `raise`, `error-object?`, `error-object-message`, `error-object-irritants`, `eval`, `apply` (variadic: `(apply + 1 2 '(3 4))` works), `force`, `void`, `load`, `file-exists?`, `exit`

### Errors

`error` raises an error object carrying a message and a list of irritants; `raise` raises any value as-is. `guard` catches, R7RS-style: the body runs, and if something is raised, the variable is bound to it and the clauses are tried like `cond` clauses (including `else` and `=>`); if none matches, the value is re-raised to the next guard out.

```scheme
(guard (e ((symbol? e) (list 'symbol e))
          ((error-object? e) (error-object-message e)))
  (error "out of cheese" 42))          ; "out of cheese"
```

Native errors (wrong types, arity mismatches, division by zero) are catchable too and arrive as error objects. Error objects print as `#<error: message irritants...>`. There is no `with-exception-handler` or `raise-continuable`.

### Promises and streams

`delay` wraps an expression into a promise without evaluating it; `force` evaluates it and memoizes the result, so the expression runs at most once. Forcing a non-promise returns it unchanged. Promises print as `#<promise>`, which also means displaying an infinite stream terminates.

`cons-stream` is the SICP stream constructor: `(cons-stream a b)` evaluates `a` and delays `b`, producing `(cons a (delay b))`.

```scheme
(define (integers-from n) (cons-stream n (integers-from (+ n 1))))
(define integers (integers-from 1))
(stream->list (stream-take (stream-filter even? integers) 5))  ; (2 4 6 8 10)
```

The preamble defines the stream library: `the-empty-stream`, `stream-null?`, `stream-car`, `stream-cdr`, `stream-ref`, `stream-map` (accepts multiple streams), `stream-filter`, `stream-for-each`, `stream-take`, `stream->list`, `stream-enumerate-interval`.

Forcing replaces the stored thunk with its value, dropping the captured environment, so the consumed prefix of a stream can be garbage-collected once nothing else references it.

### Standard library

A preamble written in Scheme is evaluated at startup. It defines:

`caar` through `cddddr`, `map`, `for-each`, `filter`, `reduce`, `fold-right`, `last-pair`, `list-tail`, `append`, `append!`, `reverse`, `zip`, `iota`, `memq`, `member`, `assoc`, `assv`, `any`, `every`, `compose`, `partial`, `curry`

`map` and `for-each` accept multiple lists: `(map + '(1 2) '(3 4))` returns `(4 6)`.

`iota` accepts optional start and step: `(iota 3 10 2)` returns `(10 12 14)`.

## Tail call optimization

Tail calls in `if`, `cond`, `let`, `let*`, `begin`, `and`, `or`, and procedure application are optimized. The evaluator uses a trampoline loop: special forms in tail position return a `TailCall` value instead of recursing, and the outer `eval` loop re-enters the evaluator until a result is produced. This keeps the C stack constant regardless of Scheme recursion depth.

```scheme
(define (loop n)
  (if (= n 0) 'done (loop (- n 1))))
(loop 1000000)  ; runs in constant space
```

Mutual tail recursion works:

```scheme
(define (even? n) (if (= n 0) #t (odd? (- n 1))))
(define (odd? n) (if (= n 0) #f (even? (- n 1))))
(even? 1000000)  ; #t
```

## Garbage collection

The collector is non-moving mark-and-sweep. All heap-allocated objects (cons cells, strings, vectors, closures, environments, bignums) are tracked in a linear list. When the live count exceeds a threshold, the collector walks all objects reachable from the global environment, then deletes everything else. The threshold doubles after each collection.

Booleans, characters, symbols, fixnums, inexact reals, and null/void are not heap-allocated; bignums are. Symbols are interned into an `unordered_set`; two symbols with the same name share a pointer, so symbol comparison is pointer equality.

## Tests

```bash
./run_tests.sh
```

Tests are written in Scheme using a small framework (`tests/framework.scm`) that provides `assert` and `summary`. There are 15 test files covering arithmetic, numbers, closures, lists, strings, chars, vectors, special forms, macros, errors, streams, preamble functions, tail calls, integration, and stress scenarios (10k-element lists, heavy GC pressure, deeply nested closures, bulk vector allocation).

## Source layout

The core interpreter lives in `src/`:

- `lex.cpp` - tokenizer. Returns `nullopt` for incomplete input so clients can
  detect multi-line expressions without a separate bracket checker.
- `parse.cpp` - recursive descent parser. Produces S-expressions (cons cells, symbols, literals), not an AST.
- `types.cpp` - the `Obj` class wrapping `std::variant<bool, char, Number, Symbol, String*, Cons*, Vector*, Procedure*, Builtin*, Null, Void>`, where `Number` is a fixnum/bignum/double variant. Provides type checks, accessors, structural equality, and printing (`to_write`/`to_display`).
- `number.cpp` - the `Number` type: exact fixnums that auto-promote to libtommath bignums on overflow, plus inexact doubles. Arithmetic, comparison, and exact/inexact conversion.
- `env.cpp` - lexical environments. `GlobalEnv` is a hash map from interned symbols to values; `LocalEnv` is a small vector of bindings with a parent pointer.
- `runtime.cpp` - persistent session state: arena allocator, symbol intern
  table, global environment, and garbage collector.
- `eval.cpp` - per-run evaluator. Dispatches special forms by symbol name,
  evaluates procedure calls, emits output, and implements the tail call
  trampoline.
- `builtins.cpp` - all built-in procedure implementations, registered as raw function pointers.
- `preamble.cpp` - the standard library, stored as a string literal and evaluated at startup.
- `source.cpp` - the `Evaluator` source loop. It reports how much input was
  consumed, distinguishes incomplete input from completion, and runs GC
  between top-level forms.
- `session.cpp` - the public `scheme::Session` boundary. It owns the runtime,
  installs builtins and the preamble, and exposes incremental and strict
  execution.

The two front-ends use `scheme::Session` through `include/scheme.hpp`:

- `cli/main.cpp` - argument parsing, file execution, and the replxx REPL loop.
- `web/main.cpp` - wraps `scheme::Session` with Embind (see [Web](#web)),
  compiled to WebAssembly.

## Limitations

- No hygienic macros (`define-syntax`, `syntax-rules`). `define-macro` is non-hygienic.
- No continuations (`call/cc`).
- Numbers are exact integers (fixnum, auto-promoting to arbitrary-precision bignum) or inexact reals (IEEE 754 double). No rationals or complex numbers.
- No ports. I/O is stdin/stdout only.
- Deep non-tail recursion is bounded by the C stack: the native binary handles a few thousand frames, and in the browser the wasm build (8MB stack) is capped lower by the JavaScript engine's own call-stack limit. Overflowing it raises a catchable error rather than crashing the session. Tail recursion is unaffected and runs in constant space.
