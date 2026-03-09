#include <stdio.h>

#include "da.h"
#include "parser.h"
#include "string.h"
#include "todo.h"
#include "typing.h"

#define PLOC(x) (x).file, (x).line+1, (x).col+1
#define AST2ARR(x) (ASTArr){&(x), 1, 0}

SymbolID make_sym(size_t id, String name) {
}

void insert_type(Arena *arena, TypeTable *tt, size_t id, String name, Type type) {
}

Type get_type(TypeTable tt, size_t id, String name) {
}

#if 0
Type string2type(String str, Location loc) {
    if (string_eq(str, S("i32"))) {
        return (Type){TYPE_I32, {0}};
    } else if (string_eq(str, S("cstr"))) {
        return (Type){TYPE_CSTR, {0}};
    } else if (string_eq(str, S("..."))) {
        return (Type){TYPE_ANY, {0}};
    } else if (string_eq(str, S("void"))) {
        return (Type){TYPE_VOID, {0}};
    } else {
        fprintf(stderr, "Unknown type \"%.*s\" at %s:%d:%d\n", PS(str),
                loc.file, loc.line + 1, loc.col + 1);
        TODO();
        return (Type){0};
    }
}
#endif

bool types_match(Type t1, Type t2) {
    return false;
}

bool type_int(Type t) {
}

Type extract_types(Arena *arena, ASTArr ast, TypeTable *tt) {
}

void print_type(FILE* out, Type t) {
}

Type find_type(TypeTable tt, AST from, String name) {
}

Type typecheck(ASTArr ast, TypeTable tt) {
}
