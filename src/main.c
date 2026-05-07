#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "lexer.h"
#include "parser.h"
#include "tac.h"
#include "todo.h"
#include "typing.h"
#include "da.h"
#include "todo.h"

typedef enum {
    TARGET_PPC,
    TARGET_X86_32,
    TARGET_PDP8,
    TARGET_DEBUG,
    TARGET_MIPS,
    TARGET_AST,
} Target;
const char *const target_names[] = {
    [TARGET_PPC] = "ppc",
    [TARGET_X86_32] = "x86_32",
    [TARGET_MIPS] = "mips",
    [TARGET_PDP8] = "pdp8",
    [TARGET_DEBUG] = "debug",
    [TARGET_AST] = "ast",
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

void dump_ast(ASTArr ast, FILE* out) {
    for (size_t i = 0; i < ast.len; i++) {
        AST node = ast.data[i];
        switch (node.kind) {
        case AST_UNIT:
            fprintf(out, "    %zu [label=\"uint\"];\n", node.id);
            break;
        case AST_BOOL:
            fprintf(out, "    %zu [label=\"bool %c\"];\n", node.id, node.as.boolean["ft"]);
            break;
        case AST_NAME:
            fprintf(out, "    %zu [label=\"name (%.*s)\"];\n", node.id, PS(node.as.name));
            break;
        case AST_NUMBER:
            fprintf(out, "    %zu [label=\"number (%"PRIu64")\"];\n", node.id, node.as.number);
            break;
        case AST_STRING:
            fprintf(out, "    %zu [label=\"string ('", node.id);
            for (int k = 0; k < node.as.string.length; k++) {
                putc(node.as.string.string[k], out);
                if (node.as.string.string[k] == '\\')
                    putc('\\', out);
            }
            fprintf(out,"')\"];\n");
            break;
        case AST_LET:
            fprintf(out, "    %zu [label=\"let (%.*s)\"];\n", node.id,
                PS(node.as.let.name));
            for (size_t j = 0; j < node.as.let.body.len; j++) {
                fprintf(out, "    %zu -> %zu;\n", node.id, node.as.let.body.data[j].id);
            }
            for (size_t j = 0; j < node.as.let.rhs.len; j++) {
                fprintf(out, "    %zu -> %zu;\n", node.id, node.as.let.rhs.data[j].id);
            }
            dump_ast(node.as.let.rhs, out);
            dump_ast(node.as.let.body, out);
            break;
        case AST_FN:
            fprintf(out, "    %zu [label=\"fn", node.id);
            for (size_t j = 0; j < node.as.fn.args_names.len; j++) {
                fprintf(out, " %.*s", PS(node.as.fn.args_names.data[j]));
            }
            fprintf(out, "\"];\n");
            for (size_t j = 0; j < node.as.fn.body.len; j++) {
                fprintf(out, "    %zu -> %zu;\n", node.id, node.as.fn.body.data[j].id);
            }
            dump_ast(node.as.fn.body, out);
            break;
        case AST_APPLY:
            fprintf(out, "    %zu [label=\"app (%.*s)\"];\n", node.id,
                PS(node.as.apply.name));
            for (size_t j = 0; j < node.as.apply.args.len; j++) {
                fprintf(out, "    %zu -> %zu;\n", node.id, node.as.apply.args.data[j].id);
            }
            dump_ast(node.as.apply.args, out);
            break;
        case AST_IF:
            fprintf(out, "    %zu [label=\"if\"];\n", node.id);
            for (size_t j = 0; j < node.as.iff.cond.len; j++) {
                fprintf(out, "    %zu -> %zu;\n", node.id, node.as.iff.cond.data[j].id);
            }
            dump_ast(node.as.iff.cond, out);
            for (size_t j = 0; j < node.as.iff.then.len; j++) {
                fprintf(out, "    %zu -> %zu;\n", node.id, node.as.iff.then.data[j].id);
            }
            dump_ast(node.as.iff.then, out);
            for (size_t j = 0; j < node.as.iff.elsee.len; j++) {
                fprintf(out, "    %zu -> %zu;\n", node.id, node.as.iff.elsee.data[j].id);
            }
            dump_ast(node.as.iff.elsee, out);
            break;
        case AST_BLOCK:
            fprintf(out, "    %zu [label=\"block\"];\n", node.id);
            for (size_t j = 0; j < node.as.block.len; j++) {
                fprintf(out, "    %zu -> %zu;\n", node.id, node.as.block.data[j].id);
            }
            dump_ast(node.as.block, out);
            break;
        }
    }
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

    if (target == TARGET_AST) {
        fprintf(out, "digraph {\n");
        dump_ast(body, out);
        fprintf(out, "}\n");
        goto exit;
    }

    if (typecheck(body).type != TYPE_UNIT) {
        fprintf(stderr, "File's evaluation result is not unit\n");
        exit(1);
    }

    //Node *ir = codegen(&arena, body, NULL);
    //for (size_t i = 0; i < ir.functions.len; i++) {
    //    StaticFunction *func = &ir.functions.data[i];
    //    bool repeat;
    //    do {
    //        repeat = peephole_optimization(&func->code);
    //        repeat |= remove_unused(&func->code);
    //        repeat |= constant_propagation(&func->code);
    //        repeat |= return_lifting(&arena, &func->code);
    //    } while (repeat);
    //    // try_tail_call_optimization(&arena, func, ir.symbols.data);
    //    remove_phi(&arena, &func->code);
    //    func->temps_count = fold_temporaries(func->code);
    //}
    //switch (target) {
    //case TARGET_DEBUG:
    //    codegen_debug(ir, out);
    //    break;
    //case TARGET_PPC:
    //    codegen_powerpc(ir, out);
    //    break;
    //case TARGET_X86_32:
    //    codegen_x86_32(ir, out);
    //    break;
    //case TARGET_MIPS:
    //    codegen_mips(ir, out);
    //    break;
    //case TARGET_PDP8:
    //    codegen_pdp8(ir, out);
    //    break;
    //case TARGET_AST:
    //    UNREACHABLE();
    //}
exit:
    arena_destroy(&arena);
    free(str.string);
}

void usage(FILE *out, const char *const program) {
    fprintf(out, "Usage: %s [-h] [-t <target>] [-o <output file>] <input file>\n\n", program);
    fprintf(out, "\t-t\tcompilation target\n");
    fprintf(out, "\t\t\tppc\t32 bit powerpc GAS\n");
    fprintf(out, "\t\t\tmips\tmips GAS\n");
    fprintf(out, "\t\t\tx86_32\t32 bit Intel x86 GAS\n");
    fprintf(out, "\t\t\tdebug\thuman-readable pseudocode\n");
    fprintf(out, "\t\t\tast\tpure ast dump in graphviz format\n");
    fprintf(out, "\t-h\tShows this help message\n");
    fprintf(out, "\t-o\tSpecifies the output file, the default one is stdout\n");
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
            bool found = false;
            for (size_t i = 0; i < ARRLEN(target_names); i++) {
                if (strcmp(target_str, target_names[i]) == 0) {
                    target = i;
                    found = true;
                }
            }
            if (!found) {
                fprintf(stderr, "Unknown target `%s`\n", target_str);
                arena_destroy(&arena);
                return 1;
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

