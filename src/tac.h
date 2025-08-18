#include <stdint.h>
#include <stddef.h>

#include "arena.h"

#ifndef TAC_H
#define TAC_H

typedef struct {
    uint32_t result;
    enum {
        TAC_LOAD_ARG,
        TAC_LOAD_SYM,
        TAC_LOAD_INT,
        TAC_CALL_PUSH,
        TAC_CALL_REG,
        TAC_CALL_SYM,
        TAC_RETURN_VAL,
        TAC_MOV,
        TAC_ADD,
        TAC_SUB,
        TAC_LABEL,
        TAC_GOTO,
        TAC_BIZ,
        TAC_LT,
    } function;
    uint32_t x, y;
} TAC32;

typedef struct {
    TAC32 *data;
    size_t len, capacity;
} TAC32Arr;

uint16_t fold_temporaries(TAC32Arr tac);

#endif // TAC_H
