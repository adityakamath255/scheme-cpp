(define p (delay (+ 1 2)))
(assert "promise?" #t (promise? p))
(assert "promise? non-promise" #f (promise? 5))
(assert "promise? procedure" #f (promise? car))
(assert "force" 3 (force p))
(assert "force twice" 3 (force p))
(assert "force non-promise" 42 (force 42))
(assert "eq? same promise" #t (eq? p p))
(assert "eq? distinct promises" #f (eq? (delay 1) (delay 1)))

(define count 0)
(define q (delay (begin (set! count (+ count 1)) count)))
(assert "delay does not run" 0 count)
(assert "first force runs" 1 (force q))
(assert "second force cached" 1 (force q))
(assert "ran exactly once" 1 count)

(define lazy (cons-stream 1 (error "boom")))
(assert "cons-stream head strict" 1 (stream-car lazy))
(assert "cons-stream tail delayed" #t (promise? (cdr lazy)))

(define ones (cons-stream 1 ones))
(assert "self-referential stream" 1 (stream-ref ones 100))

(define (integers-from n) (cons-stream n (integers-from (+ n 1))))
(define integers (integers-from 1))

(assert "stream-car" 1 (stream-car integers))
(assert "stream-cdr" 2 (stream-car (stream-cdr integers)))
(assert "stream-ref" 5 (stream-ref integers 4))
(assert "stream-take" '(1 2 3 4 5) (stream->list (stream-take integers 5)))
(assert "stream-take past end" '(1 2)
        (stream->list (stream-take (stream-enumerate-interval 1 2) 5)))

(assert "stream-filter" '(2 4 6)
        (stream->list (stream-take (stream-filter even? integers) 3)))

(assert "stream-map" '(2 4 6)
        (stream->list (stream-take (stream-map (lambda (x) (* 2 x)) integers) 3)))
(assert "stream-map multi" '(2 4 6)
        (stream->list (stream-take (stream-map + integers integers) 3)))

(assert "enumerate-interval" '(1 2 3)
        (stream->list (stream-enumerate-interval 1 3)))
(assert "enumerate-interval empty" '()
        (stream->list (stream-enumerate-interval 3 1)))
(assert "stream-null?" #t (stream-null? the-empty-stream))

(define acc '())
(stream-for-each (lambda (x) (set! acc (cons x acc)))
                 (stream-take integers 3))
(assert "stream-for-each" '(3 2 1) acc)

(define taps 0)
(define tapped
  (stream-map (lambda (x) (set! taps (+ taps 1)) x) integers))
(stream-ref tapped 9)
(stream-ref tapped 9)
(assert "stream elements computed once" 10 taps)

(assert "deep stream-ref" 100000 (stream-ref (integers-from 1) 99999))

(summary)
