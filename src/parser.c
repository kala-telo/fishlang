#include "parser.h"
#include "da.h"
#include "lexer.h"
#include "string.h"
#include "todo.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

Token expect(Token token, TokenKind expected) {
    if (token.kind != expected) {
        switch (token.kind) {
        case LEX_STRING:
        case LEX_NAME:
            fprintf(stderr, "%s:%d%d Expected %s, got %s [%.*s]\n",
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

void parse(Arena *arena, Lexer *lex, ASTArr *parent, size_t *node_id) {
    Token t = next_token(lex);
    switch (t.kind) {
    case LEX_OPAREN: {
        Token t = expect(next_token(lex), LEX_NAME);
        if (string_eq(S("fn"), t.str)) {
            da_append(arena, *parent, ((AST){AST_FUNC, {0}, lex->loc, (*node_id)++}));
            AST *func = &da_last(*parent);
            expect(next_token(lex), LEX_OBRAKET);
            while (peek_token(lex).kind != LEX_CBRAKET) {
                if (peek_token(lex).kind == LEX_NAME) {
                    String name = expect(next_token(lex), LEX_NAME).str;
                    ASTArr type = { 0 };
                    da_append(arena, type, ((AST){AST_NAME, {.string = name}, lex->loc, (*node_id)++}));
                    da_append(arena, func->as.func.args, ((VarDef){name, type}));
                } else {
                    expect(next_token(lex), LEX_OPAREN);
                    String name = expect(next_token(lex), LEX_NAME).str;
                    ASTArr type = { 0 };
                    parse(arena, lex, &type, node_id); 
                    da_append(arena, func->as.func.args, ((VarDef){name, type}));
                    expect(next_token(lex), LEX_CPAREN);
                }
            }
            expect(next_token(lex), LEX_CBRAKET);
            parse(arena, lex, &func->as.func.ret, node_id);
            assert(func->as.func.ret.len == 1);
            while (peek_token(lex).kind != LEX_CPAREN) {
                parse(arena, lex, &func->as.func.body, node_id);
            }
        } else if (string_eq(S("let"), t.str)) {
            da_append(arena, *parent, ((AST){AST_VARDEF, {0}, lex->loc, (*node_id)++}));
            AST *vars = &da_last(*parent);
            expect(next_token(lex), LEX_OBRAKET);
            while (peek_token(lex).kind != LEX_CBRAKET) {
                expect(next_token(lex), LEX_OPAREN);
                String name = expect(next_token(lex), LEX_NAME).str;
                ASTArr type = { 0 };
                parse(arena, lex, &type, node_id);
                da_append(arena, vars->as.var.variables, ((Variable){{name, type}, {0}}));
                parse(arena, lex, &da_last(vars->as.var.variables).value, node_id);
                expect(next_token(lex), LEX_CPAREN);
            }
            expect(next_token(lex), LEX_CBRAKET);
            while (peek_token(lex).kind != LEX_CPAREN) {
                parse(arena, lex, &vars->as.var.body, node_id);
            }
        } else if (string_eq(S("def"), t.str)) {
            da_append(arena, *parent, ((AST){AST_DEF, {0}, lex->loc, (*node_id)++}));
            AST *def = &da_last(*parent);
            def->as.def.name = expect(next_token(lex), LEX_NAME).str;
            parse(arena, lex, &def->as.def.body, node_id);
            assert(def->as.def.body.len == 1);
        } else if (string_eq(S("extern"), t.str)) {
            da_append(arena, *parent, ((AST){AST_EXTERN, {0}, lex->loc, (*node_id)++}));
            AST *external = &da_last(*parent);
            external->as.external.name = expect(next_token(lex), LEX_NAME).str;
            parse(arena, lex, &external->as.external.body, node_id);
            assert(external->as.external.body.len == 1);
        } else {
            da_append(arena, *parent, ((AST){AST_CALL, {0}, lex->loc, (*node_id)++}));
            AST *call = &da_last(*parent);
            call->as.call.callee = t.str;
            while (peek_token(lex).kind != LEX_CPAREN) {
                parse(arena, lex, &call->as.call.args, node_id);
            }
        }
        expect(next_token(lex), LEX_CPAREN);
    } break;
    case LEX_BOOL:
        da_append(arena, *parent, ((AST){.kind = AST_BOOL}));
        if (string_eq(t.str, S("true"))) {
            da_last(*parent).as.boolean = true;
        } else if (string_eq(t.str, S("false"))) {
            da_last(*parent).as.boolean = false;
        } else {
            TODO();
        }
        break;
    case LEX_STRING:
        da_append(arena, *parent, ((AST){.kind = AST_STRING}));
        da_last(*parent).as.string = t.str;
        break;
    case LEX_NUMBER:
        da_append(arena, *parent, ((AST){.kind = AST_NUMBER}));
        da_last(*parent).as.number = s_atoi(t.str);
        break;
    case LEX_NAME:
        da_append(arena, *parent, ((AST){.kind = AST_NAME}));
        da_last(*parent).as.name = t.str;
        break;
    case LEX_OBRAKET: {
        da_append(arena, *parent, ((AST){AST_LIST, {0}, lex->loc, (*node_id)++}));
        AST *list = &da_last(*parent);
        while (peek_token(lex).kind != LEX_CBRAKET) {
            parse(arena, lex, &list->as.list, node_id);
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

void free_ast(ASTArr *ast) {
    for (size_t i = 0; i < ast->len; i++) {
        switch (ast->data[i].kind) {
        case AST_CALL:
            free_ast(&ast->data[i].as.call.args);
            break;
        case AST_FUNC:
            for (size_t j = 0; j < ast->data[i].as.func.args.len; j++) {
                free_ast(&ast->data[i].as.func.args.data[j].type);
            }
            free(ast->data[i].as.func.args.data);
            free_ast(&ast->data[i].as.func.body);
            free_ast(&ast->data[i].as.func.ret);
            break;
        case AST_LIST:
            free_ast(&ast->data[i].as.list);
            break;
        case AST_VARDEF:
            for (size_t j = 0; j < ast->data[i].as.var.variables.len; j++) {
                free_ast(&ast->data[i].as.var.variables.data[j].value);
                free_ast(&ast->data[i].as.var.variables.data[j].definition.type);
            }
            for (size_t j = 0; j <  ast->data[i].as.var.variables.len; j++) {
                free_ast(&ast->data[i].as.var.variables.data[i].value);
            }
            free(ast->data[i].as.var.variables.data);
            ast->data[i].as.var.variables.data = NULL;
            free_ast(&ast->data[i].as.var.body);
            break;
        case AST_EXTERN:
            free_ast(&ast->data[i].as.external.body);
            break;
        case AST_DEF:
            free_ast(&ast->data[i].as.def.body);
            break;
        case AST_NAME:
        case AST_NUMBER:
        case AST_STRING:
        case AST_BOOL:
            break;
        }
    }
    free(ast->data);
    ast->data = NULL;
    ast->capacity = 0;
    ast->len = 0;
}
