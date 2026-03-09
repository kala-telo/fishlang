let puts   = fn cstr     i32 => extern
let printf = fn cstr ... i32 => extern

let main = fn i32 =>
    if 2 < 1
        then puts "2 < 1 is true"
        else puts "2 < 1 is false";
    if 1 < 2
        then puts "1 < 2 is true"
        else puts "1 < 2 is false";
    if true
        then puts "true is true"
        else puts "true is false";
    if false
        then puts "false is true"
        else puts "false is false";
    printf "0 < 1 = %d\n" 0 < 1;
    printf "1 < 0 = %d\n" 1 < 0;
    printf "true = %d\n" true;
    printf "false = %d\n" false;
    printf "(it should output 2): %d\n" (if false then 1 else 2);
    printf "(it should output 1): %d\n" (if true  then 1 else 2);
    0

