(assert "char literal" #\a #\a)
(assert "char space" #\space #\space)
(assert "char newline" #\newline #\newline)

(assert "char?" #t (char? #\a))
(assert "char? false" #f (char? "a"))

(assert "char=?" #t (char=? #\a #\a))
(assert "char=? false" #f (char=? #\a #\b))

(assert "char->integer" 97 (char->integer #\a))
(assert "integer->char" #\a (integer->char 97))
(assert "integer->char range" #t
        (guard (e ((error-object? e) #t))
          (integer->char (expt 2 100))
          #f))

(assert "string->list" (list #\h #\i) (string->list "hi"))
(assert "list->string" "hi" (list->string (list #\h #\i)))
(assert "string->list empty" '() (string->list ""))
(assert "list->string empty" "" (list->string '()))

(assert "string-ref char" #\e (string-ref "hello" 1))

(assert "equal? chars" #t (equal? #\x #\x))
(assert "equal? chars false" #f (equal? #\x #\y))

(assert "roundtrip" "hello" (list->string (string->list "hello")))

(summary)
