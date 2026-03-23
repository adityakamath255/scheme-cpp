;; measures: raw eval dispatch (tree recursion, no allocation, no GC)
(define (fib n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))
(fib 30)
