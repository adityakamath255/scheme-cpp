(define (loop n)
  (if (= n 0) 'done (loop (- n 1))))
(assert "tail recursion" 'done (loop 100000))

(define (sum-tail n acc)
  (if (= n 0) acc (sum-tail (- n 1) (+ acc n))))
(assert "tail accumulator" 5000050000 (sum-tail 100000 0))

(define (even? n)
  (if (= n 0) #t (odd? (- n 1))))
(define (odd? n)
  (if (= n 0) #f (even? (- n 1))))
(assert "mutual tail recursion" #t (even? 100000))

(summary)
