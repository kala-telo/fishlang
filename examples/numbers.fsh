(extern (defun printf [(fmt :cstr) ...] :int))

(defun main [] :int (printf "%d\n" (+ 34 35)) 0)
