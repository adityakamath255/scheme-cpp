;; ---- bank account ----

(define (make-account balance)
  (define (withdraw amount)
    (if (>= balance amount)
        (begin (set! balance (- balance amount))
               balance)
        (error "insufficient funds")))
  (define (deposit amount)
    (set! balance (+ balance amount))
    balance)
  (define (dispatch msg)
    (cond ((eq? msg 'withdraw) withdraw)
          ((eq? msg 'deposit) deposit)
          ((eq? msg 'balance) balance)
          (else (error "unknown message" msg))))
  dispatch)

(define acc (make-account 100))
((acc 'deposit) 50)
((acc 'withdraw) 30)
(assert "account balance" 120 (acc 'balance))

;; ---- symbolic differentiation ----

(define (deriv exp var)
  (cond ((number? exp) 0)
        ((symbol? exp) (if (eq? exp var) 1 0))
        ((eq? (car exp) '+)
         (list '+ (deriv (cadr exp) var) (deriv (caddr exp) var)))
        ((eq? (car exp) '*)
         (list '+
               (list '* (cadr exp) (deriv (caddr exp) var))
               (list '* (deriv (cadr exp) var) (caddr exp))))
        (else (error "unknown expression type" exp))))

(assert "deriv const" 0 (deriv 5 'x))
(assert "deriv var" 1 (deriv 'x 'x))
(assert "deriv sum" '(+ 1 0) (deriv '(+ x y) 'x))
(assert "deriv product" '(+ (* x 0) (* 1 y)) (deriv '(* x y) 'x))

;; ---- church numerals ----

(define zero (lambda (f) (lambda (x) x)))
(define (succ n) (lambda (f) (lambda (x) (f ((n f) x)))))
(define (church->int n) ((n (lambda (x) (+ x 1))) 0))

(define one (succ zero))
(define two (succ one))
(define three (succ two))

(assert "church 0" 0 (church->int zero))
(assert "church 3" 3 (church->int three))

(summary)
