#include "parser.h"
#include "da.h"
#include "lexer.h"
#include "string.h"
#include "todo.h"
#include <stdio.h>

void parse_expr(Arena *arena, Lexer *lex, ASTArr *arr, AST* parent, size_t *node_id);

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

TypeAST parse_type(Arena *arena, Lexer *lex) {
    if (peek_token(lex).kind == LEX_OPAREN) {
        expect(next_token(lex), LEX_OPAREN);
        if (peek_token(lex).kind == LEX_CPAREN) {
            expect(next_token(lex), LEX_CPAREN);
            return (TypeAST){TYPE_UNIT, {0}};
        }
        expect(next_token(lex), LEX_FN);
        TypeAST t = {0};
        t.type = TYPE_FN;
        while (peek_token(lex).kind != LEX_CPAREN) {
            da_append(arena, t.as.fn, parse_type(arena, lex));
        }
        expect(next_token(lex), LEX_CPAREN);
        return t;
    }
    String type_name = expect(next_token(lex), LEX_NAME).str;
    if (string_eq(type_name, S("i32"))) {
        return (TypeAST){TYPE_I32, {0}};
    } else if (string_eq(type_name, S("cstr"))) {
        return (TypeAST){TYPE_CSTR, {0}};
    } else if (string_eq(type_name, S("..."))) {
        return (TypeAST){TYPE_VARIADIC, {0}};
    } else {
        fprintf(stderr, "Unrecognized type: %.*s\n", PS(type_name));
        TODO();
    }
    return (TypeAST){0};
}

ASTKind is_token_binop(TokenKind k) {
    switch (k) {
    case LEX_OPAREN: case LEX_CPAREN: case LEX_OBRAKET: case LEX_CBRAKET:
    case LEX_NAME: case LEX_STRING: case LEX_NUMBER: case LEX_BOOL: case LEX_LET:
    case LEX_EQUALS: case LEX_EXTERN: case LEX_ARROW: case LEX_COLON:
    case LEX_SEMICOLON: case LEX_FN: case LEX_END: case LEX_IF: case LEX_THEN:
    case LEX_ELSE:
        return false;
    case LEX_LT: case LEX_GT: case LEX_PLUS: case LEX_MINUS:
        return true;
    }
}

void check_binop(Arena *arena, Lexer *lex, ASTArr *arr, AST* parent, size_t *node_id) {
    Token t = peek_token(lex);
    if (is_token_binop(t.kind)) {
        assert(arr->len != 0);
        next_token(lex);
        AST rhs = da_last(*arr);
        da_last(*arr) = (AST){AST_APPLY, {0}, lex->loc, (*node_id)++, parent};
        AST *binop = &da_last(*arr);
        binop->as.apply.name = t.str;
        da_append(arena, binop->as.apply.args, rhs);
        parse_expr(arena, lex, &binop->as.apply.args, binop, node_id);
        assert(binop->as.apply.args.len == 2);
    }
}

AST *parse_let_pair(Arena *arena, Lexer *lex, ASTArr *arr, AST* parent, size_t *node_id) {
    Token t = expect(next_token(lex), LEX_NAME);
    expect(next_token(lex), LEX_EQUALS);
    da_append(arena, *arr, ((AST){AST_LET, {0}, lex->loc, (*node_id)++, parent}));
    AST *let = &da_last(*arr);
    let->as.let.name = t.str;
    let->parent = parent;
    parse_expr(arena, lex, &let->as.let.rhs, let, node_id);
}

// consider inlining
void parse_name(Arena *arena, Lexer *lex, ASTArr *arr, AST* parent, size_t *node_id) {
    da_append(arena, *arr, ((AST){AST_APPLY, {0}, lex->loc, (*node_id)++, parent}));
    da_last(*arr).as.apply.name = expect(next_token(lex), LEX_NAME).str;
    check_binop(arena, lex, arr, parent, node_id);
}

