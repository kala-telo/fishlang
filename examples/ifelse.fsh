(extern puts   (fn [cstr]     i32))
(extern printf (fn [cstr ...] i32))

(def main (fn [] i32
  (if (< 2 1)
    (puts "true")
    (puts "false"))
  (if (< 1 2)
    (puts "true")
    (puts "false"))
  (printf "%d\n" (if false 1 2))
  (printf "%d\n" (if true 1 2)) 0))

