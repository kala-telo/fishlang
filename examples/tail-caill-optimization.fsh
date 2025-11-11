(extern printf (fn [(fmt cstr) ...] i32))

(def count (fn [(n i32)] i32
	(printf "%d\n" n)
	(if (> n 0) (count (- n 1)) 0)))

(def main (fn [] i32
	(count 10) 0))
