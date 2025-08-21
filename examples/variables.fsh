(extern printf (fn [cstr ...] i32))

(def main (fn [] i32
  (let [(x i32 34) (y i32 35)]
    (printf "%d\n" (+ x y))) 0))
