#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "codegen.h"
#include "string.h"
#include "lexer.h"
#include "todo.h"
#include "tac.h"
#include "da.h"

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
    };

    ASTArr body = {0};
    while (peek_token(&lex).kind != LEX_END)
        parse(&lex, &body);

    CodeGenCTX *ctx = calloc(1, sizeof(CodeGenCTX));
    IR ir = codegen(body, ctx);
    free_ctx(ctx);

    for (size_t i = 0; i < ir.functions.len; i++)
        ir.functions.data[i].temps_count =
            fold_temporaries(ir.functions.data[i].code);
    codegen_powerpc(ir, stdout);
    // codegen_debug(ir, stdout);

    // it's not like you really need to free it
    // but i wanted to make ASAN and valgrind happy
    free_ir(&ir);
    free_ast(&body);
    free(body.data);
    free(str.string);
}
