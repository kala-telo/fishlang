#include "src/da.h"
#include "src/string.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>

#ifndef DEBUG
#define DEBUG 1
#endif

#define UNIT_BUILD

#define COMMON_CFLAGS "-pedantic -std=c99 -Wall -Wextra -Wno-missing-braces "
#if DEBUG
#   define CC "clang "
#   define SANITIZERS "-fsanitize=address,undefined,cfi -flto "
#   ifndef CFLAGS
#       define CFLAGS " -O2 -D_FORTIFY_SOURCE=3 -g -fstack-protector-strong -fstack-clash-protection -fvisibility=hidden -fno-common " SANITIZERS
#   endif
#else
#   define CC "cc "
#   ifndef CFLAGS
#       define CFLAGS "-static -O3 " COMMON_CFLAGS
#   endif
#endif

#define LD CC
#define TARGET "fishc"

#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define RESET "\033[0m"

#define ARRLEN(xs) (sizeof(xs) / sizeof(*(xs)))

#define da_append_cstr(arena, xs, x)                                            \
    for (int da_append_str_i = 0; da_append_str_i < strlen((x));               \
         da_append_str_i++) {                                                  \
        da_append((arena), (xs), (x)[da_append_str_i]);                        \
    }
#define da_append_string(arena, xs, x)                                         \
    for (int da_append_str_i = 0; da_append_str_i < (x).length;                \
         da_append_str_i++) {                                                  \
        da_append((arena), (xs), (x).string[da_append_str_i]);                 \
    }

typedef enum {
    STATUS_OK,
    STATUS_BUILD_FAIL,
    STATUS_WRONG_OUTPUT,
    STATUS_NO_TEST,
} Status;
typedef enum {
    TARGET_PPC,
    TARGET_X86_32,
    TARGET_MIPS,
    TARGET_PDP8
} Target;

static const char *const targets[] = {
    [TARGET_PPC] = "ppc",
    [TARGET_X86_32] = "x86_32",
    [TARGET_MIPS] = "mips",
    [TARGET_PDP8] = "pdp8",
};

static const char *const target_compiler[] = {
    [TARGET_PPC] = "powerpc-unknown-linux-musl-gcc",
    [TARGET_X86_32] = "i686-pc-linux-musl-gcc",
    [TARGET_MIPS] = "mips-unknown-linux-musl-gcc",
    [TARGET_PDP8] = ".build/gal",
};
static const char *const runners[] = {
    [TARGET_PPC] = "qemu-ppc",
    [TARGET_X86_32] = "qemu-i386",
    [TARGET_MIPS] = "qemu-mips",
    [TARGET_PDP8] = "pdp8", // simh version should be >= 4.0, otherwise there
                            // will be a segfault
};

static bool endswith(String str, String suf) {
    if (str.length < suf.length) return false;
    return string_eq((String){.string = &str.string[str.length - suf.length],
                              .length = suf.length},
                     suf);
}

bool create_dir(char *dir) {
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        return mkdir(dir, 0700) != 0;
    }
    return false;
}

bool create_gitignore(char *path) {
    FILE *f = fopen(path, "w");
    if (f == NULL)
        return true;
    if (fwrite("*", 1, 1, f) != 1)
        return -1;
    fclose(f);
    return false;
}

char *c2o(String cpath) {
    static char buffer[1000];

    const int src_len = sizeof("src/") - 1;
    const int c_len = sizeof(".c") - 1;
    assert(cpath.length > src_len + c_len);
    assert(string_eq((String){cpath.string, src_len}, S("src/")));
    assert(string_eq((String){cpath.string + cpath.length - c_len, c_len},
                     S(".c")));

    int filename_len = cpath.length - (src_len + c_len);
    char format[] = ".build/%.*s.o";
    size_t out_len =
        sizeof(".build/") - 1 + sizeof(".o") - 1 + filename_len + 1;
    assert(out_len < sizeof(buffer));
    buffer[out_len - 1] = '\0';
    snprintf(buffer, out_len, format,
             PS(((String){cpath.string + src_len, filename_len})));
    return buffer;
}

char *file2test(String file) {
    static char buffer[1000];
    snprintf(buffer, sizeof(buffer), "examples/.%.*s.test", PS(file));
    return buffer;
}

bool rebuild_myself(void) {
    static char buffer[1000];
    snprintf(buffer, sizeof(buffer), CC "%s -o ./build -DDEBUG=%d", __FILE__, DEBUG);
    printf("$ %s\n", buffer);
    return system(buffer) != 0;
}

