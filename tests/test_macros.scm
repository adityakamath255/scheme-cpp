;; define-macro shorthand
(define-macro (when test . body)
  `(if ,test (begin ,@body)))

(assert "when true" 3 (when #t 1 2 3))
(assert "when false" #t (void? (when #f (error "boom"))))

;; unquote-splicing
(define xs '(2 3 4))
(assert "splice" '(1 2 3 4 5) `(1 ,@xs 5))
(assert "splice empty" '(1 2) `(1 ,@'() 2))

;; define-macro with expression
(define-macro my-unless
  (lambda (test . body)
    `(if (not ,test) (begin ,@body))))

(assert "macro from lambda" 42 (my-unless #f 42))
(assert "macro from lambda false" #t (void? (my-unless #t 42)))

;; macro expanding to macro
(define-macro (swap! a b)
  `(let ((tmp ,a))
     (set! ,a ,b)
     (set! ,b tmp)))

(define a 1)
(define b 2)
(swap! a b)
(assert "swap a" 2 a)
(assert "swap b" 1 b)

;; macro does not evaluate args
(define evaluated #f)
(define-macro (ignore-args . args) 42)
(ignore-args (set! evaluated #t))
(assert "unevaluated args" #f evaluated)

;; local binding shadows macro
(define-macro (shadowed) 'macro)
(assert "shadow" 99 (let ((shadowed 99)) shadowed))

;; macro with no body args
(define-macro (always-five) 5)
(assert "no args macro" 5 (always-five))

(summary)
