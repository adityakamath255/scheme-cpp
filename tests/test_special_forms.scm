(assert "if true" 1 (if #t 1 2))
(assert "if false" 2 (if #f 1 2))
(assert "if no else" #t (void? (if #f 1)))

(assert "cond" 'b (cond (#f 'a) (#t 'b) (else 'c)))
(assert "cond else" 'c (cond (#f 'a) (#f 'b) (else 'c)))
(assert "cond no body" 5 (cond (5)))
(assert "cond =>" 2 (cond ((assv 'b '((a 1) (b 2))) => cadr) (else 'no)))
(assert "cond => lambda" 10 (cond (5 => (lambda (x) (* x 2)))))
(assert "cond => skipped on false" 'no (cond (#f => car) (else 'no)))

(define (arrow-loop n)
  (cond ((= n 0) 'done)
        (n => (lambda (m) (arrow-loop (- m 1))))))
(assert "cond => tail call" 'done (arrow-loop 100000))

(assert "let" 3 (let ((x 1) (y 2)) (+ x y)))
(assert "let*" 3 (let* ((x 1) (y (+ x 1))) (+ x y)))
(assert "letrec mutual" #t
        (letrec ((even? (lambda (n) (if (= n 0) #t (odd? (- n 1)))))
                 (odd? (lambda (n) (if (= n 0) #f (even? (- n 1))))))
          (even? 10)))

(assert "named let" 55
        (let loop ((i 10) (acc 0))
          (if (= i 0) acc (loop (- i 1) (+ acc i)))))
(assert "named let tail" 500000
        (let loop ((i 0))
          (if (= i 500000) i (loop (+ i 1)))))

(assert "when true" 3 (when #t 1 2 3))
(assert "when false" #t (void? (when #f (error "boom"))))
(assert "when empty" #t (void? (when #t)))
(assert "unless false" 3 (unless #f 1 2 3))
(assert "unless true" #t (void? (unless #t (error "boom"))))

(assert "case hit" 'even (case (* 2 3) ((1 3 5) 'odd) ((0 2 4 6) 'even)))
(assert "case else" 'other (case 9 ((1 2) 'small) (else 'other)))
(assert "case no match" #t (void? (case 9 ((1 2) 'small))))
(assert "case symbol" 'yes (case 'b ((a) 'no) ((b c) 'yes)))

(assert "and true" 3 (and 1 2 3))
(assert "and false" #f (and 1 #f 3))
(assert "and empty" #t (and))
(assert "or true" 1 (or 1 2 3))
(assert "or false" #f (or #f #f #f))
(assert "or empty" #f (or))

(assert "begin" 3 (begin 1 2 3))

(define x 10)
(set! x 20)
(assert "set!" 20 x)

(assert "quote" '(1 2 3) (quote (1 2 3)))

(assert "quasiquote" '(1 2 3) (let ((x 2)) `(1 ,x 3)))
(assert "quasiquote vector" #(1 2 3) (let ((x 2)) `#(1 ,x 3)))

(define (f . args) args)
(assert "variadic" '(1 2 3) (f 1 2 3))

(define (g x . rest) rest)
(assert "rest args" '(2 3) (g 1 2 3))

(summary)
