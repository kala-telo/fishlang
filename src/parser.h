#ifndef PARSER_H
#define PARSER_H

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "arena.h"
#include "lexer.h"

typedef struct _AST AST;

typedef enum {
    AST_STRING,
    AST_NUMBER,
    AST_BOOL,
    AST_NAME,
    AST_LET,
    AST_FN,
    AST_APPLY,
    AST_BLOCK,
    AST_IF,
} ASTKind;

typedef struct {
    struct _AST *data;
    size_t len, capacity;
} ASTArr;

typedef struct _VarDef VarDef;
typedef struct _Variable Variable;

typedef struct _TypeAST TypeAST;

struct _TypeAST {
    enum {
        TYPE_I32,
        TYPE_CSTR,
        TYPE_UNIT,
        TYPE_FN,
        TYPE_BOOL,
        TYPE_VARIADIC,
    } type;
    union {
        struct {
            // last type is the return type
            TypeAST *data;
            size_t len, capacity;
        } fn;
    } as;
};


struct _AST {
    ASTKind kind;
    union {
        struct {
            String name;
            ASTArr rhs;
            ASTArr body;
        } let; // AST_LET
        struct {
            struct {
                String* data;
                size_t len, capacity;
            } args_names;
            struct {
                TypeAST* data;
                size_t len, capacity;
            } args_types;
            ASTArr body;
        } fn; // AST_FN
        struct {
            String name;
            ASTArr args;
        } apply; // AST_APPLY
        ASTArr block; // AST_BLOCK
        struct {
            ASTArr cond;
            ASTArr then;
            ASTArr elsee;
        } iff; // AST_IF
        String string;  // AST_STRING
        String name;    // AST_NAME
        int64_t number; // AST_NUMBER
        bool boolean;   // AST_BOOL
    } as;
    Location loc;
    size_t id;
    AST *parent;
    struct {
        TypeAST type;
        bool checked;
    } typing;
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

#endif // PARSER_H
