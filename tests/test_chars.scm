;; literals
(assert "char literal" #\a #\a)
(assert "char space" #\space #\space)
(assert "char newline" #\newline #\newline)

;; type predicate
(assert "char?" #t (char? #\a))
(assert "char? false" #f (char? "a"))

;; equality
(assert "char=?" #t (char=? #\a #\a))
(assert "char=? false" #f (char=? #\a #\b))

;; conversion
(assert "char->integer" 97 (char->integer #\a))
(assert "integer->char" #\a (integer->char 97))

;; string bridge
(assert "string->list" (list #\h #\i) (string->list "hi"))
(assert "list->string" "hi" (list->string (list #\h #\i)))
(assert "string->list empty" '() (string->list ""))
(assert "list->string empty" "" (list->string '()))

;; string-ref returns char
(assert "string-ref char" #\e (string-ref "hello" 1))

;; equal? on chars
(assert "equal? chars" #t (equal? #\x #\x))
(assert "equal? chars false" #f (equal? #\x #\y))

;; roundtrip
(assert "roundtrip" "hello" (list->string (string->list "hello")))

(summary)
