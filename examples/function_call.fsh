(defun foo [] (puts "foo") 0)
(defun bar [x y] (+ x y))
(defun main []
  (foo)
  (printf "%d\n" (+ (bar 1 2) (bar 3 4))) 0)
