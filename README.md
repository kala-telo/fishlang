# The unnamed programming language (working name fishlang)
Getting started:
```shell
$ sh build.sh
$ ./fishc ./test.fsh | tee test.S
$ powerpc-unknown-linux-musl-as test.S -o test.o # or your assembler & linker
$ powerpc-unknown-linux-musl-gcc test.o -o test
$ qemu-ppc ./test # or just `./test`, if you are on powerpc
```
