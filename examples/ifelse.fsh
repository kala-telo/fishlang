(extern (defun puts [(str :cstr)] :int))
(extern (defun printf [(fmt :cstr) ...] :int))

(defun main [] :int
  (if (< 2 1)
    (puts "true")
    (puts "false"))
  (if (< 1 2)
    (puts "true")
    (puts "false"))
  (printf "%d\n" (if 0 1 2))
  (printf "%d\n" (if 1 1 2)) 0)

