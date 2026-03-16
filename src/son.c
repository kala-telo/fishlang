#include "parser.h"
#include "todo.h"
#include "son.h"
#include "da.h"

inline static Node *alloc_node(Arena *a, Node n) {
    Node *p = arena_alloc(a, sizeof(n));
    *p = n;
    return p;
}

#define MAKE_NODE(...) \
    alloc_node(arena, (Node){.in = {0}, .out = {0}, .as = {0}, __VA_ARGS__})

static void add_parent(Arena *arena, Node *parent, Node *child) {
    if (!parent) return;
    da_append(arena, parent->in, child);
    da_append(arena, child->out, parent);
}

static void add_end(Arena *arena, Node *start, Node *end) {
    if (start->out.len == 0) {
        add_parent(arena, start, end);
        return;
    }
    for (size_t i = 0; i < start->out.len; i++) {
        add_end(arena, start->out.data[i], end);
    }
}

void dump_graph(Node *start) {
    TODO();
}

Node* codegen(Arena *arena, ASTArr ast, Node* parent) {
    if (!parent) {
        Node *start = MAKE_NODE(.type = NODE_START);
        Node *end   = MAKE_NODE(.type = NODE_RETURN);
        codegen(arena, ast, start);
        return start;
    }
    for (size_t i = 0; i < ast.len; i++) {
        AST ast_node = ast.data[i];
        switch (ast_node.kind) {
        case AST_STRING:
            printf("Unimplemented: AST_STRING\n");
            break;
        case AST_NUMBER:
            printf("Unimplemented: AST_NUMBER\n");
            break;
        case AST_BOOL:
            printf("Unimplemented: AST_BOOL\n");
            break;
        case AST_NAME:
            printf("Unimplemented: AST_NAME\n");
            break;
        case AST_LET: {
            Node *n = MAKE_NODE(.type = NODE_LET);
            add_parent(arena, parent, n);
            n->as.let = ast_node.as.let.name;
            codegen(arena, ast_node.as.let.body, n);
            codegen(arena, ast_node.as.let.rhs,  n);
        } break;
        case AST_FN: {
            // Node *n = MAKE_NODE(.type = NODE_FN);
            printf("Unimplemented: AST_FN\n");
        } break;
        case AST_APPLY:
            printf("Unimplemented: AST_APPLY\n");
            break;
        case AST_BLOCK:
            printf("Unimplemented: AST_BLOCK\n");
            break;
        case AST_IF:
            printf("Unimplemented: AST_IF\n");
            break;
        }
    }
}
