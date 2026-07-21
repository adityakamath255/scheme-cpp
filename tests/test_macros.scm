(define-macro (my-when test . body)
  `(if ,test (begin ,@body)))

(assert "when true" 3 (my-when #t 1 2 3))
(assert "when false" #t (void? (my-when #f (error "boom"))))

(define xs '(2 3 4))
(assert "splice" '(1 2 3 4 5) `(1 ,@xs 5))
(assert "splice empty" '(1 2) `(1 ,@'() 2))

(define-macro my-unless
  (lambda (test . body)
    `(if (not ,test) (begin ,@body))))

(assert "macro from lambda" 42 (my-unless #f 42))
(assert "macro from lambda false" #t (void? (my-unless #t 42)))

(define-macro (swap! a b)
  `(let ((tmp ,a))
     (set! ,a ,b)
     (set! ,b tmp)))

(define a 1)
(define b 2)
(swap! a b)
(assert "swap a" 2 a)
(assert "swap b" 1 b)

(define evaluated #f)
(define-macro (ignore-args . args) 42)
(ignore-args (set! evaluated #t))
(assert "unevaluated args" #f evaluated)

(define-macro (shadowed) 'macro)
(assert "shadow" 99 (let ((shadowed 99)) shadowed))

(define-macro (always-five) 5)
(assert "no args macro" 5 (always-five))

(define expansion-value 1)
(define-macro (captured-expansion) expansion-value)
(define (use-captured-expansion) (captured-expansion))
(set! expansion-value 2)
(assert "expanded once" 1 (use-captured-expansion))

(assert "macro is not a runtime value" 'undefined
        (guard (e (#t 'undefined)) captured-expansion))

(define-macro (reserved-operator x) `(+ ,x 1))
(assert "macro name reserved in operator position" 2
        (let ((reserved-operator (lambda (x) 99)))
          (reserved-operator 1)))

(define expansion-order '())
(define-macro (record-expansion tag form)
  (set! expansion-order (cons tag expansion-order))
  form)
(define (ordered-expansions)
  ((record-expansion operator +)
   (record-expansion first 1)
   (record-expansion second 2)))
(assert "expansion order" '(second first operator) expansion-order)
(assert "ordered expansion result" 3 (ordered-expansions))

(begin
  (define (macro-helper x) `(+ ,x 1))
  (define-macro (uses-helper x) (macro-helper x))
  (define helper-result (uses-helper 41)))
(assert "top-level begin is sequential" 42 helper-result)

(assert "local macro rejected" 'caught
        (guard (e (#t 'caught))
          (eval '((lambda ()
                    (define-macro (local) 1)
                    (local))))))

(define macro-gc-junk (iota 5000))
(assert "macro survives collection" 5 (always-five))

(summary)
