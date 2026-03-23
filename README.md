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
./build/scheme file.scm     # run a file, then enter the REPL
./build/scheme -b file.scm  # run a file and exit
```

The REPL supports multi-line input (brackets are tracked across lines), line editing, and history via [replxx](https://github.com/AmokHuginnsson/replxx). Ctrl+D exits. Ctrl+C clears the current input.

## What's implemented

### Special forms

`define`, `lambda`, `if`, `cond`, `let`, `let*`, `set!`, `begin`, `and`, `or`, `quote`, `quasiquote`, `unquote`

`and` and `or` return the deciding value, not a boolean: `(and 1 2 3)` returns `3`, `(or #f 42)` returns `42`. `cond` clauses with no body return the test value: `(cond (5))` returns `5`. Quasiquote works inside vectors: `` `#(1 ,x 3) ``.

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

The interpreter is 9 source files:

- `lex.cpp` - tokenizer. Returns `nullopt` for incomplete input, which the REPL uses to detect multi-line expressions without a separate bracket checker.
- `parse.cpp` - recursive descent parser. Produces S-expressions (cons cells, symbols, literals), not an AST.
- `types.cpp` - the `Obj` class wrapping `std::variant<bool, double, Symbol, String*, Cons*, Vector*, Procedure*, Builtin*, Null, Void>`. Provides type checks, accessors, structural equality, and printing.
- `env.cpp` - lexical environments. An `Env` is a hash map from interned symbols to values, with a parent pointer.
- `ctx.cpp` - the `Ctx` class: arena allocator, symbol intern table, and garbage collector.
- `eval.cpp` - evaluator. Dispatches special forms by symbol name, evaluates procedure calls, implements the tail call trampoline.
- `builtins.cpp` - all built-in procedure implementations, registered as raw function pointers.
- `preamble.cpp` - the standard library, stored as a string literal and evaluated at startup.
- `main.cpp` - argument parsing, file execution, REPL loop.

## Limitations

- No macros (`define-syntax`, `syntax-rules`).
- No continuations (`call/cc`).
- Numbers are IEEE 754 doubles only. No exact integers, rationals, or bignums. Integers above 2^53 lose precision.
- No ports. I/O is stdin/stdout only.
- `unquote-splicing` (`,@`) is parsed but not evaluated.