bool link_(String *files, size_t files_count) {
    struct {
        char *data;
        size_t len, capacity;
    } link_command = {0};
    Arena scratch = {0};
    da_append_cstr(&scratch, link_command, LD);
    da_append_cstr(&scratch, link_command, CFLAGS);
    for (size_t i = 0; i < files_count; i++) {
        char *o_file = c2o(files[i]);
        da_append_cstr(&scratch, link_command, o_file);
        da_append(&scratch, link_command, ' ');
    }
    da_append_cstr(&scratch, link_command, "-o " TARGET);
    da_append(&scratch, link_command, '\0');
    printf("$ %s\n", link_command.data);
    bool result = system(link_command.data) != 0;
    arena_destroy(&scratch);
    return result;
}

bool build_c(String path) {
    char buffer[1000];
    snprintf(buffer, sizeof(buffer), CC CFLAGS " -c %.*s -o %s", PS(path),
             c2o(path));

    printf("$ %s\n", buffer);
    return system(buffer) != 0;
}

Status run_file(Target target, String file, bool gen_test) {
    char asm_path[1000], buffer[1000], output[1000];
    snprintf(asm_path, sizeof(asm_path), ".build/examples/%.*s.S",
             file.length - 4, file.string);
    snprintf(buffer, sizeof(buffer), "./" TARGET " -t %s examples/%.*s -o %s",
             targets[target], PS(file), asm_path);
    if (system(buffer) != 0) {
        return STATUS_BUILD_FAIL;
    }
    snprintf(output, sizeof(output), ".build/examples/%.*s-%s", file.length - 4,
             file.string, targets[target]);
    snprintf(buffer, sizeof(buffer), "%s -static %s -o %s",
             target_compiler[target], asm_path, output);
    
    if (system(buffer) != 0) {
        return STATUS_BUILD_FAIL;
    }
    if (gen_test) {
        FILE *reference = fopen(file2test(file), "w"),
             *file_stdout;
        assert(reference != NULL);
        if (target == TARGET_PDP8) {
            return STATUS_NO_TEST;
        } else {
            snprintf(buffer, sizeof(buffer), "%s %s", runners[target], output);
            file_stdout = popen(buffer, "r");
        }
        int c;
        while ((c = getc(file_stdout)) != EOF) {
            putc(c, reference);
        }
        fclose(reference);
        pclose(file_stdout);
    } else {
        FILE *reference = fopen(file2test(file), "r"),
             *file_stdout;
        if (reference == NULL)
            return STATUS_NO_TEST;
        if (target == TARGET_PDP8) {
            int stdin_pipe[2], stdout_pipe[2];
            if (pipe(stdin_pipe) || pipe(stdout_pipe)) {
                return STATUS_BUILD_FAIL;
            }

            pid_t pdp8_pid = fork();
            if (pdp8_pid < 0) {
                return STATUS_BUILD_FAIL;
            } else if (pdp8_pid == 0) {
                // Child process
                dup2(stdin_pipe[0], STDIN_FILENO);
                dup2(stdout_pipe[1], STDOUT_FILENO);
                dup2(stdout_pipe[1], STDERR_FILENO);

                close(stdin_pipe[1]);
                close(stdout_pipe[0]);
                
                execlp(runners[TARGET_PDP8], runners[TARGET_PDP8], NULL); // Run emulator
                exit(1);
            } else {
                // Parent process
                close(stdin_pipe[0]);
                close(stdout_pipe[1]);

                file_stdout = fdopen(stdout_pipe[0], "r");

                char buffer[1000];
                snprintf(buffer, sizeof(buffer), "load %s\nrun 200\nquit\n", output);
                write(stdin_pipe[1], buffer, strlen(buffer) + 1);

                short n = 0;
                while (n < 2) {
                    int c = getc(file_stdout);
                    if (c == '\n')
                        n++;
                    if (c == EOF)
                        break;
                }
                
            }
        } else {
            snprintf(buffer, sizeof(buffer), "%s %s", runners[target], output);
            file_stdout = popen(buffer, "r");
        }

        int c1;
        while ((c1 = getc(reference)) != EOF) {
            int c2 = getc(file_stdout);
            if (target == TARGET_PDP8) {
                while (c2 == '\r')
                    c2 = getc(file_stdout);
                c1 = toupper(c1);
            }
            if (c1 != c2) {
                fclose(reference);
                if (target == TARGET_PDP8)
                    fclose(file_stdout);
                else
                    pclose(file_stdout);
                return STATUS_WRONG_OUTPUT;
            }
        }
        fclose(reference);
        if (target == TARGET_PDP8)
            fclose(file_stdout);
        else
            pclose(file_stdout);
    }
    return STATUS_OK;
}

