#include "parser.h"
#include "da.h"
#include "lexer.h"
#include "todo.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

Token expect(Token token, TokenKind expected) {
    if (token.kind != expected) {
        fprintf(stderr, "Expected %s, got %s\n", tok_names[expected],
                tok_names[token.kind]);
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

void parse(Lexer *lex, ASTArr *parent) {
    Token t = next_token(lex);
    switch (t.kind) {
    case LEX_OPAREN: {
        Token t = expect(next_token(lex), LEX_NAME);
        if (string_eq(S("defun"), t.str)) {
            da_append(*parent, ((AST){AST_FUNCDEF, {0}}));
            AST *func = &da_last(*parent);
            t = expect(next_token(lex), LEX_NAME);
            func->as.func.name = t.str;
            expect(next_token(lex), LEX_OBRAKET);
            while (peek_token(lex).kind != LEX_CBRAKET) {
                t = expect(next_token(lex), LEX_NAME);
                da_append(func->as.func.args, t.str);
            }
            expect(next_token(lex), LEX_CBRAKET);
            while (peek_token(lex).kind != LEX_CPAREN) {
                parse(lex, &func->as.func.body);
            }
        } else if (string_eq(S("let"), t.str)) {
            da_append(*parent, ((AST){AST_VARDEF, {0}}));
            AST *vars = &da_last(*parent);
            expect(next_token(lex), LEX_OBRAKET);
            while (peek_token(lex).kind != LEX_CBRAKET) {
                expect(next_token(lex), LEX_OPAREN);
                t = expect(next_token(lex), LEX_NAME);
                da_append(vars->as.var.variables, ((Variable){t.str, {0}}));
                parse(lex, &da_last(vars->as.var.variables).value);
                expect(next_token(lex), LEX_CPAREN);
            }
            expect(next_token(lex), LEX_CBRAKET);
            while (peek_token(lex).kind != LEX_CPAREN) {
                parse(lex, &vars->as.var.body);
            }
        } else if (string_eq(S("extern"), t.str)) {
            da_append(*parent, ((AST){AST_EXTERN, {0}}));
            da_last(*parent).as.exteral = expect(next_token(lex), LEX_NAME).str;
        } else {
            da_append(*parent, ((AST){AST_CALL, {0}}));
            AST *call = &da_last(*parent);
            call->as.call.callee = t.str;
            while (peek_token(lex).kind != LEX_CPAREN) {
                parse(lex, &call->as.call.args);
            }
        }
        expect(next_token(lex), LEX_CPAREN);
    } break;
    case LEX_STRING:
        da_append(*parent, ((AST){.kind = AST_STRING}));
        da_last(*parent).as.string = t.str;
        break;
    case LEX_NUMBER:
        da_append(*parent, ((AST){.kind = AST_NUMBER}));
        da_last(*parent).as.number = s_atoi(t.str);
        break;
    case LEX_NAME:
        da_append(*parent, ((AST){.kind = AST_NAME}));
        da_last(*parent).as.name = t.str;
        break;
    case LEX_OBRAKET: {
        da_append(*parent, ((AST){AST_LIST, {0}}));
        AST *list = &da_last(*parent);
        while (peek_token(lex).kind != LEX_CBRAKET) {
            parse(lex, &list->as.list);
        }
        expect(next_token(lex), LEX_CBRAKET);
    } break;
    default:
        printf("Unexpected token: %s\n", tok_names[t.kind]);
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
        case AST_FUNCDEF:
            free_ast(&ast->data[i].as.func.body);
            free(ast->data[i].as.func.args.data);
            break;
        case AST_LIST:
            free_ast(&ast->data[i].as.list);
            break;
        case AST_VARDEF:
            for (size_t j = 0; j < ast->data[i].as.var.variables.len; j++) {
                free_ast(&ast->data[i].as.var.variables.data[j].value);
            }
            free(ast->data[i].as.var.variables.data);
            ast->data[i].as.var.variables.data = NULL;
            free_ast(&ast->data[i].as.var.body);
            break;
        case AST_NAME:
        case AST_EXTERN:
        case AST_NUMBER:
        case AST_STRING:
            break;
        }
    }
    free(ast->data);
    ast->data = NULL;
    ast->capacity = 0;
    ast->len = 0;
}
