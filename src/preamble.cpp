#include "preamble.hpp"

const std::string_view preamble = R"(

(begin
  ;; --- ca/dr combos ---

  (define (caar x) (car (car x)))
  (define (cadr x) (car (cdr x)))
  (define (cdar x) (cdr (car x)))
  (define (cddr x) (cdr (cdr x)))

  (define (caaar x) (car (car (car x))))
  (define (caadr x) (car (car (cdr x))))
  (define (cadar x) (car (cdr (car x))))
  (define (caddr x) (car (cdr (cdr x))))
  (define (cdaar x) (cdr (car (car x))))
  (define (cdadr x) (cdr (car (cdr x))))
  (define (cddar x) (cdr (cdr (car x))))
  (define (cdddr x) (cdr (cdr (cdr x))))

  (define (caaaar x) (car (car (car (car x)))))
  (define (caaadr x) (car (car (car (cdr x)))))
  (define (caadar x) (car (car (cdr (car x)))))
  (define (caaddr x) (car (car (cdr (cdr x)))))
  (define (cadaar x) (car (cdr (car (car x)))))
  (define (cadadr x) (car (cdr (car (cdr x)))))
  (define (caddar x) (car (cdr (cdr (car x)))))
  (define (cadddr x) (car (cdr (cdr (cdr x)))))
  (define (cdaaar x) (cdr (car (car (car x)))))
  (define (cdaadr x) (cdr (car (car (cdr x)))))
  (define (cdadar x) (cdr (car (cdr (car x)))))
  (define (cdaddr x) (cdr (car (cdr (cdr x)))))
  (define (cddaar x) (cdr (cdr (car (car x)))))
  (define (cddadr x) (cdr (cdr (car (cdr x)))))
  (define (cdddar x) (cdr (cdr (cdr (car x)))))
  (define (cddddr x) (cdr (cdr (cdr (cdr x)))))

  ;; --- higher-order ---

  (define (map f . lists)
    (define (single lst acc)
      (if (null? lst)
          (reverse acc)
          (single (cdr lst) (cons (f (car lst)) acc))))
    (define (multi lists acc)
      (if (any null? lists)
          (reverse acc)
          (multi (map cdr lists)
                 (cons (apply f (map car lists)) acc))))
    (if (null? (cdr lists))
        (single (car lists) '())
        (multi lists '())))

  (define (for-each f . lists)
    (if (null? (cdr lists))
        (let ((lst (car lists)))
          (if (not (null? lst))
              (begin (f (car lst))
                     (for-each f (cdr lst)))))
        (if (not (any null? lists))
            (begin (apply f (map car lists))
                   (apply for-each f (map cdr lists))))))

  (define (filter pred lst)
    (define (iter lst acc)
      (if (null? lst)
          (reverse acc)
          (iter (cdr lst)
                (if (pred (car lst))
                    (cons (car lst) acc)
                    acc))))
    (iter lst '()))

  (define (reduce f init lst)
    (if (null? lst)
        init
        (reduce f (f init (car lst)) (cdr lst))))

  (define (fold-right f init lst)
    (define (iter lst acc)
      (if (null? lst)
          acc
          (iter (cdr lst) (f (car lst) acc))))
    (iter (reverse lst) init))

  ;; --- list operations ---

  (define (last-pair lst)
    (if (null? (cdr lst))
        lst
        (last-pair (cdr lst))))

  (define (list-tail lst k)
    (if (= k 0)
        lst
        (list-tail (cdr lst) (- k 1))))

  (define (append . lists)
    (define (prepend lst acc)
      (if (null? lst)
          acc
          (prepend (cdr lst) (cons (car lst) acc))))
    (define (iter lists acc)
      (if (null? (cdr lists))
          (prepend acc (car lists))
          (iter (cdr lists) (prepend (car lists) acc))))
    (if (null? lists)
        '()
        (iter lists '())))

  (define (append! . lists)
    (define (find-last-pair lst)
      (if (null? (cdr lst))
          lst
          (find-last-pair (cdr lst))))
    (cond ((null? lists) '())
          ((null? (cdr lists)) (car lists))
          (else
           (let ((first-list (car lists)))
             (if (null? first-list)
                 (apply append! (cdr lists))
                 (begin
                   (set-cdr! (find-last-pair first-list)
                             (apply append! (cdr lists)))
                   first-list))))))

  (define (reverse lst)
    (define (iter l acc)
      (if (null? l)
          acc
          (iter (cdr l) (cons (car l) acc))))
    (iter lst '()))

  (define (zip a b)
    (define (iter a b acc)
      (if (or (null? a) (null? b))
          (reverse acc)
          (iter (cdr a) (cdr b)
                (cons (list (car a) (car b)) acc))))
    (iter a b '()))

  (define (iota count . rest)
    (let ((start (if (null? rest) 0 (car rest)))
          (step (if (or (null? rest) (null? (cdr rest))) 1 (cadr rest))))
      (define (iter i acc)
        (if (= i 0)
            acc
            (iter (- i 1) (cons (+ start (* (- i 1) step)) acc))))
      (iter count '())))

  ;; --- search ---

  (define (memq obj lst)
    (if (null? lst)
        #f
        (if (eq? obj (car lst))
            lst
            (memq obj (cdr lst)))))

  (define (member obj lst)
    (if (null? lst)
        #f
        (if (equal? obj (car lst))
            lst
            (member obj (cdr lst)))))

  (define (assoc key alist)
    (if (null? alist)
        #f
        (if (equal? key (caar alist))
            (car alist)
            (assoc key (cdr alist)))))

  (define (assv key alist)
    (if (null? alist)
        #f
        (if (eq? key (caar alist))
            (car alist)
            (assv key (cdr alist)))))

  ;; --- predicates ---

  (define (any pred lst)
    (if (null? lst)
        #f
        (if (pred (car lst))
            #t
            (any pred (cdr lst)))))

  (define (every pred lst)
    (if (null? lst)
        #t
        (if (pred (car lst))
            (every pred (cdr lst))
            #f)))

  ;; --- streams ---

  (define the-empty-stream '())

  (define (stream-null? s) (null? s))

  (define (stream-car s) (car s))

  (define (stream-cdr s) (force (cdr s)))

  (define (stream-ref s n)
    (if (= n 0)
        (stream-car s)
        (stream-ref (stream-cdr s) (- n 1))))

  (define (stream-map f . streams)
    (if (any stream-null? streams)
        the-empty-stream
        (cons-stream (apply f (map stream-car streams))
                     (apply stream-map f (map stream-cdr streams)))))

  (define (stream-filter pred s)
    (cond ((stream-null? s) the-empty-stream)
          ((pred (stream-car s))
           (cons-stream (stream-car s)
                        (stream-filter pred (stream-cdr s))))
          (else (stream-filter pred (stream-cdr s)))))

  (define (stream-for-each f s)
    (if (not (stream-null? s))
        (begin (f (stream-car s))
               (stream-for-each f (stream-cdr s)))))

  (define (stream-take s n)
    (if (or (= n 0) (stream-null? s))
        the-empty-stream
        (cons-stream (stream-car s)
                     (stream-take (stream-cdr s) (- n 1)))))

  (define (stream->list s)
    (if (stream-null? s)
        '()
        (cons (stream-car s) (stream->list (stream-cdr s)))))

  (define (stream-enumerate-interval low high)
    (if (> low high)
        the-empty-stream
        (cons-stream low (stream-enumerate-interval (+ low 1) high))))

  ;; --- function combinators ---

  (define (compose f g)
    (lambda args (f (apply g args))))

  (define (partial f . args)
    (lambda rest (apply f (append args rest))))

  (define (curry f n)
    (define (helper args)
      (if (>= (length args) n)
          (apply f args)
          (lambda rest
            (helper (append args rest)))))
    (helper '()))
)

)";
