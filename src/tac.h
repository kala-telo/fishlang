#include <stdint.h>

#include "arena.h"
#include "string.h"

#ifndef TAC_H
#define TAC_H

typedef enum {
    A_NULLARY,
    A_UNARY,
    A_BINARY,
} Arity;

typedef enum {
    TAC_NOP,

    TAC_PHI,

    TAC_LOAD_ARG,
    TAC_LOAD_SYM,
    TAC_LOAD_INT,

    TAC_CALL_PUSH,
    TAC_CALL_PUSH_INT,
    TAC_CALL_PUSH_SYM,

    TAC_CALL_REG,
    TAC_CALL_SYM,

    // exit returns out of the function
    TAC_EXIT,
    // return_val sets return value to register
    TAC_RETURN_VAL,
    // return_val sets return value to immediate
    TAC_RETURN_INT,

    TAC_MOV,

    TAC_ADD,
    TAC_ADDI,

    TAC_SUB,
    TAC_SUBI,

    TAC_LT,
    TAC_LTI,

    TAC_LABEL,
    TAC_GOTO,
    TAC_BIZ,
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

typedef struct {
    // index in symbols array
    size_t name;
    TAC32Arr code;
    uint16_t temps_count;
} StaticFunction;

Arity get_arity(TACOp op);

uint16_t fold_temporaries(TAC32Arr tac);
bool peephole_optimization(TAC32Arr *tac);
bool constant_propagation(TAC32Arr *tac);
bool remove_unused(TAC32Arr *tac);

void try_tail_call_optimization(Arena *arena, StaticFunction *func,
                                String *names);
bool return_lifting(Arena *arena, TAC32Arr *tac);
void remove_phi(Arena *arena, TAC32Arr *tac);
#endif // TAC_H
