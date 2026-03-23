;; measures: string allocation via repeated append
(define (loop n acc)
  (if (= n 0) (string-length acc)
      (loop (- n 1) (string-append acc "x"))))
(loop 10000 "")
