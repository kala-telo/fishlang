(extern (defun printf [(fmt :cstr) ...] :int))

(defun fib [(n :int)] :int
  (if (< n 2)
    n
    (+ (fib (- n 1))
       (fib (- n 2)))))

(defun main [] :int
  (printf "%d\n" (fib 10)) 0)

