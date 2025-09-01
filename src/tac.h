#include <stdint.h>

#include "arena.h"

#ifndef TAC_H
#define TAC_H

typedef enum {
    TAC_LOAD_ARG,
    TAC_LOAD_SYM,
    TAC_LOAD_INT,

    TAC_CALL_PUSH,
    TAC_CALL_PUSH_INT,
    TAC_CALL_PUSH_SYM,

    TAC_CALL_REG,
    TAC_CALL_SYM,

    TAC_RETURN_VAL,
    TAC_RETURN_INT,

    TAC_MOV,

    TAC_ADD,
    TAC_ADDI,

    TAC_SUB,
    TAC_SUBI,

    TAC_LT,
    // TAC_LTI

    TAC_LABEL,
    TAC_GOTO,
    TAC_BIZ,

    TAC_NOP,
} TACOp;

typedef struct {
    uint32_t result;
    TACOp function;
    uint32_t x, y;
} TAC32;

typedef struct {
    TAC32 *data;
    size_t len, capacity;
} TAC32Arr;

uint16_t fold_temporaries(TAC32Arr tac);
bool peephole_optimization(TAC32Arr *tac);
bool constant_propagation(TAC32Arr *tac);
bool remove_unused(TAC32Arr *tac);

#endif // TAC_H
