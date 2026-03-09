#ifndef PARSER_H
#define PARSER_H

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "arena.h"
#include "lexer.h"

typedef struct _AST AST;

typedef enum {
    // OLD
    AST_FUNC,
    AST_DEF,
    AST_VARDEF,
    AST_EXTERN,
    AST_CALL,
    AST_NUMBER,
    AST_BOOL,
    AST_LIST,
    // NEW
    AST_STRING,
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

typedef struct {
    String name;
    TypeAST type;
} FnArg;

struct _AST {
    ASTKind kind;
    union {
        // OLD
        struct { // AST_FUNC
            struct {
                VarDef *data;
                size_t len, capacity;
            } args;
            ASTArr ret;
            ASTArr body;
            bool ret_type_void;
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
        // NEW
        struct {
            String name;
            ASTArr rhs;
            ASTArr body;
        } let; // AST_LET
        struct {
            struct {
                FnArg* data;
                size_t len, capacity;
            } args;
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

#endif // PARSER_H
