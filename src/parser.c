#include "parser.h"
#include "da.h"
#include "lexer.h"
#include "string.h"
#include "todo.h"
#include <stdio.h>

Token expect(Token token, TokenKind expected) {
    if (token.kind != expected) {
        switch (token.kind) {
        case LEX_STRING:
        case LEX_NAME:
            fprintf(stderr, "%s:%d:%d Expected %s, got %s [%.*s]\n",
                    token.loc.file, token.loc.line + 1, token.loc.col + 1,
                    tok_names[expected], tok_names[token.kind], PS(token.str));
            break;
        default:
            fprintf(stderr, "%s:%d:%d Expected %s, got %s\n", token.loc.file,
                    token.loc.line + 1, token.loc.col + 1, tok_names[expected],
                    tok_names[token.kind]);
            break;
        }
        abort();
    }
    return token;
}

int64_t s_atoi(String s) {
    int64_t result = 0;
    for (int len = 0; len < s.length; len++) {
        result *= 10;
        result += s.string[len] - '0';
    }
    return result;
}

// NOTE: i've kinda realized that i'm using lex->loc everywhere and it seems weird
// maybe it's fine but i should consider using token position l8r
// NOTE: maybe consider spliting it into parsing 1 thing and array of things
void parse(Arena *arena, Lexer *lex, ASTArr *arr, AST* parent, size_t *node_id) {
    Token t = next_token(lex);
    switch (t.kind) {
    case LEX_OPAREN: {
        Token t = expect(next_token(lex), LEX_NAME);
        if (string_eq(S("fn"), t.str)) {
            da_append(arena, *arr, ((AST){AST_FUNC, {0}, lex->loc, (*node_id)++, parent}));
            AST *func = &da_last(*arr);
            expect(next_token(lex), LEX_OBRAKET);
            while (peek_token(lex).kind != LEX_CBRAKET) {
                if (peek_token(lex).kind == LEX_NAME) {
                    String name = expect(next_token(lex), LEX_NAME).str;
                    ASTArr type = { 0 };
                    da_append(arena, type,
                              ((AST){AST_NAME, .as.name = name, lex->loc,
                                     (*node_id)++, func}));
                    da_append(arena, func->as.func.args, ((VarDef){{0}, type}));
                } else {
                    expect(next_token(lex), LEX_OPAREN);
                    String name = expect(next_token(lex), LEX_NAME).str;
                    ASTArr type = { 0 };
                    parse(arena, lex, &type, func, node_id);
                    da_append(arena, func->as.func.args,
                              ((VarDef){name, type}));
                    expect(next_token(lex), LEX_CPAREN);
                }
            }
            expect(next_token(lex), LEX_CBRAKET);
            parse(arena, lex, &func->as.func.ret, func, node_id);
            assert(func->as.func.ret.len == 1);
            while (peek_token(lex).kind != LEX_CPAREN) {
                parse(arena, lex, &func->as.func.body, func, node_id);
            }
        } else if (string_eq(S("let"), t.str)) {
            da_append(arena, *arr, ((AST){AST_VARDEF, {0}, lex->loc, (*node_id)++, parent}));
            AST *vars = &da_last(*arr);
            expect(next_token(lex), LEX_OBRAKET);
            while (peek_token(lex).kind != LEX_CBRAKET) {
                expect(next_token(lex), LEX_OPAREN);
                String name = expect(next_token(lex), LEX_NAME).str;
                ASTArr type = { 0 };
                parse(arena, lex, &type, vars, node_id);
                da_append(arena, vars->as.var.variables, ((Variable){{name, type}, {0}}));
                parse(arena, lex, &da_last(vars->as.var.variables).value, vars, node_id);
                expect(next_token(lex), LEX_CPAREN);
            }
            expect(next_token(lex), LEX_CBRAKET);
            while (peek_token(lex).kind != LEX_CPAREN) {
                parse(arena, lex, &vars->as.var.body, vars, node_id);
            }
        } else if (string_eq(S("def"), t.str)) {
            da_append(arena, *arr, ((AST){AST_DEF, {0}, lex->loc, (*node_id)++, parent}));
            AST *def = &da_last(*arr);
            def->as.def.name = expect(next_token(lex), LEX_NAME).str;
            parse(arena, lex, &def->as.def.body, def, node_id);
            assert(def->as.def.body.len == 1);
        } else if (string_eq(S("extern"), t.str)) {
            da_append(arena, *arr, ((AST){AST_EXTERN, {0}, lex->loc, (*node_id)++, parent}));
            AST *external = &da_last(*arr);
            external->as.external.name = expect(next_token(lex), LEX_NAME).str;
            parse(arena, lex, &external->as.external.body, external, node_id);
            assert(external->as.external.body.len == 1);
        } else {
            da_append(arena, *arr, ((AST){AST_CALL, {0}, lex->loc, (*node_id)++, parent}));
            AST *call = &da_last(*arr);
            call->as.call.callee = t.str;
            while (peek_token(lex).kind != LEX_CPAREN) {
                parse(arena, lex, &call->as.call.args, call, node_id);
            }
        }
        expect(next_token(lex), LEX_CPAREN);
    } break;
    case LEX_BOOL:
        da_append(arena, *arr,
                  ((AST){AST_BOOL, {0}, lex->loc, (*node_id)++, parent}));
        if (string_eq(t.str, S("true"))) {
            da_last(*arr).as.boolean = true;
        } else if (string_eq(t.str, S("false"))) {
            da_last(*arr).as.boolean = false;
        } else {
            TODO();
        }
        break;
    case LEX_STRING:
        da_append(arena, *arr,
                  ((AST){AST_STRING, {0}, lex->loc, (*node_id)++, parent}));
        da_last(*arr).as.string = t.str;
        break;
    case LEX_NUMBER:
        da_append(arena, *arr,
                  ((AST){AST_NUMBER, {0}, lex->loc, (*node_id)++, parent}));
        da_last(*arr).as.number = s_atoi(t.str);
        break;
    case LEX_NAME:
        da_append(arena, *arr,
                  ((AST){AST_NAME, {0}, lex->loc, (*node_id)++, parent}));
        da_last(*arr).as.name = t.str;
        break;
    case LEX_OBRAKET: {
        da_append(arena, *arr,
                  ((AST){AST_LIST, {0}, lex->loc, (*node_id)++, parent}));
        AST *list = &da_last(*arr);
        while (peek_token(lex).kind != LEX_CBRAKET) {
            parse(arena, lex, &list->as.list, list, node_id);
        }
        expect(next_token(lex), LEX_CBRAKET);
    } break;
    default:
        printf("%s:%d:%d Unexpected token: %s\n", t.loc.file, t.loc.line + 1,
               t.loc.col + 1, tok_names[t.kind]);
        TODO();
        break;
    }
}
