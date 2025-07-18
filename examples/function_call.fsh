(extern (defun puts   [(str :cstr)] :int))
(extern (defun printf [(fmt :cstr) ...] :int))

(defun twice [(f :todo) (x :int)] :int
  (f (f x)))

(defun inc [(n :int)] :int
  (+ n 1))

(defun foo [] :void
  (puts "foo") 0)

(defun bar [(x :int) (y :int)] :int
  (+ x y))

(defun main [] :int
  (foo)
  (printf "%d\n" (+ (bar 1 2) (bar 3 4)))
  (printf "%d\n" (twice inc 5)) 0)
