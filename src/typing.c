#include <stdio.h>

#include "da.h"
#include "parser.h"
#include "string.h"
#include "todo.h"
#include "typing.h"

#define PLOC(x) (x).file, (x).line+1, (x).col+1
#define AST2ARR(x) (ASTArr){&(x), 1, 0}

bool match_types(TypeAST t1, TypeAST t2) {
    if (t1.type == TYPE_VARIADIC || t2.type == TYPE_VARIADIC) return true;
    if (t1.type != t2.type) return false;
    if (t1.type != TYPE_FN) return true;
    if (t1.as.fn.len != t2.as.fn.len) return false;
    for (size_t i = 0; i < t1.as.fn.len; i++) {
        if (!match_types(t1.as.fn.data[i], t2.as.fn.data[i]))
            return false;
    }
    return true;
}

TypeAST find_type(AST starting, Location loc, String name) {
    // TODO: unhardcode builtin operators
    if (string_eq(name, S("+")) || string_eq(name, S("-"))) {
        static TypeAST sign[] = {
            {TYPE_I32, {}},
            {TYPE_I32, {}},
            {TYPE_I32, {}},
        };
        static TypeAST int_binop = {TYPE_FN, {sign, 3, 0}};
        return int_binop;
    }
    if (string_eq(name, S("<")) || string_eq(name, S(">"))) {
        static TypeAST sign[] = {
            {TYPE_I32, {}},
            {TYPE_I32, {}},
            {TYPE_BOOL, {}},
        };
        static TypeAST int_binop = {TYPE_FN, {sign, 3, 0}};
        return int_binop;
    }
    if (starting.kind == AST_LET) {
        if (string_eq(starting.as.let.name, name)) {
            return typecheck(starting.as.let.rhs);
        }
    }
    if (starting.kind == AST_FN) {
        assert(starting.as.fn.args_names.len == starting.as.fn.args_types.len - 1);
        for (size_t i = 0; i < starting.as.fn.args_names.len; i++) {
            if (!string_eq(starting.as.fn.args_names.data[i], name))
                continue;
            return starting.as.fn.args_types.data[i];
        }
    }

    if (!starting.parent) {
        fprintf(stderr, "%s:%d:%d: Couldn't find a type for the name `%.*s'\n",
            PLOC(loc), PS(name));
        abort();
    }
    return find_type(*starting.parent, loc, name);
}

TypeAST typecheck(ASTArr ast) {
    TypeAST t = {0};
    for (size_t i = 0; i < ast.len; i++) {
        AST node = ast.data[i];
        if (node.typing.checked) {
            t = node.typing.type;
            continue;
        }
        switch (node.kind) {
        case AST_LET:
            typecheck(node.as.let.rhs);
            t = typecheck(node.as.let.body);
            break;
        case AST_FN: {
            TypeAST ret = da_last(node.as.fn.args_types);
            // marking it ahead of time to not check body recursively
            t.type = TYPE_FN;
            t.as.fn.data = node.as.fn.args_types.data;
            t.as.fn.len = node.as.fn.args_types.len;
            ast.data[i].typing.checked = true;
            ast.data[i].typing.type = t;
            if (ret.type != TYPE_UNIT && !match_types(ret, typecheck(node.as.fn.body))) {
                fprintf(stderr, "%s:%d:%d: Return type doesn't match with function signature",
                    PLOC(node.loc));
                TODO();
            }
        } break;
        case AST_NUMBER:
            t.type = TYPE_I32;
            break;
        case AST_STRING:
            t.type = TYPE_CSTR;
            break;
        case AST_BOOL:
            t.type = TYPE_BOOL;
            break;
        case AST_APPLY: {
            TypeAST app_type = find_type(node, node.loc, node.as.apply.name);
            if (node.as.apply.args.len == 0) {
                t = app_type;
                break;
            }
            if (app_type.type != TYPE_FN) {
                fprintf(stderr, "app_type.type [%d] = %d\n",
                    node.as.apply.args.len, app_type.type);
                TODO(); // supposedly those are supposed to be handled by above
            }
            // -1 for return type
            if (app_type.as.fn.len - 1 != node.as.apply.args.len) {
                fprintf(stderr, "node.as.apply.name: %.*s\n", PS(node.as.apply.name));
                TODO(); // TODO: handle variadics
            }
            for (size_t j = 0; j < app_type.as.fn.len - 1; j++) {
                bool m = match_types(
                    app_type.as.fn.data[j],
                    typecheck(AST2ARR(node.as.apply.args.data[j]))
                );
                if (!m) {
                    fprintf(stderr, "%s:%d:%d: Argument types do not match for `%.*s'\n",
                        PLOC(node.loc), PS(node.as.apply.name));
                    TODO(); // error message
                }
            }
            t = da_last(app_type.as.fn);
        } break;
        case AST_IF: {
            TypeAST t1 = typecheck(node.as.iff.then);
            TypeAST t2 = typecheck(node.as.iff.elsee);
            if (!match_types(t1, t2)) {
                TODO();
            }
            if (typecheck(node.as.iff.cond).type != TYPE_BOOL) {
                fprintf(stderr, "%s:%d:%d: Condition type is not bool\n", PLOC(node.loc));
                TODO();
            }
            t = t1;
        } break;
        case AST_BLOCK:
            t = typecheck(node.as.block);
            break;
        case AST_UNIT:
            t.type = TYPE_UNIT;
            break;
        default:
            fprintf(stderr, "unimplemented node kind %d\n", node.kind);
            TODO();
        }
        ast.data[i].typing.checked = true;
        ast.data[i].typing.type = t;
    }
    return t;
}
