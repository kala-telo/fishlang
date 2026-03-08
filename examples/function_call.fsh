let puts   = fn str: cstr     i32 => extern
let printf = fn fmt: cstr ... i32 => extern

let twice = fn f: (fn i32 i32) x: i32 i32 =>
    f (f x)

let inc = fn n: i32 i32 =>
    n + 1

let foo = fn () =>
    puts "foo"

let bar = fn x: i32 y: i32 i32 =>
    x + y

let main = fn i32 =>
    foo;
    printf "%d\n" (bar 1 2) + (bar 3 4);
    printf "%d\n" (twice inc 5);
    0
