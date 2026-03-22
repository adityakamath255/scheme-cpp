;; build a large list and traverse it
(define big (iota 10000))
(assert "big list length" 10000 (length big))
(assert "big list sum" 49995000 (reduce + 0 big))

;; heavy allocation — map creates many cons cells, triggers GC
(define squares (map (lambda (x) (* x x)) big))
(assert "big map" 100 (list-ref squares 10))

;; deeply nested closures
(define (nest n)
  (if (= n 0)
      (lambda (x) x)
      (let ((inner (nest (- n 1))))
        (lambda (x) (inner (+ x 1))))))
(assert "nested closures" 1000 ((nest 1000) 0))

;; many vector allocations
(define (make-vectors n)
  (if (= n 0) 'done
      (begin (make-vector 100) (make-vectors (- n 1)))))
(make-vectors 10000)
(assert "vector alloc" #t #t)

(summary)
