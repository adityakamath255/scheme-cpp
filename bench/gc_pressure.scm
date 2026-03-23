;; measures: GC cost (allocate + immediately discard)
(define (loop n) (if (= n 0) 0 (begin (cons 1 2) (loop (- n 1)))))
(loop 1000000)
