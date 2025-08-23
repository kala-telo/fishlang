#ifndef PARSER_H
#define PARSER_H

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "arena.h"
#include "lexer.h"

typedef struct _AST AST;

typedef enum {
    AST_FUNC,
    AST_DEF,
    AST_VARDEF,
    AST_EXTERN,
    AST_CALL,
    AST_NAME,
    AST_STRING,
    AST_NUMBER,
    AST_BOOL,
    AST_LIST,
} ASTKind;

typedef struct {
    struct _AST *data;
    size_t len, capacity;
} ASTArr;

typedef struct _VarDef VarDef;
typedef struct _Variable Variable;

struct _AST {
    ASTKind kind;
    union {
        struct { // AST_FUNC
            struct {
                VarDef *data;
                size_t len, capacity;
            } args;
            ASTArr ret;
            ASTArr body;
        } func;
        struct { // AST_CALL
            String callee;
            ASTArr args;
        } call;
        String string;  // AST_STRING
        String name;    // AST_NAME
        ASTArr list;    // AST_LIST
        int64_t number; // AST_NUMBER
        bool boolean;   // AST_BOOL
        struct {        // AST_VARDEF
            struct {
                Variable *data;
                size_t len, capacity;
            } variables;
            ASTArr body;
        } var;
        struct {
            String name;
            ASTArr body;
        } external; // AST_EXTERN
        struct {
            String name;
            ASTArr body;
        } def; // AST_DEF
    } as;
    Location loc;
    size_t id;
    AST *parent;
};

struct _VarDef {
    String name;
    ASTArr type;
};

struct _Variable {
    VarDef definition;
    ASTArr value;
};

void parse(Arena *arena, Lexer *lex, ASTArr *arr, AST* parent, size_t *node_id);
void dump_ast(ASTArr ast, int indent);

#endif // PARSER_H
