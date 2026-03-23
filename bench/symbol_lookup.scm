;; measures: eq? dispatch + tail loop (symbols interned at parse time)
(define (loop n)
  (if (= n 0) 'done
      (begin (eq? 'hello 'hello)
             (loop (- n 1)))))
(loop 500000)
