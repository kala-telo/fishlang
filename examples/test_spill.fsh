let printf = fn cstr i32 => i32

let foo = fn a: i32 b: i32 c: i32 d: i32 e: i32 f: i32 g: i32 i32 =>
    let [v0  =   a + 1
         v1  =   b + 2
         v2  =   c + 3
         v3  =   d + 4
         v4  =   e + 5
         v5  =   f + 6
         v6  =   g + 7
         v7  =  v0 + v1
         v8  =  v2 + v3
         v9  =  v4 + v5
         v10 =  v6 + v7
         v11 =  v8 + v9
         v12 = v10 + v11
         v13 = v12 + a
         v14 = v13 + b
         v15 = v14 + c
         v16 = v15 + d
         v17 = v16 + e
         v18 = v17 + f
         v19 = v18 + g
         v20 = v19 + v0
         v21 = v20 + v1
         v22 = v21 + v2
         v23 = v22 + v3
         v24 = v23 + v4
         v25 = v24 + v5
         v26 = v25 + v6
         v27 = v26 + v7
         v28 = v27 + v8
         v29 = v28 + v9
         v30 = v29 + v10
         v31 = v30 + v11
         v32 = v31 + v12
         v33 = v32 + v13
         v34 = v33 + v14
         v35 = v34 + v15
         v36 = v35 + v16
         v37 = v36 + v17
         v38 = v37 + v18
         v39 = v38 + v19]
     v0 +  v1 +  v2 +  v3 +  v4 +  v5 +  v6 +  v7 +  v8 +  v9 +
    v10 + v11 + v12 + v13 + v14 + v15 + v16 + v17 + v18 + v19 +
    v20 + v21 + v22 + v23 + v24 + v25 + v26 + v27 + v28 + v29 +
    v30 + v31 + v32 + v33 + v34 + v35 + v36 + v37 + v38 + v39 +
    a b c d e f + g

let main = fn i32 =>
    printf "%d\n" (foo 1 2 3 4 5 6 7);
    0
