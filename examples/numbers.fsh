let printf = fn cstr ... i32 => extern

let main = fn i32 =>
    printf "%d\n" 34 + 35;
    0
