#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "string.h"
#include "todo.h"

const char *tok_names[] = {
    [LEX_OPAREN]    = "`(`",
    [LEX_CPAREN]    = "`)`",
    [LEX_OBRAKET]   = "`[`",
    [LEX_CBRAKET]   = "`]`",
    [LEX_LET]       = "`let`",
    [LEX_LT]        = "`<`",
    [LEX_GT]        = "`>`",
    [LEX_EXTERN]    = "`extern`",
    [LEX_FN]        = "`fn`",
    [LEX_IF]        = "`if`",
    [LEX_THEN]      = "`then`",
    [LEX_ELSE]      = "`else`",
    [LEX_ARROW]     = "`=>`",
    [LEX_COLON]     = "`:`",
    [LEX_SEMICOLON] = "`;`",
    [LEX_PLUS]      = "`+`",
    [LEX_NAME]      = "<name>",
    [LEX_STRING]    = "<string>",
    [LEX_NUMBER]    = "<number>",
    [LEX_BOOL]      = "<bool>",
    [LEX_END]       = "<end of file>",
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
    return isalnum(c) || c == '.';
}

Token peek_token(Lexer *lex) {
    Lexer copy = *lex;
    Token token = next_token(&copy);
    return token;
}

Token peek_token_n(Lexer *lex, size_t n) {
    Lexer copy = *lex;
    Token token = {0};
    while (n--) {
        token = next_token(&copy);
    }
    return token;
}

Token next_token(Lexer *lex) {
    {
        Token eof = {
            .kind = LEX_END,
            .str = (String){lex->position, 0},
            .loc = lex->loc
        };
        while (isspace(*lex->position)) {
            if (!eat_char(lex))
                return eof;
        }
        if (lex->length == 0)
            return eof;
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
    case ';':
        if (!eat_char(lex)) goto fail;
        return (Token){
            .kind = LEX_SEMICOLON,
            .str = (String){lex->position-1, 1},
            .loc = lex->loc
        };
        break;
    case ':':
        if (!eat_char(lex)) goto fail;
        return (Token){
            .kind = LEX_COLON,
            .str = (String){lex->position-1, 1},
            .loc = lex->loc
        };
        break;
    case '<':
        if (!eat_char(lex)) goto fail;
        return (Token){
            .kind = LEX_LT,
            .str = (String){lex->position-1, 1},
            .loc = lex->loc
        };
        break;
    case '+':
        if (!eat_char(lex)) goto fail;
        return (Token){
            .kind = LEX_PLUS,
            .str = (String){lex->position-1, 1},
            .loc = lex->loc
        };
        break;
    case '>':
        if (!eat_char(lex)) goto fail;
        return (Token){
            .kind = LEX_GT,
            .str = (String){lex->position-1, 1},
            .loc = lex->loc
        };
        break;
    case '=':
        if (!eat_char(lex)) goto fail;
        if (*lex->position == '>') {
            if (!eat_char(lex)) goto fail;
            return (Token){
                .kind = LEX_ARROW,
                .str = (String){lex->position-2, 2},
                .loc = lex->loc
            };
        }
        return (Token){
            .kind = LEX_EQUALS,
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
            if (string_eq(word, S("let"))) {
                return (Token){
                    .kind = LEX_LET,
                    .str = word,
                    .loc = lex->loc
                };
            }
            if (string_eq(word, S("fn"))) {
                return (Token){
                    .kind = LEX_FN,
                    .str = word,
                    .loc = lex->loc
                };
            }
            if (string_eq(word, S("extern"))) {
                return (Token){
                    .kind = LEX_EXTERN,
                    .str = word,
                    .loc = lex->loc
                };
            }
            if (string_eq(word, S("if"))) {
                return (Token){
                    .kind = LEX_IF,
                    .str = word,
                    .loc = lex->loc
                };
            }
            if (string_eq(word, S("then"))) {
                return (Token){
                    .kind = LEX_THEN,
                    .str = word,
                    .loc = lex->loc
                };
            }
            if (string_eq(word, S("else"))) {
                return (Token){
                    .kind = LEX_ELSE,
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
