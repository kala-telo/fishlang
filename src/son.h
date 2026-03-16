#include <stddef.h>
#include "arena.h"
#include "parser.h"

#ifndef SON_H
#define SON_H
typedef struct _Node Node;

struct _Node {
    struct {
        Node **data;
        size_t len, capacity;
    } in, out;
    enum {
        NODE_START,
        NODE_RETURN,
        NODE_CONST,
        NODE_LET,
    } type;
    union {
        String let;
    } as;
};
Node* codegen(Arena *arena, ASTArr ast, Node* parent);
#endif
