#include <stdio.h>

#include "da.h"
#include "parser.h"
#include "todo.h"
#include "typing.h"

#define PLOC(x) (x).file, (x).line+1, (x).col+1
#define AST2ARR(x) (ASTArr){&(x), 1, 0}

SymbolID make_sym(size_t id, String name) {
    SymbolID tid = { 0 };
    snprintf(tid.name, sizeof(tid.name), "%.*s", PS(name));
    tid.id = id;
    return tid;
}

void insert_type(Arena *arena, TypeTable *tt, size_t id, String name, Type type) {
    SymbolID tid = make_sym(id, name);
    hm_put(arena, *tt, tid, type);
}

Type get_type(TypeTable tt, size_t id, String name) {
    SymbolID tid = make_sym(id, name);
    Type r;
    bool found;
    hm_get(r, tt, tid, found);
    assert(found);
    return r;
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
    if (t1.type != t2.type)
        return false;
    if (t1.type != TYPE_FUNCTION)
        return true;
    if (!types_match(*t1.as.func.ret, *t2.as.func.ret))
        return false;
    if (t1.as.func.args.len != t2.as.func.args.len)
        return false;
    for (size_t i = 0; i < t1.as.func.args.len; i++) {
        if (!types_match(t1.as.func.args.data[i], t2.as.func.args.data[i]))
            return false;
    }
    return true;
}

bool type_int(Type t) {
    switch (t.type) {
    case TYPE_I32:
        return true;
    default:
        return false;
    }
}

Type extract_types(Arena *arena, ASTArr ast, TypeTable *tt) {
    Type r = {0};
    for (size_t i = 0; i < ast.len; i++) {
        AST node = ast.data[i];
        switch (node.kind) {
        case AST_DEF: {
            Type t = extract_types(arena, node.as.def.body, tt);
            insert_type(arena, tt, node.parent != NULL ? node.id : 0,
                        node.as.def.name, t);
            extract_types(arena, node.as.def.body, tt);
        } break;
        case AST_EXTERN: {
            Type t = extract_types(arena, node.as.external.body, tt);
            insert_type(arena, tt, node.parent != NULL ? node.id : 0,
                        node.as.external.name, t);
        } break;
        case AST_FUNC: {
            r.type = TYPE_FUNCTION;
            assert(node.as.func.ret.len == 1);
            Type ret = extract_types(arena, node.as.func.ret, tt);
            extract_types(arena, node.as.func.body, tt);
            r.as.func.ret = arena_alloc(arena, sizeof(ret));
            memcpy(r.as.func.ret, &ret, sizeof(ret));
            for (size_t j = 0; j < node.as.func.args.len; j++) {
                ASTArr ast_type = node.as.func.args.data[j].type;
                assert(ast_type.len == 1);
                Type type = extract_types(arena, ast_type, tt);
                String name = node.as.func.args.data[j].name;
                da_append(arena, r.as.func.args, type);
                insert_type(arena, tt, node.parent != NULL ? node.id : 0, name,
                            type);
            }
        } break;
        case AST_VARDEF:
            for (size_t j = 0; j < node.as.var.variables.len; j++) {
                String name = node.as.var.variables.data[j].definition.name;
                Type type = extract_types(
                    arena, node.as.var.variables.data[j].definition.type, tt);
                insert_type(arena, tt, node.parent != NULL ? node.id : 0, name,
                            type);
                extract_types(arena, node.as.var.variables.data[j].value, tt);
            }
            extract_types(arena, node.as.var.body, tt);
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
                fprintf(stderr, "Unknown name -- %.*s at %s:%d:%d\n",
                        PS(node.as.name), node.loc.file, node.loc.line + 1,
                        node.loc.col + 1);
                TODO();
            }
            break;
        case AST_BOOL:
            r.type = TYPE_BOOL;
            break;
        case AST_NUMBER:
            r.type = TYPE_I32;
            break;
        case AST_STRING:
            r.type = TYPE_CSTR;
            break;
        case AST_LIST:
            TODO();
            break;
        case AST_CALL:
            break;
        }
    }
    return r;
}

void print_type(FILE* out, Type t) {
    switch (t.type) {
    case TYPE_VOID:
        fprintf(out, "void");
        break;
    case TYPE_ANY:
        fprintf(out, "...");
        break;
    case TYPE_BOOL:
        fprintf(out, "bool");
        break;
    case TYPE_CSTR:
        fprintf(out, "cstr");
        break;
    case TYPE_I32:
        fprintf(out, "i32");
        break;
    case TYPE_FUNCTION:
        fprintf(out, "(fn [");
        for (size_t i = 0; i < t.as.func.args.len; i++) {
            print_type(out, t.as.func.args.data[i]);
            if (i != t.as.func.args.len-1) putc(' ', out);
        }
        fprintf(out, "] ");
        print_type(out, *t.as.func.ret);
        fprintf(out, ")");
        break;
    default:
        TODO();
        break;
    }
}

