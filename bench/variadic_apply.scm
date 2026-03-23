;; measures: variadic apply (list construction + builtin dispatch)
(define (loop n)
  (if (= n 0) 'done
      (begin (apply + 1 2 (list 3 4 5))
             (loop (- n 1)))))
(loop 50000)
