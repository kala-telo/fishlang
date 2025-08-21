#include <stdlib.h>

#include "parser.h"

#ifndef TYPING_H
#define TYPING_H

typedef struct {
    char name[120];
    size_t id;
} SymbolID;

typedef struct _Type Type;

struct _Type {
    enum {
        TYPE_VOID,
        TYPE_FUNCTION,
        TYPE_CSTR,
        TYPE_I32,
        TYPE_ANY, // changing it to variadic or so can be a good idea
        TYPE_BOOL,
    } type;
    union {
        struct {
            struct { // TYPE_FUNCTION
                Type* data;
                size_t len, capacity;
            } args;
            Type *ret;
        } func;
    } as;
};

typedef struct {
    struct {
        struct {
            SymbolID key;
            Type value;
        }* data;
        size_t len;
        size_t capacity;
    }* table;
    size_t length;
} TypeTable;

Type typecheck(ASTArr ast, TypeTable tt);
Type extract_types(Arena *arena, ASTArr ast, TypeTable *tt);

#endif