Type find_type(TypeTable tt, AST from, String name) {
    if (from.parent == NULL) {
        SymbolID id = make_sym(0, name);
        Type t;
        bool found;
        hm_get(t, tt, id, found);
        if (found) {
            return t;
        }
        fprintf(stderr, "Couldn't find name: %.*s\n", PS(name));
        TODO();
    }
    AST parent = *from.parent;
    SymbolID id = make_sym(parent.id, name);
    Type t;
    bool found;
    hm_get(t, tt, id, found);
    if (found) {
        return t;
    }
    return find_type(tt, parent, name);
}

Type typecheck(ASTArr ast, TypeTable tt) {
    Type t = {0};
    for (size_t i = 0; i < ast.len; i++) {
        AST node = ast.data[i];
        switch (node.kind) {
        case AST_EXTERN:
            break;
        case AST_DEF:
            typecheck(node.as.def.body, tt);
            break;
        case AST_FUNC: {
            Type return_type = typecheck(node.as.func.body, tt);
            Type expected_type = extract_types(NULL, node.as.func.ret, NULL);
            if (!types_match(return_type, expected_type) && expected_type.type != TYPE_VOID) {
                fprintf(stderr, "%s:%d:%d -- ", PLOC(da_last(node.as.func.body).loc));
                fprintf(stderr, "Type `");
                print_type(stderr, return_type);
                fprintf(stderr, "` doesn't match type `"); 
                print_type(stderr, expected_type);
                fprintf(stderr, "`\n");
                exit(1);
            }
            t = return_type;
        } break;
        case AST_CALL: {
            if (string_eq(node.as.call.callee, S("+")) ||
                string_eq(node.as.call.callee, S("-"))) {
                assert(node.as.call.args.len == 2);
                Type t1 = typecheck(AST2ARR(node.as.call.args.data[0]), tt);
                Type t2 = typecheck(AST2ARR(node.as.call.args.data[1]), tt);
                if (!types_match(t1, t2)) {
                    fprintf(stderr, "%s:%d:%d -- ", PLOC(node.loc));
                    fprintf(stderr, "Type `");
                    print_type(stderr, t1);
                    fprintf(stderr, "` doesn't match type `"); 
                    print_type(stderr, t2);
                    fprintf(stderr, "`\n");
                }
                if (!type_int(t1)) {
                    TODO();
                }
                t.type = t1.type;
                break;
            } else if (string_eq(node.as.call.callee, S("<")) ||
                       string_eq(node.as.call.callee, S(">"))) {
                Type t1 = typecheck(AST2ARR(node.as.call.args.data[0]), tt);
                Type t2 = typecheck(AST2ARR(node.as.call.args.data[1]), tt);
                if (!types_match(t1, t2)) {
                    TODO();
                }
                if (!type_int(t1)) {
                    TODO();
                }
                t.type = TYPE_BOOL;
                break;
            } else if (string_eq(node.as.call.callee, S("if"))) {
                assert(node.as.call.args.len == 3);
                Type condition = typecheck(AST2ARR(node.as.call.args.data[0]), tt);
                if (condition.type != TYPE_BOOL)
                    TODO();
                Type t1 = typecheck(AST2ARR(node.as.call.args.data[1]), tt);
                Type t2 = typecheck(AST2ARR(node.as.call.args.data[2]), tt);
                if (!types_match(t1, t2)) {
                    TODO();
                }
                t.type = t1.type;
                break;
            }
            Type call_type = find_type(tt, node, node.as.call.callee);
            assert(call_type.type == TYPE_FUNCTION);
            if (call_type.as.func.args.len > node.as.call.args.len)
                TODO();
            if (da_last(call_type.as.func.args).type != TYPE_ANY &&
                call_type.as.func.args.len != node.as.call.args.len) {
            }
            for (size_t j = 0; j < node.as.call.args.len; j++) {
                Type t1 = call_type.as.func.args.data[j],
                     t2 = typecheck(AST2ARR(node.as.call.args.data[j]), tt);
                if (t1.type == TYPE_ANY)
                    break;
                if (!types_match(t1, t2)) {
                    TODO();
                }
            }
            t = *call_type.as.func.ret;
        } break;
        case AST_VARDEF:
            t = typecheck(node.as.var.body, tt);
            break;
        case AST_NAME:
            t = find_type(tt, node, node.as.name);
            break;
        case AST_BOOL:
            t.type = TYPE_BOOL;
            break;
        case AST_LIST:
            TODO();
            break;
        case AST_NUMBER:
            t.type = TYPE_I32;
            break;
        case AST_STRING:
            t.type = TYPE_CSTR;
            break;
        }
    }
    return t;
}
