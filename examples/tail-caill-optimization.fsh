let printf = fn cstr ... i32 => extern

let count = fn n: i32 i32 =>
    printf "%d\n" n;
    if n > 0
        then count (n-1)
        else 0

let main = fn i32 =>
    count 10; 0
