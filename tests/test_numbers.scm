(define (fact n) (if (= n 0) 1 (* n (fact (- n 1)))))

(assert "fixnum add exact" "6" (number->string (+ 1 2 3)))
(assert "fixnum mul exact" "120" (number->string (* 1 2 3 4 5)))
(assert "fixnum sub exact" "-5" (number->string (- 5 10)))

(assert "int64 max literal" "9223372036854775807"
        (number->string 9223372036854775807))
(assert "add overflows to bignum" "9223372036854775808"
        (number->string (+ 9223372036854775807 1)))
(assert "mul overflows to bignum" "18446744073709551614"
        (number->string (* 9223372036854775807 2)))

(assert "20! (fits fixnum)" "2432902008176640000" (number->string (fact 20)))
(assert "21! (bignum)" "51090942171709440000" (number->string (fact 21)))
(assert "25!" "15511210043330985984000000" (number->string (fact 25)))
(assert "30!" "265252859812191058636308480000000" (number->string (fact 30)))
(assert "2^100" "1267650600228229401496703205376" (number->string (expt 2 100)))
(assert "big literal = expt" #t (= 100000000000000000000 (expt 10 20)))
(assert "big product exact" #t
        (eqv? (* 1000000000000 1000000000000) 1000000000000000000000000))

(assert "bignum minus bignum -> fixnum" "5"
        (number->string (- (+ (expt 2 64) 5) (expt 2 64))))
(assert "shrunk value is exact" #t
        (eqv? 5 (- (+ (expt 2 64) 5) (expt 2 64))))

(assert "eqv same exact" #t (eqv? 3 3))
(assert "eqv exact vs inexact" #f (eqv? 3 3.0))
(assert "equal exact vs inexact" #f (equal? 3 3.0))
(assert "= ignores exactness" #t (= 3 3.0))
(assert "eqv bignum by value" #t (eqv? (expt 2 100) (expt 2 100)))

(assert "exact + exact -> exact" "3" (number->string (+ 1 2)))
(assert "exact + inexact -> inexact" "3.0" (number->string (+ 1 2.0)))
(assert "exact * inexact -> inexact" "6.0" (number->string (* 2 3.0)))
(assert "bignum + inexact -> inexact" #t
        (eqv? (+ (expt 10 20) 0.0) 1e20))

(assert "fixnum < bignum" #t (< 1000000 (expt 10 30)))
(assert "bignum > fixnum" #t (> (expt 10 30) 999999999))
(assert "neg bignum < 0" #t (< (- (expt 10 30)) 0))
(assert "neg bignum < fixnum" #t (< (- (expt 2 64)) 1))
(assert "bignum ordering" #t (< (expt 2 64) (expt 2 65)))
(assert "= bignum" #t (= (expt 2 100) (expt 2 100)))
(assert ">= bignum equal" #t (>= (expt 2 64) (expt 2 64)))

(assert "quotient exact" 3 (quotient 10 3))
(assert "quotient neg trunc" -3 (quotient -10 3))
(assert "remainder neg" -1 (remainder -7 3))
(assert "modulo neg" 2 (modulo -7 3))
(assert "quotient bignum" "12500000000000000000"
        (number->string (quotient (expt 10 20) 8)))
(assert "remainder bignum zero" 0 (remainder (expt 10 20) 8))
(assert "modulo bignum" 1 (modulo (+ 1 (expt 10 20)) 2))

(assert "expt small" 1024 (expt 2 10))
(assert "expt 0^0" 1 (expt 0 0))
(assert "expt exact big" "100000000000000000000" (number->string (expt 10 20)))
(assert "expt negative exp -> inexact" #t (eqv? (expt 2 -1) 0.5))

(assert "/ divides -> exact" "4" (number->string (/ 12 3)))
(assert "/ not divisible -> inexact" #t (eqv? (/ 1 2) 0.5))
(assert "/ big exact" "100000000000000000000"
        (number->string (/ (expt 10 40) (expt 10 20))))

(assert "abs bignum" "1000000000000000000000" (number->string (abs (- (expt 10 21)))))
(assert "neg bignum equals 0 - x" #t (= (- (expt 2 64)) (- 0 (expt 2 64))))

(assert "floor of exact -> exact" 5 (floor 5))
(assert "floor of inexact -> inexact" #t (eqv? (floor 3.7) 3.0))
(assert "ceiling of exact -> exact" 7 (ceiling 7))
(assert "round of inexact -> inexact" #t (eqv? (round 3.5) 4.0))

(assert "max all exact" 5 (max 1 5 3))
(assert "max with inexact -> inexact" #t (eqv? (max 1 5.0 3) 5.0))
(assert "min with inexact -> inexact" #t (eqv? (min 1.0 5 3) 1.0))

(assert "string->number bignum roundtrip" "123456789012345678901234567890"
        (number->string (string->number "123456789012345678901234567890")))
(assert "string->number = literal" #t
        (= (string->number "18446744073709551616") (expt 2 64)))
(assert "string->number float" #t (eqv? (string->number "2.5") 2.5))
(assert "string->number invalid" #f (string->number "abc"))

(assert "even? bignum" #t (even? (expt 2 100)))
(assert "odd? bignum" #t (odd? (+ 1 (expt 2 100))))
(assert "integer? bignum" #t (integer? (expt 2 100)))
(assert "integer? inexact whole" #t (integer? 2.0))
(assert "integer? non-integer" #f (integer? 2.5))
(assert "number? bignum" #t (number? (expt 2 100)))

(assert "sqrt perfect square -> exact" 3 (sqrt 9))
(assert "sqrt perfect square exactness" #t (eqv? (sqrt 16) 4))
(assert "sqrt big perfect square" "1000000000000" (number->string (sqrt (expt 10 24))))
(assert "sqrt non-square -> inexact" #t (< 1.414 (sqrt 2) 1.415))
(assert "sqrt exact non-square -> inexact" #t (< 3.162 (sqrt 10) 3.163))
(assert "sqrt inexact input -> inexact" #t (< 2.999 (sqrt 9.0) 3.001))

(assert "exact? exact" #t (exact? 5))
(assert "exact? inexact" #f (exact? 5.0))
(assert "exact? bignum" #t (exact? (expt 2 100)))
(assert "inexact? inexact" #t (inexact? 5.0))
(assert "inexact" #t (eqv? (inexact 5) 5.0))
(assert "exact" #t (eqv? (exact 5.0) 5))
(assert "exact of bignum unchanged" #t (eqv? (exact (expt 2 64)) (expt 2 64)))
(assert "exact->inexact alias" #t (eqv? (exact->inexact 5) 5.0))
(assert "inexact->exact alias" #t (eqv? (inexact->exact 5.0) 5))

(assert "zero?" #t (zero? 0))
(assert "zero? nonzero" #f (zero? 1))
(assert "zero? bignum" #f (zero? (expt 2 64)))
(assert "positive?" #t (positive? 3))
(assert "positive? neg" #f (positive? -3))
(assert "positive? bignum" #t (positive? (expt 2 64)))
(assert "negative? bignum" #t (negative? (- (expt 2 64))))
(assert "negative? zero" #f (negative? 0))

(summary)
