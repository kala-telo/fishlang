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
        struct {
            String key;
            uintptr_t value;
        } *data;
        size_t len, capacity;
    } *table;
    size_t length;
} SymbolTable;

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
    StaticFunction *current_function;
    SymbolTable global_symbols;
    uint16_t temp_num;
    uint16_t branch_id;
    struct {
        uint32_t *data;
        size_t len, capacity;
    } args_stack;
    SymbolTable variables;
} CodeGenCTX;

void generate_string(FILE *output, String str);

IR codegen(Arena *arena, ASTArr ast, CodeGenCTX *ctx);
void codegen_powerpc(IR ir, FILE *output);
void codegen_debug(IR ir, FILE *output);
void codegen_x86_32(IR ir, FILE *output);

#endif // CODEGEN_H
