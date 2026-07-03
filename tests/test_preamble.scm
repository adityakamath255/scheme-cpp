(assert "map" '(1 4 9) (map (lambda (x) (* x x)) '(1 2 3)))
(assert "map multi" '(5 7 9) (map + '(1 2 3) '(4 5 6)))

(define acc '())
(for-each (lambda (x) (set! acc (cons x acc))) '(1 2 3))
(assert "for-each" '(3 2 1) acc)

(assert "filter" '(2 4) (filter even? '(1 2 3 4 5)))
(assert "reduce" 15 (reduce + 0 '(1 2 3 4 5)))
(assert "fold-right" '(1 2 3) (fold-right cons '() '(1 2 3)))

(assert "append" '(1 2 3 4) (append '(1 2) '(3 4)))
(assert "reverse" '(3 2 1) (reverse '(1 2 3)))
(assert "zip" '((1 a) (2 b)) (zip '(1 2) '(a b)))
(assert "iota" '(0 1 2 3 4) (iota 5))
(assert "iota start" '(10 11 12) (iota 3 10))
(assert "iota step" '(0 2 4) (iota 3 0 2))
(assert "list-tail" '(c d) (list-tail '(a b c d) 2))

(assert "memq" '(c d) (memq 'c '(a b c d)))
(assert "memq miss" #f (memq 'z '(a b c)))
(assert "member" '((2) (3)) (member '(2) '((1) (2) (3))))
(assert "assoc" '(b 2) (assoc 'b '((a 1) (b 2) (c 3))))
(assert "assv" '(b 2) (assv 'b '((a 1) (b 2) (c 3))))

(assert "any" #t (any even? '(1 3 4)))
(assert "any false" #f (any even? '(1 3 5)))
(assert "every" #t (every number? '(1 2 3)))
(assert "every false" #f (every even? '(1 2 3)))

(assert "partial" 6 ((partial + 1) 5))
(assert "compose" 10 ((compose (partial * 2) (partial + 1)) 4))
(assert "curry" 6 (((curry + 3) 1) 2 3))

(summary)
