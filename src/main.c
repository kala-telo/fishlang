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

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file.fsh>\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (f == NULL) {
        fprintf(stderr, "Couldn't open file %s\n", argv[1]);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    int size = ftell(f);
    fseek(f, 0, SEEK_SET);

    String str = {0};
    str.string = malloc(size * sizeof(char));
    str.length = size;

    size = fread(str.string, 1, size, f);
    fclose(f);

    Lexer lex = {
        // -1 for \0, though it still must be in memory for
        // lexer to not read into unallocated memory
        .length = size - 1,
        .position = str.string,
        .loc = {0, 0, argv[1]},
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
        ir.functions.data[i].temps_count =
            fold_temporaries(ir.functions.data[i].code);
    }
    codegen_powerpc(ir, stdout);
    // codegen_debug(ir, stdout);

    arena_destroy(&arena);
    free(str.string);
}
