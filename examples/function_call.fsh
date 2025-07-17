(extern puts)
(extern printf)

(defun twice [f x] (f (f x)))

(defun inc [n] (+ n 1))

(defun foo [] (puts "foo") 0)
(defun bar [x y] (+ x y))
(defun main []
  (foo)
  (printf "%d\n" (+ (bar 1 2) (bar 3 4)))
  (printf "%d\n" (twice inc 5)) 0)
