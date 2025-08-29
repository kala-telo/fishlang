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

#ifndef DEBUG
#define DEBUG 1
#endif

#if DEBUG
#define CC "clang "
#define CFLAGS "-pedantic -std=c99 -fsanitize=address -O2 -D_FORTIFY_SOURCE=3 -g -Wall -Wextra "
#else
#define CC "cc "
#define CFLAGS "-static -pedantic -std=c99 -O3 -Wall -Wextra "
#endif

#define LD CC
#define TARGET "fishc"

#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"

#define ARRLEN(xs) (sizeof(xs) / sizeof(*(xs)))

#define da_append_str(arena, xs, x)                                            \
    for (int da_append_str_i = 0; da_append_str_i < strlen((x));               \
         da_append_str_i++) {                                                  \
        da_append((arena), (xs), (x)[da_append_str_i]);                        \
    }

typedef enum {
    STATUS_OK,
    STATUS_BUILD_FAIL,
    STATUS_WRONG_OUTPUT
} Status;
typedef enum {
    TARGET_PPC,
    TARGET_X86_32,
} Target;

static const char *const targets[] = {
    [TARGET_PPC] = "ppc", [TARGET_X86_32] = "x86_32"};

static const char *const target_compiler[] = {
    [TARGET_PPC] = "powerpc-unknown-linux-musl-gcc",
    [TARGET_X86_32] = "i686-pc-linux-musl-gcc",
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

bool rebuild_myself(void) {
    static char buffer[1000];
    snprintf(buffer, sizeof(buffer), CC "%s -o ./build", __FILE__);
    printf("$ %s\n", buffer);
    return system(buffer) != 0;
}

bool link(String *files, size_t files_count) {
    struct {
        char *data;
        size_t len, capacity;
    } link_command = {0};
    Arena scratch = {0};
    da_append_str(&scratch, link_command, LD);
    da_append_str(&scratch, link_command, CFLAGS);
    for (size_t i = 0; i < files_count; i++) {
        char *o_file = c2o(files[i]);
        da_append_str(&scratch, link_command, o_file);
        da_append(&scratch, link_command, ' ');
    }
    da_append_str(&scratch, link_command, "-o " TARGET);
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

Status run_file(Target target, String file) {
    char asm_path[1000], buffer[1000];
    snprintf(asm_path, sizeof(asm_path), ".build/examples/%.*s.S",
             file.length - 4, file.string);
    snprintf(buffer, sizeof(buffer), "./" TARGET " -t %s examples/%.*s -o %s",
             targets[target], PS(file), asm_path);
    if (system(buffer) != 0) {
        return STATUS_BUILD_FAIL;
    }
    snprintf(buffer, sizeof(buffer), "%s -static %s -o .build/examples/%.*s-%s", target_compiler[target],
             asm_path, file.length - 4, file.string, targets[target]);
    if (system(buffer) != 0) {
        return STATUS_BUILD_FAIL;
    }
    return STATUS_OK;
}

bool build_examples() {
    DIR *examples_dir = opendir("examples");
    if (examples_dir == NULL)
        return true;
    struct dirent *de;
    int the_longest_name = 0;
    while ((de = readdir(examples_dir)) != NULL) {
        int name_len = strlen(de->d_name);
        if (name_len > the_longest_name) {
            the_longest_name = name_len;
        }
    }
    rewinddir(examples_dir);
    while ((de = readdir(examples_dir)) != NULL) {
        int len = strlen(de->d_name);
        if (!endswith((String){de->d_name, len}, S(".fsh"))) {
            continue;
        }
        printf("%*s: ", the_longest_name, de->d_name);
        for (size_t i = ARRLEN(targets); i-- > 0;) {
            Status result = run_file(i, (String){de->d_name, len});
            switch (result) {
            case STATUS_BUILD_FAIL:
                printf(RED "x " RESET);
                break;
            case STATUS_OK:
                printf(GREEN "✓ " RESET);
                break;
            case STATUS_WRONG_OUTPUT:
                printf(YELLOW "? " RESET);
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
    closedir(examples_dir);
    return false;
}

void usage(char *program) {
    printf("%s [-h] [run]\n", program);
}

int main(int argc, char *argv[]) {
    if (rebuild_myself())
        return -1;
    bool run = false;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "run") == 0) {
            run = true;
        }
    }

    if (create_dir(".build"))
        return -1;
    if (create_dir(".build/targets"))
        return -1;
    if (create_gitignore(".build/.gitignore"))
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
        S("src/targets/debug.c"),
    };

    for (size_t i = 0; i < ARRLEN(files); i++) {
        if (build_c(files[i]))
            return -1;
    }
    link(files, ARRLEN(files));
    if (!run) return 0;

    if (create_dir(".build/examples")) return -1;
    printf("\n\n");
    if (build_examples()) return -1;
}
