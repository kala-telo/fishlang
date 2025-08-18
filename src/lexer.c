#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "string.h"
#include "todo.h"

const char *tok_names[] = {
    [LEX_OPAREN]  = "`(`",
    [LEX_CPAREN]  = "`)`",
    [LEX_OBRAKET] = "`[`",
    [LEX_CBRAKET] = "`]`",
    [LEX_NAME]    = "<name>",
    [LEX_STRING]  = "<string>",
    [LEX_END]     = "<end of file>",
    [LEX_NUMBER]  = "<number>",
};

static bool eat_char(Lexer *lex) {
    if (lex->length < 1) {
        return false;
    }
    if (*lex->position++ == '\n') {
        lex->loc.line++;
        lex->loc.col = 0;
    } else {
        lex->loc.col++;
    }
    lex->length--;
    return true;
}

static bool valid_name(char c) {
    return isalnum(c) || c == '+' || c == '-' || c == '>' || c == '<' ||
           c == '=' || c == ':' || c == '.';
}

Token peek_token(Lexer *lex) {
    Lexer copy = *lex;
    Token token = next_token(&copy);
    return token;
}

Token next_token(Lexer *lex) {
    while (isspace(*lex->position)) {
        if (!eat_char(lex)) {
            return (Token){
                .kind = LEX_END,
                .str = (String){lex->position, 0},
                .loc = lex->loc
            };
        }
    }
    switch (*lex->position) {
    case '(':
        if (!eat_char(lex)) goto fail;
        return (Token){
            .kind = LEX_OPAREN,
            .str = (String){lex->position-1, 1},
            .loc = lex->loc
        };
        break;
    case ')':
        if (!eat_char(lex)) goto fail;
        return (Token){
            .kind = LEX_CPAREN,
            .str = (String){lex->position-1, 1},
            .loc = lex->loc
        };
        break;
    case '[':
        if (!eat_char(lex)) goto fail;
        return (Token){
            .kind = LEX_OBRAKET,
            .str = (String){lex->position-1, 1},
            .loc = lex->loc
        };
        break;
    case ']':
        if (!eat_char(lex)) goto fail;
        return (Token){
            .kind = LEX_CBRAKET,
            .str = (String){lex->position-1, 1},
            .loc = lex->loc
        };
        break;
    case '"': {
        if (!eat_char(lex)) goto fail;
        String str = {
            .string = lex->position,
            .length = 1,
        };
        while (*lex->position != '"') {
            str.length++;
            if (!eat_char(lex)) goto fail;
        }
        if (!eat_char(lex)) goto fail;
        str.length--;
        return (Token) {
            .kind = LEX_STRING,
            .str = str,
            .loc = lex->loc
        };
    }
    default:
        // Parsing word
        if (isdigit(*lex->position)) {
            String word = {
                .string = lex->position,
                .length = 1,
            };
            if (!eat_char(lex)) goto fail;
            while (isdigit(*lex->position)) {
                word.length++;
                if (!eat_char(lex)) goto fail;
            }
            return (Token){
                .kind = LEX_NUMBER,
                .str = word,
                .loc = lex->loc
            };
        } else if (valid_name(*lex->position)) {
            String word = {
                .string = lex->position,
                .length = 1,
            };
            if (!eat_char(lex)) goto fail;
            while (valid_name(*lex->position)) {
                word.length++;
                if (!eat_char(lex)) goto fail;
            }
            if (string_eq(word, S("true")) || string_eq(word, S("false"))) {
                return (Token){
                    .kind = LEX_BOOL,
                    .str = word,
                    .loc = lex->loc
                };
            }
            return (Token){
                .kind = LEX_NAME,
                .str = word,
                .loc = lex->loc
            };
        }
    }

    fprintf(stderr, "%s:%d:%d Unexpected value '%c' (%d)\n", lex->loc.file,
            lex->loc.line + 1, lex->loc.col, *lex->position, *lex->position);
    TODO();

    fail: {
        fprintf(stderr, "Unexpected end of file\n");
        abort();
    }
    // in reality it is aborted by this point
    // but some compilers, like tcc, show this as warning
    return (Token){ 0 };
}
