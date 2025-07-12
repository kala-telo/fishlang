#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>
#include <assert.h>
#include "lexer.h"

#define TODO()                                                                 \
    do {                                                                       \
        fprintf(stderr, "%s:%d: TODO\n", __FILE__, __LINE__);                  \
        abort();                                                               \
    } while (0)

typedef enum {
    AST_FUNCDEF,
    AST_CALL,
    AST_NAME,
    AST_STRING,
    AST_LIST,
} ASTKind;

typedef struct _ASTArr {
    struct _AST *data;
    size_t len, capacity;
} ASTArr;

typedef struct _AST {
    ASTKind kind;
    union {
        struct { // AST_FUNCDEF
            String name;
            ASTArr body;
        } func;
        struct { // AST_CALL
            String callee;
            ASTArr args;
        } call;
        String string; // AST_STRING
        String name; // AST_NAME
        ASTArr list; // AST_LIST
    } as;
} AST;

void parse(Lexer *lex, ASTArr *parent);
void free_ast(ASTArr *ast);
void dump_ast(ASTArr ast, int indent);

#endif // PARSER_H
