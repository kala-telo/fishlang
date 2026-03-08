let printf = fn cstr ... i32 => extern

let main = fn i32 =>
    let [x = 34 y = 35]
    printf "%d\n" (x + y);
    0
