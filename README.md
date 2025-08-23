# The unnamed programming language (working name fishlang)
As you could've guessed, considering the language is not even named yet, it's
still WIP. You can see examples in `examples/` directory, though at the current
moment they are closer to just tests, the only "real" example being recursive fibonacci
program in `examples/recursive_fib.fsh`. The language goal is to provide playground in
compiler development to me. <br>
That being said, here is a vague roadmap:
- [ ] Tests
- [ ] Enough features to write complex programs
- [ ] More optimizations
- [ ] MIPS, ARM and other backends
- [ ] Standard library to not dependent on libc
- [ ] FP features
- [ ] Replace GAS with my own assembler
- [ ] Make my own linker
# Getting started
```shell
$ cc build.c -o build && ./build run
```
You'll get the examples built in `.build/examples/`. You can run them as usual binaries
if you are on PowerPC, otherwise you can use `qemu-ppc`.

# Notes
## Typechecking
It was quite hard, you can even see gap on my github activity when I was doing it
(August of 2025, not to say it was the only reason why I didn't made any commits, but definetly one of).
It took me about 3 attemts to get it to point I'm happy with.

# References
## On compiler dev in general
- A New C Compiler by Ken Thompson [link1](https://c9x.me/compile/bib/new-c.pdf) [link2](https://doc.cat-v.org/bell_labs/new_c_compilers/new_c_compiler.pdf)
## PowerPC
- https://jimkatz.github.io/powerpc_for_dummies
- https://www.nxp.com/docs/en/user-guide/MPCFPE_AD_R1.pdf
- https://www.nxp.com/docs/en/user-guide/MPCFPE_AD_R1.pdf
- https://devblogs.microsoft.com/oldnewthing/20180810-00/?p=99465
- https://devblogs.microsoft.com/oldnewthing/20180809-00/?p=99455
- https://math-atlas.sourceforge.net/devel/assembly/elfspec_ppc.pdf
