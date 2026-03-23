;; measures: environment creation via let
(define (loop n) (if (= n 0) 0 (let ((x n)) (loop (- x 1)))))
(loop 500000)
