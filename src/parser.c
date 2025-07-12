#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "lexer.h"
#include "parser.h"
#include "todo.h"
#include "da.h"

char *ast_names[] = {
    [AST_FUNCDEF] = "funcdef",
    [AST_CALL] = "call",
    [AST_NAME] = "name",
    [AST_STRING] = "string",
};

Token expect(Token token, TokenKind expected) {
    if (token.kind != expected) {
        fprintf(stderr, "Expected %s, got %s\n", tok_names[expected], tok_names[token.kind]);
        abort();
    }
    return token;
}

int64_t s_atoi(String s) {
    int64_t result = 0;
    for (int len = 0; len < s.length; len++) {
        result *= 10;
        result += s.string[len]-'0';
    }
    return result;
}

void parse(Lexer *lex, ASTArr *parent) {
    Token t = next_token(lex);
    switch (t.kind) {
    case LEX_OPAREN: {
        Token t = expect(next_token(lex), LEX_NAME);
        if (string_eq(S("defun"), t.str)) {
            da_append(*parent, ((AST){
                .kind = AST_FUNCDEF,
            }));
            AST *func = &da_last(*parent);
            t = expect(next_token(lex), LEX_NAME);
            func->as.func.name = t.str;
            expect(next_token(lex), LEX_OBRAKET);
            while (peek_token(lex).kind != LEX_CBRAKET) {
                TODO();
            }
            expect(next_token(lex), LEX_CBRAKET);
            while (peek_token(lex).kind != LEX_CPAREN) {
                parse(lex, &func->as.func.body);
            }
       } else {
            da_append(*parent, ((AST){
                .kind = AST_CALL,
            }));
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
        da_append(*parent, ((AST){
            .kind = AST_LIST,
        }));
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

void dump_ast(ASTArr arr, int indent) {
    for (size_t i = 0; i < arr.len; i++) {
        AST ast = arr.data[i];
        switch (ast.kind) {
        case AST_CALL:
            if (indent > 0)
                putchar('\n');
            for (int j = 0; j < indent*4; j++) {
                putchar(' ');
            }
            printf("(%s *%.*s* ", ast_names[ast.kind], ast.as.call.callee.length,
                   ast.as.call.callee.string);
            dump_ast(ast.as.call.args, indent+1);
            putchar(')');
            break;
        case AST_FUNCDEF:
            printf("(%s *%.*s* ", ast_names[ast.kind], ast.as.func.name.length,
                   ast.as.func.name.string);
            dump_ast(ast.as.func.body, indent+1);
            putchar(')');
            break;
        case AST_LIST:
            putchar('[');
            dump_ast(ast.as.func.body, indent+1);
            putchar(']');
            break;
        case AST_NAME:
            printf("%.*s", ast.as.string.length, ast.as.string.string);
            break;
        case AST_NUMBER:
            printf("%"PRId64" ", ast.as.number);
            break;
        case AST_STRING:
            printf("\"%.*s\"", ast.as.string.length, ast.as.string.string);
            break;
        }
    }
    if (indent == 0)
        putchar('\n');
}

void free_ast(ASTArr *ast) {
    for (size_t i = 0; i < ast->len; i++) {
        switch (ast->data[i].kind) {
        case AST_CALL:
            free_ast(&ast->data[i].as.call.args);
            break;
        case AST_FUNCDEF:
            free_ast(&ast->data[i].as.func.body);
            break;
        case AST_LIST:
            free_ast(&ast->data[i].as.list);
            break;
        case AST_NAME:
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

