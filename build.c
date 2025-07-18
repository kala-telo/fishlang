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

#define CC "clang "
#define CFLAGS "-fsanitize=address -O2 -D_FORTIFY_SOURCE=3 -g -Wall -Wextra "
#define LD CC
#define TARGET "fishc"
#define AS "powerpc-unknown-linux-musl-as "
#define PPC_LD "powerpc-unknown-linux-musl-gcc -static "

#define RED "\033[31m"
#define RESET "\033[0m"

#define ARRLEN(xs) (sizeof(xs) / sizeof(*(xs)))

#define da_append_str(xs, x)                                                   \
    for (int da_append_str_i = 0; da_append_str_i < strlen(x);                 \
         da_append_str_i++) {                                                  \
        da_append(xs, x[da_append_str_i]);                                     \
    }

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

bool link(String *files, size_t files_count) {
    struct {
        char *data;
        size_t len, capacity;
    } link_command = {0};
    da_append_str(link_command, LD);
    da_append_str(link_command, CFLAGS);
    for (size_t i = 0; i < files_count; i++) {
        char *o_file = c2o(files[i]);
        da_append_str(link_command, o_file);
        da_append(link_command, ' ');
    }
    da_append_str(link_command, "-o " TARGET);
    da_append(link_command, '\0');
    printf("$ %s\n", link_command.data);
    bool result = system(link_command.data) != 0;
    free(link_command.data);
    return result;
}

bool build_c(String path) {
    char buffer[1000];
    snprintf(buffer, sizeof(buffer), CC CFLAGS " -c %.*s -o %s", PS(path),
             c2o(path));

    printf("$ %s\n", buffer);
    return system(buffer) != 0;
}

bool build_examples() {
    DIR *examples_dir = opendir("examples");
    if (examples_dir == NULL)
        return true;
    struct dirent *de;
    char buffer[1000];
    while ((de = readdir(examples_dir)) != NULL) {
        int len = strlen(de->d_name);
        if (!endswith((String){de->d_name, len}, S(".fsh"))) {
            continue;
        }
        char asm_path[1000];
        snprintf(asm_path, sizeof(asm_path), ".build/examples/%.*s.S", len - 4,
                 de->d_name);
        char obj_path[1000];
        snprintf(obj_path, sizeof(obj_path), ".build/examples/%.*s.o", len - 4,
                 de->d_name);
        snprintf(buffer, sizeof(buffer), "./" TARGET " examples/%s > %s",
                 de->d_name, asm_path);
        printf("$ %s\n", buffer);
        if (system(buffer) != 0) {
            printf(RED"Example `%s` failed to compile! "RESET"\n", de->d_name);
            continue;
        }
        snprintf(buffer, sizeof(buffer), AS " %s -o %s", asm_path, obj_path);
        printf("$ %s\n", buffer);
        if (system(buffer) != 0) {
            printf(RED"Example `%s` failed to assemble!\n"RESET, de->d_name);
            continue;
        }
        snprintf(buffer, sizeof(buffer), PPC_LD " %s -o .build/examples/%.*s", obj_path, len - 4,
                 de->d_name);
        printf("$ %s\n", buffer);
        system(buffer);
    }
    closedir(examples_dir);
    return false;
}

void usage(char *program) {
    printf("%s [-h] [run]\n", program);
}

int main(int argc, char *argv[]) {
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
    if (create_gitignore(".build/.gitignore"))
        return -1;

    String files[] = {
        S("src/codegen.c"),
        S("src/parser.c"),
        S("src/lexer.c"),
        S("src/main.c"),
        S("src/tac.c"),
    };

    for (size_t i = 0; i < ARRLEN(files); i++) {
        if (build_c(files[i]))
            return -1;
    }
    link(files, ARRLEN(files));
    if (!run) return 0;

    if (create_dir(".build/examples")) return -1;
    if (build_examples()) return -1;
}
