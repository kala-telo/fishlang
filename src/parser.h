#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include "lexer.h"

typedef struct _AST AST;

typedef enum {
    AST_FUNCDEF,
    AST_VARDEF,
    AST_EXTERN,
    AST_CALL,
    AST_NAME,
    AST_STRING,
    AST_NUMBER,
    AST_LIST,
} ASTKind;

typedef struct {
    struct _AST *data;
    size_t len, capacity;
} ASTArr;

typedef struct {
    String name;
    String type;
} VarDef;

typedef struct {
    VarDef definition;
    ASTArr value;
} Variable;

struct _AST {
    ASTKind kind;
    union {
        struct { // AST_FUNCDEF
            String name;
            struct {
                VarDef *data;
                size_t len, capacity;
            } args;
            String ret;
            ASTArr body;
        } func;
        struct { // AST_CALL
            String callee;
            ASTArr args;
        } call;
        String string; // AST_STRING
        String name; // AST_NAME
        ASTArr list; // AST_LIST
        int64_t number; // AST_NUMBER
        struct { // AST_VARDEF
            struct {
                Variable *data;
                size_t len, capacity;
            } variables;
            ASTArr body;
        } var;
        struct {
            String name;
            struct {
                VarDef *data;
                size_t len, capacity;
            } args;
            String ret;
        } external; // AST_EXTERN
    } as;
};

void parse(Lexer *lex, ASTArr *parent);
void free_ast(ASTArr *ast);
void dump_ast(ASTArr ast, int indent);

#endif // PARSER_H
