let printf = fn cstr ... i32 => extern

let fib = fn n: i32 i32 =>
    if n < 2
        n
        (fib (n - 1)) + (fib (n - 2))

let main = fn i32 =>
    printf "%d\n" (fib 10);
    0

