(extern printf (fn [(fmt cstr) ...] i32))

(def fib (fn [(n i32)] i32
  (if (< n 2)
    n
    (+ (fib (- n 1))
       (fib (- n 2))))))

(def main (fn [] i32
  (printf "%d\n" (fib 10)) 0))

