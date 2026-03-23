;; measures: vector allocation + access
(define (loop n)
  (if (= n 0) 'done
      (let ((v (make-vector 100 0)))
        (vector-set! v 50 n)
        (vector-ref v 50)
        (loop (- n 1)))))
(loop 100000)
