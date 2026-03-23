;; measures: procedure (closure) allocation
(define (loop n) (if (= n 0) 0 (begin (lambda (x) x) (loop (- n 1)))))
(loop 500000)
