;; measures: trampoline + eval dispatch + symbol lookup + builtin call
;; each iteration: if, =, -, recursive call
(define (loop n) (if (= n 0) 0 (loop (- n 1))))
(loop 1000000)
