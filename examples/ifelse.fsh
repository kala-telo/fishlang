(extern puts   (fn [cstr]     i32))
(extern printf (fn [cstr ...] i32))

(def main (fn [] i32
  (if (< 2 1)
    (puts "2 < 1 is true")
    (puts "2 < 1 is false"))
  (if (< 1 2)
    (puts "1 < 2 is true")
    (puts "1 < 2 is false"))
  (if true
    (puts "true is true")
    (puts "true is false"))
  (if false
    (puts "false is true")
    (puts "false is false"))
  (printf "0 < 1 = %d\n" (< 0 1))
  (printf "1 < 0 = %d\n" (< 1 0))
  (printf "true = %d\n" true)
  (printf "false = %d\n" false)
  (printf "(it should output 2): %d\n" (if false 1 2))
  (printf "(it should output 1): %d\n" (if true 1 2)) 0))

