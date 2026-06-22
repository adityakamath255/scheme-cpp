# scheme-cpp

A Scheme interpreter in C++20. Evaluates S-expressions directly, with tail call optimization via trampolining, a mark-and-sweep garbage collector, and symbol interning.

## Building

Requires CMake 3.16+ and a C++20 compiler (GCC 12+, Clang 15+).

```bash
mkdir -p build && cd build
cmake ..
make
```

The binary is `build/scheme`.

## Usage

```bash
./build/scheme              # start the REPL
./build/scheme file.scm     # run a file and exit
./build/scheme -i file.scm  # run a file, then enter the REPL
```

With no file on a terminal it starts the REPL; piped on stdin (`echo '(+ 1 2)' | ./build/scheme`) it runs the input and exits. `-i` (or `--interactive`) keeps the REPL open after a file.

The REPL supports multi-line input (brackets are tracked across lines), line editing, and history via [replxx](https://github.com/AmokHuginnsson/replxx). Ctrl+D exits. Ctrl+C clears the current input.

## Web

The same interpreter runs in the browser, compiled to WebAssembly: <https://adityakamath255.github.io/scheme-cpp/>

The `src/` sources compile to wasm via Embind. `web/main.cpp` exposes a `Session` whose `step` method reads and evaluates one form at a time, sharing the same read/eval driver as the terminal. `web/index.html` is a two-pane playground: a code editor on the left, results on the right, run with the button or Ctrl/Cmd+Enter. The program persists in `localStorage`, the split between the panes is draggable, "Copy link" encodes the program into a shareable URL, and each run reports its evaluation time. A session keeps its definitions across runs; "Reset session" starts fresh. A GitHub Actions workflow (`.github/workflows/pages.yml`) builds with emscripten and deploys to GitHub Pages on every push to `main`.

Building the wasm locally needs the [emscripten SDK](https://emscripten.org):

```bash
emcmake cmake -S web -B web/build
cmake --build web/build
```

## What's implemented

### Special forms

`define`, `lambda`, `if`, `cond`, `let`, `let*`, `set!`, `begin`, `and`, `or`, `quote`, `quasiquote`, `unquote`, `unquote-splicing`, `define-macro`

`and` and `or` return the deciding value, not a boolean: `(and 1 2 3)` returns `3`, `(or #f 42)` returns `42`. `cond` clauses with no body return the test value: `(cond (5))` returns `5`. Quasiquote works inside vectors: `` `#(1 ,x 3) ``, and `unquote-splicing` (`,@`) splices a list into the surrounding form: `` `(1 ,@'(2 3) 4) `` returns `(1 2 3 4)`.

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

Arithmetic: `+`, `-`, `*`, `/`, `abs`, `sqrt`, `sin`, `cos`, `log`, `expt`, `ceiling`, `floor`, `round`, `max`, `min`, `quotient`, `remainder`, `modulo`, `even?`, `odd?`

Comparison: `<`, `>`, `=`, `<=`, `>=` (all accept multiple arguments and test pairwise)

Lists: `cons`, `car`, `cdr`, `list`, `length`, `list-ref`, `set-car!`, `set-cdr!`

Strings: `string-length`, `string-ref`, `substring`, `string-append`, `string=?`

Vectors: `vector`, `make-vector`, `vector-ref`, `vector-set!`, `vector-length`, `vector->list`, `list->vector`

Type predicates: `null?`, `boolean?`, `number?`, `integer?`, `pair?`, `symbol?`, `string?`, `procedure?`, `list?`, `vector?`, `void?`, `not`

Equality: `eq?` (identity), `equal?` (structural, iterates along cdrs to avoid stack overflow on long lists)

Conversion: `number->string`, `string->number`, `symbol->string`, `string->symbol`

I/O: `display` (unquoted), `write` (quoted), `newline`, `read`

Other: `error`, `eval`, `apply` (variadic: `(apply + 1 2 '(3 4))` works), `void`, `load`, `file-exists?`, `exit`

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

The collector is non-moving mark-and-sweep. All heap-allocated objects (cons cells, strings, vectors, closures, environments) are tracked in a linear list. When the live count exceeds a threshold, the collector walks all objects reachable from the global environment, then deletes everything else. The threshold doubles after each collection.

Strings, booleans, numbers, symbols, and null/void are not heap-allocated. Symbols are interned into an `unordered_set`; two symbols with the same name share a pointer, so symbol comparison is pointer equality.

## Tests

```bash
./run_tests.sh
```

Tests are written in Scheme using a small framework (`tests/framework.scm`) that provides `assert` and `summary`. There are 11 test files covering arithmetic, closures, lists, strings, vectors, special forms, preamble functions, tail calls, and stress scenarios (10k-element lists, heavy GC pressure, deeply nested closures, bulk vector allocation).

## Source layout

The interpreter is 10 source files:

- `lex.cpp` - tokenizer. Returns `nullopt` for incomplete input, which `driver.cpp` turns into an `Incomplete` result so both the terminal and browser REPLs detect multi-line expressions without a separate bracket checker.
- `parse.cpp` - recursive descent parser. Produces S-expressions (cons cells, symbols, literals), not an AST.
- `types.cpp` - the `Obj` class wrapping `std::variant<bool, double, Symbol, String*, Cons*, Vector*, Procedure*, Builtin*, Null, Void>`. Provides type checks, accessors, structural equality, and printing.
- `env.cpp` - lexical environments. An `Env` is a hash map from interned symbols to values, with a parent pointer.
- `ctx.cpp` - the `Ctx` class: arena allocator, symbol intern table, and garbage collector.
- `eval.cpp` - evaluator. Dispatches special forms by symbol name, evaluates procedure calls, implements the tail call trampoline.
- `builtins.cpp` - all built-in procedure implementations, registered as raw function pointers.
- `preamble.cpp` - the standard library, stored as a string literal and evaluated at startup.
- `driver.cpp` - reads and evaluates one top-level form (`read_eval`) or a whole source (`run_all`). The GC-recycle step between forms lives here, and both the terminal REPL and the wasm front-end build on it.
- `main.cpp` - argument parsing, file execution, and the replxx REPL loop.

## Limitations

- No hygienic macros (`define-syntax`, `syntax-rules`). `define-macro` is non-hygienic.
- No continuations (`call/cc`).
- Numbers are IEEE 754 doubles only. No exact integers, rationals, or bignums. Integers above 2^53 lose precision.
- No ports. I/O is stdin/stdout only.
- Deep non-tail recursion is bounded by the C stack: the native binary handles a few thousand frames, and in the browser the wasm build (8MB stack) is capped lower by the JavaScript engine's own call-stack limit. Overflowing it raises a catchable error rather than crashing the session. Tail recursion is unaffected and runs in constant space.
