# The unnamed programming language (working name fishlang)
Getting started:
```shell
$ sh build.sh
$ ./fishc ./examples/hello_world.fsh | tee hello_world.S
$ powerpc-unknown-linux-musl-as hello_world.S -o hello_world.o # or your assembler & linker
$ powerpc-unknown-linux-musl-gcc hello_world.o -o hello_world
$ qemu-ppc ./hello_world # or just `./hello_world`, if you are on powerpc
```
