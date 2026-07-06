(define big (iota 2000))
(assert "big list length" 2000 (length big))
(assert "big list sum" 1999000 (reduce + 0 big))

(define squares (map (lambda (x) (* x x)) big))
(assert "big map" 100 (list-ref squares 10))

(define (nest n)
  (if (= n 0)
      (lambda (x) x)
      (let ((inner (nest (- n 1))))
        (lambda (x) (inner (+ x 1))))))
(assert "nested closures" 500 ((nest 500) 0))

(define (make-vectors n)
  (if (= n 0) 'done
      (begin (make-vector 100) (make-vectors (- n 1)))))
(make-vectors 10000)
(assert "vector alloc" #t #t)

(summary)
