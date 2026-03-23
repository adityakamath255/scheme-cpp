;; measures: cons allocation (retained, so no GC)
(define (loop n acc) (if (= n 0) acc (loop (- n 1) (cons n acc))))
(loop 200000 '())