bool build_examples(bool gen_test) {
    DIR *examples_dir = opendir("examples");
    if (examples_dir == NULL)
        return true;
    Arena arena = {0};
    struct dirent *de;
    int the_longest_name = 0;
    int file_count = 0;
    while ((de = readdir(examples_dir)) != NULL) {
        if (de->d_name[0] == '.') continue;
        int name_len = strlen(de->d_name);
        if (name_len > the_longest_name) {
            the_longest_name = name_len;
        }
        file_count++;
    }
    Status(*results)[ARRLEN(targets)] =
        arena_alloc(&arena, sizeof(*results) * file_count);
    char (*names)[256] = arena_alloc(&arena, sizeof(*names) * file_count);
    rewinddir(examples_dir);
    int file_number = 0;
    while ((de = readdir(examples_dir)) != NULL) {
        if (*de->d_name == '.')
            continue;
        strncpy(names[file_number], de->d_name, 256);
        int len = strlen(de->d_name);
        if (!endswith((String){de->d_name, len}, S(".fsh"))) {
            continue;
        }
        for (int i = 0; i < ARRLEN(targets); i++) {
            results[file_number][i] = run_file(i, (String){de->d_name, len}, gen_test);
        }
        file_number++;
    }
    if (gen_test)
        goto exit;
    printf("\n\n");
    for (int i = 0; i < file_count; i++) {
        printf("%*s: ", the_longest_name, names[i]);
        for (size_t j = ARRLEN(targets); j-- > 0;) {
            switch (results[i][j]) {
            case STATUS_BUILD_FAIL:
                printf(RED "x " RESET);
                break;
            case STATUS_OK:
                printf(GREEN "✓ " RESET);
                break;
            case STATUS_WRONG_OUTPUT:
                printf(YELLOW "? " RESET);
                break;
            case STATUS_NO_TEST:
                printf(BLUE "! " RESET);
                break;
            }
        }
        printf("\n");
    }
    for (size_t i = 0; i < ARRLEN(targets); i++) {
        printf("%*s  ", the_longest_name, "");
        for (size_t j = ARRLEN(targets); j-- > 0;) {
            if (i == j) {
                printf("╰> ");
                printf("%s", targets[j]);
            } else if (j > i) {
                printf("│ ");
            }
        }
        printf("\n");
    }
exit:
    closedir(examples_dir);
    arena_destroy(&arena);
    return false;
}

bool build_gal(void) {
    char *command = "cc 2nd_party/gal.c -o .build/gal";
    printf("$ %s\n", command);
    return system(command) != 0;
}

void usage(char *program) {
    printf("%s [-h] [run] [gen-test]\n", program);
}

int main(int argc, char *argv[]) {
    if (rebuild_myself())
        return -1;
    bool run = false,
         gen_test = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "run") == 0) {
            run = true;
        } else if (strcmp(argv[i], "gen-test") == 0) {
            gen_test = true;
        } else {
            printf("Invalid option: `%s`\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (create_dir(".build"))
        return -1;
    if (create_dir(".build/targets"))
        return -1;
    if (create_gitignore(".build/.gitignore"))
        return -1;
    if (build_gal())
        return -1;

    String files[] = {
        S("src/codegen.c"),
        S("src/parser.c"),
        S("src/lexer.c"),
        S("src/main.c"),
        S("src/tac.c"),
        S("src/typing.c"),
        S("src/targets/x86.c"),
        S("src/targets/ppc.c"),
        S("src/targets/mips.c"),
        S("src/targets/debug.c"),
        S("src/targets/pdp8.c"),
    };

#ifdef UNIT_BUILD
    Arena arena = {0};
    struct {
        char *data;
        size_t len, capacity;
    } cmd = { 0 };
    da_append_cstr(&arena, cmd, CC CFLAGS);
    for (size_t i = 0; i < ARRLEN(files); i++) {
        da_append_cstr(&arena, cmd, " ");
        da_append_string(&arena, cmd, files[i]);
    }
    da_append_cstr(&arena, cmd, " -o "TARGET);
    da_append(&arena, cmd, 0);
    printf("$ %s\n", cmd.data);
    if (system(cmd.data) != 0)
        return -1;
    arena_destroy(&arena);
#else
    for (size_t i = 0; i < ARRLEN(files); i++) {
        if (build_c(files[i]))
            return -1;
    }
    link_(files, ARRLEN(files));
#endif
    if (run || gen_test) {
        if (create_dir(".build/examples")) return -1;
        printf("\n\n");
        if (build_examples(gen_test))
            return -1;
    }
}
