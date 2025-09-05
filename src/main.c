#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include "tac.h"
#include "typing.h"

typedef enum {
    TARGET_PPC,
    TARGET_X86_32,
    TARGET_PDP8,
    TARGET_DEBUG,
} Target;
const char *const target_names[] = {
    [TARGET_PPC] = "ppc",
    [TARGET_X86_32] = "x86_32",
    [TARGET_PDP8] = "pdp8",
    [TARGET_DEBUG] = "debug",
};
#define ARRLEN(xs) (sizeof(xs)/sizeof(*(xs)))

char *next_arg(int* argc, char ***argv, char* error) {
    if (*argc == 0) {
        if (error != NULL) {
            fprintf(stderr, "%s\n", error);
        }
        exit(1);
    }
    char *result = **argv;
    (*argc)--;
    (*argv)++;
    return result;
}

void compile(Target target, const char *const file_name, FILE *input,
             FILE *out) {
    fseek(input, 0, SEEK_END);
    int size = ftell(input);
    fseek(input, 0, SEEK_SET);

    String str = {0};
    str.string = malloc(size * sizeof(char));
    str.length = size;

    size = fread(str.string, 1, size, input);
    fclose(input);

    Lexer lex = {
        // -1 for \0, though it still must be in memory for
        // lexer to not read into unallocated memory
        .length = size - 1,
        .position = str.string,
        .loc = {0, 0, file_name},
    };

    ASTArr body = {0};
    Arena arena = {0};
    {
        size_t node_id = 1;
        while (peek_token(&lex).kind != LEX_END)
            parse(&arena, &lex, &body, NULL, &node_id);
    }

    TypeTable tt = {0};
    extract_types(&arena, body, &tt);

    typecheck(body, tt);

    CodeGenCTX cg_ctx = { 0 };
    IR ir = codegen(&arena, body, &cg_ctx);
    for (size_t i = 0; i < ir.functions.len; i++) {
        bool repeat = true;
        while (repeat) {
            repeat = peephole_optimization(&ir.functions.data[i].code);
            ir.functions.data[i].temps_count =
                fold_temporaries(ir.functions.data[i].code);
        }
        ir.functions.data[i].temps_count =
            fold_temporaries(ir.functions.data[i].code);
    }
    switch (target) {
    case TARGET_DEBUG:
        codegen_debug(ir, out);
        break;
    case TARGET_PPC:
        codegen_powerpc(ir, out);
        break;
    case TARGET_X86_32:
        codegen_x86_32(ir, out);
        break;
    case TARGET_PDP8:
        codegen_pdp8(ir, out);
        break;
    }

    arena_destroy(&arena);
    free(str.string);
}

void usage(FILE *out, const char *const program) {
    fprintf(out, "Usage: %s [-h] [-t <target>] [-o <output file>] <input file>\n\n", program);
    fprintf(out, "\t-t\tcompilation target\n");
    fprintf(out, "\t\t\tppc\t32 bit powerpc GAS\n");
    fprintf(out, "\t\t\tx86_32\t32 bit Intel x86 GAS\n");
    fprintf(out, "\t\t\tdebug\thuman-readable pseudocode\n");
    fprintf(out, "\t-h\tShows this help message\n");
    fprintf(out, "\t-o\tSpecifies the output file, the default one stdout\n");
}

int main(int argc, char *argv[]) {
    char *program = next_arg(&argc, &argv, NULL);
    Target target = TARGET_PPC;
    FILE* output = stdout;
    struct {
        char** data;
        size_t len, capacity;
    } inputs = {0};
    Arena arena = {0};
    while (argc) {
        char *arg = next_arg(&argc, &argv, NULL);
        if (strcmp(arg, "-t") == 0) {
            char *target_str = next_arg(&argc, &argv, "Argument `-t` expects target name next, see -h for list");
            for (size_t i = 0; i < ARRLEN(target_names); i++) {
                if (strcmp(target_str, target_names[i]) == 0)
                    target = i;
            }
        } else if (strcmp(arg, "-o") == 0) {
            if (output != stdout) {
                // TODO: rephrase this
                fprintf(stderr, "Attempting to set output the second time is weird\n");
                return 1;
            }
            char *filename = next_arg(&argc, &argv, "Argument `-o` expects filename next, see -h");
            output = fopen(filename, "w");
            if (!output) {
                fprintf(stderr, "Couldn't open file `%s`\n", filename);
                arena_destroy(&arena);
                return 1;
            }
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            usage(stdout, program);
            arena_destroy(&arena);
            return 0;
        } else {
            da_append(&arena, inputs, arg);
        }
    }
    if (inputs.len == 0) {
        fprintf(stderr, "No input files were provided.\n");
    }
    for (size_t i = 0; i < inputs.len; i++) {
        char *filename = inputs.data[i];
        FILE* input = fopen(filename, "r");
        if (input == NULL) {
            fprintf(stderr, "Couldn't open input file `%s`\n", filename);
            arena_destroy(&arena);
            return 1;
        }
        compile(target, filename, input, output);
    }
    arena_destroy(&arena);
    if (output != stdout) fclose(output);
}