void parse_expr(Arena *arena, Lexer *lex, ASTArr *arr, AST* parent, size_t *node_id) {
    Token t = peek_token(lex);
    switch (t.kind) {
    case LEX_NAME: {
        da_append(arena, *arr, ((AST){AST_APPLY, {0}, lex->loc, (*node_id)++, parent}));
        AST *application = &da_last(*arr);
        bool loop = true;
        // NOTE: i have a strong feeling that it either wants to know arity of a
        // function at parsing time, or to have some sort of expression ending
        // delimeter. otherwise it's hard to know when expression ends.
        // currently we implicitly threat some tokens as ending ones
        application->as.apply.name = expect(next_token(lex), LEX_NAME).str;
        while (loop) {
            Token t = peek_token(lex);
            switch (t.kind) {
            case LEX_BOOL: case LEX_STRING: case LEX_NUMBER: case LEX_OPAREN:
                parse_expr(arena, lex, &application->as.apply.args, application, node_id);
                break;
            case LEX_NAME:
                parse_name(arena, lex, &application->as.apply.args, application, node_id);
                break;
            case LEX_SEMICOLON: case LEX_IF: case LEX_THEN: case LEX_ELSE: case LEX_LET:
            case LEX_PLUS: case LEX_CPAREN: case LEX_CBRAKET: case LEX_LT: case LEX_GT:
            case LEX_MINUS: case LEX_EQUALS: case LEX_END:
                loop = false;
                break;
            default:
                printf("peek_token(lex).kind = %s\n", tok_names[t.kind]);
                TODO();
            }
        }
        check_binop(arena, lex, arr, parent, node_id);
    } break;
    case LEX_IF: {
        expect(next_token(lex), LEX_IF);
        da_append(arena, *arr, ((AST){AST_IF, {0}, lex->loc, (*node_id)++, parent}));
        AST *iff = &da_last(*arr);
        parse(arena, lex, &iff->as.iff.cond, iff, node_id);
        expect(next_token(lex), LEX_THEN);
        parse_expr(arena, lex, &iff->as.iff.then, iff, node_id);
        expect(next_token(lex), LEX_ELSE);
        parse_expr(arena, lex, &iff->as.iff.elsee, iff, node_id);
    } break;
    case LEX_BOOL: {
        String b = expect(next_token(lex), LEX_BOOL).str;
        da_append(arena, *arr, ((AST){AST_BOOL, {0}, lex->loc, (*node_id)++, parent}));
        if (string_eq(b, S("true"))) {
            da_last(*arr).as.boolean = true;
        } else if (string_eq(b, S("false"))) {
            da_last(*arr).as.boolean = false;
        } else {
            UNREACHABLE();
        }
    } break;
    case LEX_STRING: {
        expect(next_token(lex), LEX_STRING);
        AST str = {0};
        str.kind = AST_STRING;
        str.as.string = t.str;
        str.id = (*node_id)++;
        da_append(arena, *arr, str);
    } break;
    case LEX_NUMBER: {
        Token t = expect(next_token(lex), LEX_NUMBER);
        AST num = {0};
        num.kind = AST_NUMBER;
        num.as.number = s_atoi(t.str);
        num.id = (*node_id)++;
        da_append(arena, *arr, num);
        check_binop(arena, lex, arr, parent, node_id);
    } break;
    case LEX_OPAREN: {
        expect(next_token(lex), LEX_OPAREN);
        if (peek_token(lex).kind == LEX_CPAREN) {
            expect(next_token(lex), LEX_CPAREN);
            da_append(arena, *arr, ((AST){AST_UNIT, {0}, lex->loc, (*node_id)++, parent}));
            break;
        }
        da_append(arena, *arr, ((AST){AST_BLOCK, {0}, lex->loc, (*node_id)++, parent}));
        parse(arena, lex, &da_last(*arr).as.block, &da_last(*arr), node_id);
        expect(next_token(lex), LEX_CPAREN);
        check_binop(arena, lex, arr, parent, node_id);
    } break;
    case LEX_LET: {
        expect(next_token(lex), LEX_LET);
        if (peek_token(lex).kind == LEX_OBRAKET) {
            expect(next_token(lex), LEX_OBRAKET);
            while (true) {
                parse_let_pair(arena, lex, arr, parent, node_id);
                parent = &da_last(*arr);
                assert(parent->kind == AST_LET);
                arr = &parent->as.let.body;
                if (peek_token(lex).kind == LEX_CBRAKET) {
                    break;
                }
                expect(next_token(lex), LEX_SEMICOLON);
            }
            expect(next_token(lex), LEX_CBRAKET);
        } else {
            parse_let_pair(arena, lex, arr, parent, node_id);
            parent = &da_last(*arr);
            arr = &parent->as.let.body;
        }
        assert(parent->kind == AST_LET);
        // threat everything past `let` as ocaml's
        // ```
        // let x = 123 in
        // ... (* has x defined in it's context *)
        // ```
        parse_expr(arena, lex, arr, parent, node_id);
    } break;
    case LEX_FN: {
        expect(next_token(lex), LEX_FN);
        da_append(arena, *arr, ((AST){AST_FN, {0}, lex->loc, (*node_id)++, parent}));
        AST *fn = &da_last(*arr);
        if (peek_token(lex).kind == LEX_ARROW) {
            TODO(); // error message about empty function signature
        }
        while (peek_token(lex).kind != LEX_ARROW) {
            String name = S("");
            TypeAST type;
            // TODO: make it impossible to name return type
            if (peek_token_n(lex, 2).kind == LEX_COLON) {
                name = expect(next_token(lex), LEX_NAME).str;
                expect(next_token(lex), LEX_COLON);
            }
            type = parse_type(arena, lex);
            da_append(arena, fn->as.fn.args_names, name);
            da_append(arena, fn->as.fn.args_types, type);
        }
        // for return type
        fn->as.fn.args_names.len--;
        expect(next_token(lex), LEX_ARROW);
        if (peek_token(lex).kind == LEX_EXTERN) {
            expect(next_token(lex), LEX_EXTERN);
        } else {
            parse(arena, lex, &fn->as.fn.body, fn, node_id);
        }
    } break;
    case LEX_SEMICOLON:
        break;
    case LEX_END:
        break;
    default:
        fprintf(stderr, "%s:%d:%d Unexpected token: %s\n", t.loc.file, t.loc.line + 1,
               t.loc.col + 1, tok_names[t.kind]);
        TODO();
        break;
    }
}

void parse(Arena *arena, Lexer *lex, ASTArr *arr, AST* parent, size_t *node_id) {
    Token t = peek_token(lex);
    switch (t.kind) {
    case LEX_SEMICOLON:
        break;
    case LEX_NAME: case LEX_LET: case LEX_OPAREN: case LEX_BOOL: case LEX_NUMBER:
    case LEX_STRING: case LEX_IF: case LEX_FN:
        parse_expr(arena, lex, arr, parent, node_id);
        break;
    default:
        fprintf(stderr, "%s:%d:%d Unexpected token: %s\n", t.loc.file, t.loc.line + 1,
               t.loc.col + 1, tok_names[t.kind]);
        TODO();
        break;
    }
    if (peek_token(lex).kind == LEX_SEMICOLON) {
        expect(next_token(lex), LEX_SEMICOLON);
        parse(arena, lex, arr, parent, node_id);
    }
}
