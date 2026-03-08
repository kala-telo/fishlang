let printf = fn cstr ... i32 => extern

let count = fn n: i32 i32 =>
    printf "%d\n" n;
    if n > 0
        count (n-1)
        0

let main = fn i32 =>
    count 10; 0
