(extern (defun printf [(fmt :cstr) ...] :int))

(defun main [] :int
  (let [(x :int 34) (y :int 35)]
    (printf "%d\n" (+ x y))) 0)
