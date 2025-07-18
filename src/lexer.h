#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>
#include "string.h"

extern const char *tok_names[];

typedef struct {
    char *position;
    size_t length;
} Lexer;

typedef enum {
    LEX_OPAREN,
    LEX_CPAREN,
    LEX_OBRAKET,
    LEX_CBRAKET,
    LEX_NAME,
    LEX_STRING,
    LEX_NUMBER,
    LEX_END,
} TokenKind;

typedef struct {
    String str;
    TokenKind kind;
} Token;

Token next_token(Lexer *lex);
Token peek_token(Lexer *lex);

#endif // LEXER_H
