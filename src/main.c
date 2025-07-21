#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "codegen.h"
#include "da.h"
#include "lexer.h"
#include "parser.h"
#include "string.h"
#include "tac.h"
#include "todo.h"

typedef struct {
    String function;
    String name;
    uint16_t scope;
} SymbolID;

typedef struct _Type Type;

struct _Type {
    enum {
        TYPE_FUNCTION,
        TYPE_CSTR,
        TYPE_INT,
        TYPE_ANY,
        TYPE_VOID,
    } type;
    union {
        struct { // TYPE_FUNCTION
            struct {
                Type* data;
                size_t len, capacity;
            } args;
            Type *return_value;
        } func;
    } as;
};

typedef struct {
    struct {
        struct {
            String key;
            Type value;
        } *data;
        size_t len, capacity;
    } *table;
    size_t length;
    struct {
        SymbolID *data;
        size_t len, capacity;
    } symbols;
} TypesTable;

#define typeid_to_string(typeid) ((String){(void*)(typeid), sizeof(SymbolID)})

typedef struct {
    String current_function;
    uint16_t current_scope;
    TypesTable tt;
} TypeCollectCtx;

void type_free(Type t) {
    switch (t.type) {
    case TYPE_VOID:
    case TYPE_ANY:
    case TYPE_CSTR:
    case TYPE_INT:
        break;
    case TYPE_FUNCTION:
        for (size_t i = 0; i < t.as.func.args.len; i++) {
            type_free(t.as.func.args.data[i]);
        }
        free(t.as.func.args.data);
        t.as.func.args.data = NULL;
        type_free(*t.as.func.return_value);
        free(t.as.func.return_value);
        t.as.func.return_value = NULL;
        break;
    }
}

void typestable_free(TypesTable *tt) {
    for (size_t i = 0; i < tt->length; i++) {
        for (size_t j = 0; j < tt->table[i].len; j++) {
            type_free(tt->table[i].data[j].value);
        }
        free(tt->table[i].data);
    }
    free(tt->table);
    tt->table = NULL;
    tt->table = 0;
    free(tt->symbols.data);
    tt->symbols.data = NULL;
}

Type extract_type(String type) {
    if (string_eq(type, S(":int"))) {
        return (Type){TYPE_INT, {}};
    } else if (string_eq(type, S(":cstr"))) {
        return (Type){TYPE_CSTR, {}};
    } else  if (string_eq(type, S("..."))) {
        return (Type){TYPE_ANY, {}};
    } else  if (string_eq(type, S(":todo"))) {
        return (Type){TYPE_ANY, {}};
    } else if (string_eq(type, S(":void"))) {
        return (Type){TYPE_VOID, {}};
    }
    fprintf(stderr, "%.*s\n", PS(type));
    TODO();
}

TypesTable extract_types(ASTArr *ast, TypeCollectCtx *ctx) {
    for (size_t i = 0; i < ast->len; i++) {
        AST *node = &ast->data[i];
        switch (node->kind) {
        case AST_EXTERN: {
            da_append(ctx->tt.symbols,
                      ((SymbolID){ctx->current_function, node->as.external.name,
                                  ctx->current_scope}));
            String type_id = typeid_to_string(&da_last(ctx->tt.symbols));
            Type type = (Type){TYPE_FUNCTION, {0}};
            for (size_t j = 0; j < node->as.external.args.len; j++) {
                Type t = extract_type(node->as.external.args.data[j].type);
                da_append(type.as.func.args, t);
            }
            type.as.func.return_value =
                calloc(1, sizeof(*type.as.func.return_value));
            *type.as.func.return_value = extract_type(node->as.external.ret);
            hmput(ctx->tt, type_id, type);
        } break;
        case AST_FUNCDEF: {
            da_append(ctx->tt.symbols,
                      ((SymbolID){ctx->current_function, node->as.func.name,
                                  ctx->current_scope}));
            String type_id = typeid_to_string(&da_last(ctx->tt.symbols));
            Type type = (Type){TYPE_FUNCTION, {0}};
            for (size_t j = 0; j < node->as.func.args.len; j++) {
                Type t = extract_type(node->as.func.args.data[j].type);
                da_append(type.as.func.args, t);
            }
            type.as.func.return_value =
                calloc(1, sizeof(*type.as.func.return_value));
            *type.as.func.return_value = extract_type(node->as.func.ret);
            hmput(ctx->tt, type_id, type);
        } break;
        case AST_VARDEF:
            TODO();
            break;
        case AST_CALL:
        case AST_LIST:
        case AST_NAME:
        case AST_NUMBER:
        case AST_STRING:
            TODO();
            break;
        }
    }
    return ctx->tt;
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
    };

    ASTArr body = {0};
    while (peek_token(&lex).kind != LEX_END)
        parse(&lex, &body);

    TypeCollectCtx tc_ctx = { 0 }; 
    TypesTable tt = extract_types(&body, &tc_ctx);
    typestable_free(&tt);

    CodeGenCTX cg_ctx = { 0 };
    IR ir = codegen(body, &cg_ctx);
    free_ctx(&cg_ctx);

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
