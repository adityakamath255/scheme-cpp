;; if
(assert "if true" 1 (if #t 1 2))
(assert "if false" 2 (if #f 1 2))
(assert "if no else" #t (void? (if #f 1)))

;; cond
(assert "cond" 'b (cond (#f 'a) (#t 'b) (else 'c)))
(assert "cond else" 'c (cond (#f 'a) (#f 'b) (else 'c)))
(assert "cond no body" 5 (cond (5)))

;; let / let*
(assert "let" 3 (let ((x 1) (y 2)) (+ x y)))
(assert "let*" 3 (let* ((x 1) (y (+ x 1))) (+ x y)))

;; and / or
(assert "and true" 3 (and 1 2 3))
(assert "and false" #f (and 1 #f 3))
(assert "and empty" #t (and))
(assert "or true" 1 (or 1 2 3))
(assert "or false" #f (or #f #f #f))
(assert "or empty" #f (or))

;; begin
(assert "begin" 3 (begin 1 2 3))

;; define + set!
(define x 10)
(set! x 20)
(assert "set!" 20 x)

;; quote
(assert "quote" '(1 2 3) (quote (1 2 3)))

;; quasiquote
(assert "quasiquote" '(1 2 3) (let ((x 2)) `(1 ,x 3)))
(assert "quasiquote vector" #(1 2 3) (let ((x 2)) `#(1 ,x 3)))

;; variadic lambda
(define (f . args) args)
(assert "variadic" '(1 2 3) (f 1 2 3))

;; rest args
(define (g x . rest) rest)
(assert "rest args" '(2 3) (g 1 2 3))

(summary)
