#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "parser.h"
#include "string.h"
#include "tac.h"
#include "da.h"

#ifndef CODEGEN_H
#define CODEGEN_H

typedef struct {
    // index in symbols array
    size_t name;

    // string can hold any bytes in fact
    String data;
} StaticVariable;

typedef struct {
    // index in symbols array
    size_t name;
    TAC32Arr code;
    uint16_t temps_count;
} StaticFunction;

typedef struct {
    struct {
        // if string is NULL, it's index will be
        // threated as name
        String *data;
        size_t len, capacity;
    } symbols;

    struct {
        StaticFunction *data;
        size_t len, capacity;
    } functions;

    struct {
        StaticVariable *data;
        size_t len, capacity;
    } data;
} IR;

typedef struct {
    IR ir;
    size_t current_function;
    uint16_t temp_num;
    uint16_t branch_id;
    struct {
        uint32_t *data;
        size_t len, capacity;
    } args_stack;
    HashMap variables;
} CodeGenCTX;

void free_ctx(CodeGenCTX *ctx);
IR codegen(ASTArr ast, CodeGenCTX *ctx);
void codegen_powerpc(IR ir, FILE *output);
void codegen_debug(IR ir, FILE *output);
void free_ir(IR *ir);

#endif // CODEGEN_H
