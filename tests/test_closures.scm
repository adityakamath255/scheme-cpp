(define (make-counter)
  (let ((n 0))
    (lambda ()
      (set! n (+ n 1))
      n)))

(define c (make-counter))
(assert "closure 1" 1 (c))
(assert "closure 2" 2 (c))
(assert "closure 3" 3 (c))

(define (make-adder x)
  (lambda (y) (+ x y)))

(define add5 (make-adder 5))
(assert "adder" 12 (add5 7))

(assert "lambda immediate" 9 ((lambda (x) (* x x)) 3))

(define (apply-twice f x) (f (f x)))
(assert "higher-order" 4 (apply-twice (lambda (x) (+ x 1)) 2))

(assert "apply" 10 (apply + '(1 2 3 4)))
(assert "apply variadic" 10 (apply + 1 2 '(3 4)))

(summary)
