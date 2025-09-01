# The unnamed programming language (working name fishlang)
As you could've guessed, considering the language is not even named yet, it's
still WIP. You can see examples in `examples/` directory, though at the current
moment they are closer to just tests, the only "real" example being recursive fibonacci
program in `examples/recursive_fib.fsh`. The language goal is to provide playground in
compiler development to me. <br>
## That being said, here is a vague roadmap:
- [x] Tests
- [ ] Enough features to write complex programs
- [ ] More optimizations
- [ ] Standard library to not dependent on libc
- [ ] FP features
- [ ] Replace GAS with my own assembler
- [ ] Make my own linker
## Backends
- [x] PowerPC
- [x] MIPS
- [x] x86_32
- [ ] ARM
- [ ] RISC-V
- ..?
# Getting started
```shell
$ cc build.c -o build && ./build run
```
You'll get the examples built in `.build/examples/`. It will try to build and test them for all platforms. You'll need compilers and VMs for that.

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
- https://devblogs.microsoft.com/oldnewthing/20180810-00/?p=99465
- https://devblogs.microsoft.com/oldnewthing/20180809-00/?p=99455
- https://math-atlas.sourceforge.net/devel/assembly/elfspec_ppc.pdf
## X86_32
- https://wiki.osdev.org/Calling_Conventions
## MIPS
- https://en.wikibooks.org/wiki/MIPS_Assembly/Register_File
