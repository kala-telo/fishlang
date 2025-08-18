#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"
#include "da.h"
#include "lexer.h"
#include "parser.h"
#include "string.h"
#include "tac.h"
#include "todo.h"

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

void free_type(Type t) {
    // printf("%d\n", t.type);
    if (t.type != TYPE_FUNCTION)
        return;
    free(t.as.func.ret);
    for (size_t i = 0; i < t.as.func.args.len; i++) {
        free_type(t.as.func.args.data[i]);
    }
    free(t.as.func.args.data);
}

void free_typetable(TypeTable *tt) {
    for (size_t i = 0; i < tt->length; i++) {
        for (size_t j = 0; j < tt->table[i].len; j++) {
            Type t = tt->table[i].data[j].value;
            free_type(t);
        }
        free(tt->table[i].data);
    }
    free(tt->table);
    tt->table = NULL;
    tt->length = 0;
}

void insert_type(Arena *arena, TypeTable *tt, size_t id, String name, Type type) {
    SymbolID tid = { 0 };
    snprintf(tid.name, sizeof(tid.name), "%.*s", PS(name));
    tid.id = id;
    hm_put(arena, *tt, tid, type);
}

Type extract_types(Arena *arena, ASTArr ast, TypeTable *tt) {
    Type r = {0};
    for (size_t i = 0; i < ast.len; i++) {
        AST node = ast.data[i];
        switch (node.kind) {
        case AST_BOOL:
            TODO();
            break;
        case AST_CALL:
            TODO();
            break;
        case AST_DEF: {
            Type t = extract_types(arena, node.as.def.body, tt);
            insert_type(arena, tt, node.id, node.as.def.name, t);
            extract_types(arena, node.as.def.body, tt);
        } break;
        case AST_EXTERN: {
            Type t = extract_types(arena, node.as.external.body, tt);
            insert_type(arena, tt, node.id, node.as.external.name, t);
        } break;
        case AST_FUNC: {
            r.type = TYPE_FUNCTION;
            Type ret = extract_types(arena, node.as.func.ret, tt);
            r.as.func.ret = arena_alloc(arena, sizeof(ret));
            memcpy(r.as.func.ret, &ret, sizeof(ret));
            for (size_t j = 0; j < node.as.func.args.len; j++) {
                Type type = extract_types(arena, node.as.func.args.data[j].type, tt);
                String name = node.as.func.args.data[j].name;
                da_append(arena, r.as.func.args, type);
                insert_type(arena, tt, node.id, name, type);
            }
        } break;
        case AST_LIST:
            TODO();
            break;
        case AST_NUMBER:
            TODO();
            break;
        case AST_STRING:
            TODO();
            break;
        case AST_VARDEF:
            TODO();
            break;
        case AST_NAME:
            if (string_eq(node.as.name, S("i32"))) {
                r = (Type){TYPE_I32, {0}};
            } else if (string_eq(node.as.name, S("cstr"))) {
                r = (Type){TYPE_CSTR, {0}};
            } else if (string_eq(node.as.name, S("..."))) {
                r = (Type){TYPE_ANY, {0}};
            } else if (string_eq(node.as.name, S("void"))) {
                r = (Type){TYPE_VOID, {0}};
            } else {
                fprintf(stderr, "%.*s\n", PS(node.as.name));
                TODO();
            }
            break;
        }
    }
    return r;
}

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
        size_t node_id = 0;
        while (peek_token(&lex).kind != LEX_END)
            parse(&arena, &lex, &body, &node_id);
    }

    TypeTable tt = {0};
    extract_types(&arena, body, &tt);

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
