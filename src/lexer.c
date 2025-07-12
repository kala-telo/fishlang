#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
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

bool eat_char(Lexer *lex) {
    if (lex->length < 1) {
        return false;
    }
    lex->position++;
    lex->length--;
    return true;
}

// XXX: cache tokens maybe
Token peek_token(Lexer *lex) {
    Lexer copy = *lex;
    Token token = next_token(&copy);
    return token;
}

Token next_token(Lexer *lex) {
    if (lex->length < 1) {
        return (Token){
            .kind = LEX_END,
            .str = (String){lex->position, 0},
        };
    }
    while (isspace(*lex->position)) {
        if (!eat_char(lex)) goto fail;
    }
    switch (*lex->position) {
    case '(':
        if (!eat_char(lex)) goto fail;
        return (Token){
            .kind = LEX_OPAREN,
            .str = (String){lex->position-1, 1},
        };
        break;
    case ')':
        if (!eat_char(lex)) goto fail;
        return (Token){
            .kind = LEX_CPAREN,
            .str = (String){lex->position-1, 1},
        };
        break;
    case '[':
        if (!eat_char(lex)) goto fail;
        return (Token){
            .kind = LEX_OBRAKET,
            .str = (String){lex->position-1, 1},
        };
        break;
    case ']':
        if (!eat_char(lex)) goto fail;
        return (Token){
            .kind = LEX_CBRAKET,
            .str = (String){lex->position-1, 1},
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
        };
    }
    default:
        // Parsing word
        if (isalpha(*lex->position) || *lex->position == '+') {
            String word = {
                .string = lex->position,
                .length = 1,
            };
            if (!eat_char(lex)) goto fail;
            while (isalnum(*lex->position)) {
                word.length++;
                if (!eat_char(lex)) goto fail;
            }
            return (Token){
                .kind = LEX_NAME,
                .str = word,
            };
        } else if (isdigit(*lex->position)) {
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
            };
        }
    }

    fprintf(stderr, "Unexpected value '%c' (%d)\n", *lex->position,
            *lex->position);
    TODO();

    fail: {
        fprintf(stderr, "Unexpected end of file\n");
        abort();
    }
    // in reality it is aborted by this point
    // but some compilers, like tcc, show this as warning
    return (Token){ 0 };
}
