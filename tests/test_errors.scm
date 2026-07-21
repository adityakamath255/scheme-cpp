(assert "guard no raise" 42 (guard (e (#t 'caught)) 42))
(assert "guard catches error" 'caught (guard (e (#t 'caught)) (error "boom")))
(assert "error-object?" #t (guard (e (#t (error-object? e))) (error "boom")))
(assert "error-object? non-error" #f (error-object? 5))
(assert "error message" "boom"
        (guard (e (#t (error-object-message e))) (error "boom" 1 2)))
(assert "error irritants" '(1 2)
        (guard (e (#t (error-object-irritants e))) (error "boom" 1 2)))
(assert "error no irritants" '()
        (guard (e (#t (error-object-irritants e))) (error "boom")))
(assert "error symbol message" "sym"
        (guard (e (#t (error-object-message e))) (error 'sym)))

(assert "raise value" 42 (guard (e (#t e)) (raise 42)))
(assert "raise not wrapped" #f (guard (e (#t (error-object? e))) (raise 42)))
(assert "raise list" '(a b) (guard (e (#t e)) (raise '(a b))))

(assert "clause dispatch" 'num
        (guard (e ((symbol? e) 'sym) ((number? e) 'num)) (raise 5)))
(assert "else clause" 'other
        (guard (e ((symbol? e) 'sym) (else 'other)) (raise 5)))
(assert "arrow clause" 2
        (guard (e ((assv e '((a 1) (b 2))) => cadr)) (raise 'b)))
(assert "reraise to outer" 'outer
        (guard (e ((number? e) 'outer))
          (guard (e ((symbol? e) 'inner))
            (raise 5))))
(assert "no clauses reraises" 'outer
        (guard (e (#t 'outer))
          (guard (e) (error "boom"))))

(assert "native error caught" 'caught (guard (e (#t 'caught)) (car 5)))
(assert "pair type name" "car: expected pair, got number"
        (guard (e (#t (error-object-message e))) (car 5)))
(assert "native error is error object" #t
        (guard (e (#t (error-object? e))) (vector-ref (vector) 0)))
(assert "division by zero caught" "/: division by zero"
        (guard (e (#t (error-object-message e))) (/ 1 0)))
(assert "builtin alias error name" "exact: not an integer"
        (guard (e (#t (error-object-message e))) (exact 1.5)))
(assert "unbounded arity message" "-: expected 1 or more arguments, got 0"
        (guard (e (#t (error-object-message e))) (-)))
(assert "nested builtin keeps origin" "car: expected pair, got number"
        (guard (e (#t (error-object-message e))) (eval '(car 5))))
(assert "undefined variable caught" 'caught
        (guard (e (#t 'caught)) nonexistent-variable))
(assert "native reraise keeps message" "car: expected pair, got null"
        (guard (e (#t (error-object-message e)))
          (guard (e ((number? e) 'nope))
            (car '()))))

(assert "malformed binding caught" 'caught
        (guard (e (#t 'caught)) (let ((x)) x)))
(assert "accessor error message" "expected pair, got null"
        (guard (e (#t (error-object-message e))) (let ((x)) x)))

(define side 0)
(guard (e (#t 'x)) (set! side 1) (error "after"))
(assert "body effects persist" 1 side)

(define p (guard (e (#t e)) (error "held" 'x)))
(assert "error object first-class" #t (error-object? p))
(assert "eq? same error" #t (eq? p p))

(define (count-fails n acc)
  (if (= n 0) acc
      (count-fails (- n 1)
                   (+ acc (guard (e (#t 1)) (car '()) 0)))))
(assert "guard under load" 10000 (count-fails 10000 0))

(summary)
