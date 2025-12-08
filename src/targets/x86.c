#include <stdlib.h>

#include "../arena.h"
#include "../codegen.h"
#include "../tac.h"
#include "../todo.h"

#define ARRLEN(xs) (sizeof(xs)/sizeof(*(xs)))
#define MIN(x, y) ((x) > (y) ? (y) : (x))

static const char *const preserved_registers[] = {
    "%ebx", "%esi", "%edi","%ebp"
};
static const char *const scratch_registers[] = {
    "%ecx", "%edx", "%eax"
};

static uint8_t used_temp_registers = 0;
static int32_t reg_map[ARRLEN(scratch_registers)] = {0};

typedef enum {
    RW_READ = 1,
    RW_WRITE = 2,
    RW_BOTH = RW_READ|RW_WRITE,
} RWFlag;

static const char *access_register(FILE *output, uint32_t frame, uint32_t reg,
                                   RWFlag rw) {
    if (reg == 0) return NULL;
    int pres_reg_cnt = ARRLEN(preserved_registers);
    if ((int)reg-1 < pres_reg_cnt)
        return preserved_registers[reg-1];
    assert((size_t)used_temp_registers < ARRLEN(scratch_registers));
    int32_t stack_offset = frame-(reg-4)*4;
    const char *rreg = scratch_registers[used_temp_registers];
    if (rw & RW_READ) {
        fprintf(output, "    movl %d(%%esp), %s\n",
                stack_offset, rreg);
    }
    reg_map[used_temp_registers++] = (rw & RW_WRITE) ? stack_offset : 0;
    return rreg;
}

static void free_scratch_registers(FILE *output) {
    for (int i = 0; i < used_temp_registers; i++) {
        if (reg_map[i] == 0) continue;
        fprintf(output, "    movl %s, %d(%%esp)\n",
                scratch_registers[i], reg_map[i]);
    }
    used_temp_registers = 0;
}

void codegen_x86_32(IR ir, FILE *output) {
    // UPDATE: i encountered this very problem, and changing `frame` value seems
    // to fix it, but i still will leave this as warning in case of future problems
    // WARNING: we refer to esp multiple times, but at the same time it's
    // constantly moved by function calls, so i should consider using ebp
    // itstead. this would bring down saved registers to 3 though
    fprintf(output, ".section .text\n");
    for (size_t i = 0; i < ir.functions.len; i++) {
        uint16_t temps_count = ir.functions.data[i].temps_count;
        uint32_t frame = temps_count*4;
        String func_name = ir.symbols.data[ir.functions.data[i].name];
        fprintf(output, ".global %.*s\n", PS(func_name));
        fprintf(output, "%.*s:\n", PS(func_name));
        TAC32Arr func = ir.functions.data[i].code;
        fprintf(output, "    subl $%d, %%esp\n", frame);
        size_t hard_registers = MIN(temps_count, ARRLEN(preserved_registers));
        for (size_t j = 0; j < hard_registers; j++) {
            fprintf(output, "    movl %s, %zu(%%esp)\n", preserved_registers[j], j*4);
        }
        fprintf(output, "\n");
        Arena arena = {0};

        for (size_t j = 0; j < func.len; j++) {
            TAC32 inst = func.data[j];
            const char *r = NULL, *x = NULL, *y = NULL;
            switch (get_arity(inst.function)) {
            case A_BINARY:
                y = access_register(output, frame, inst.y, RW_READ);
                /* fallthrough */
            case A_UNARY:
                x = access_register(output, frame, inst.x, RW_READ);
                /* fallthrough */
            case A_NULLARY:
                // sometimes result is read too, as in TAC_ADD for example
                r = access_register(output, frame, inst.result, RW_BOTH);
                break;
            }
            switch (inst.function) {
            case TAC_LOAD_ARG:
                fprintf(output, "    movl %d(%%esp), %s\n",
                        frame + inst.x * 4 + 4, r);
                break;
            case TAC_LOAD_SYM:
                if (ir.symbols.data[inst.x].string != NULL) {
                    fprintf(output, "    movl $%.*s, %s\n",
                            PS(ir.symbols.data[inst.x]), r);
                } else {
                    fprintf(output, "    movl $data_%d, %s\n", inst.x, r);
                }
                break;
            case TAC_LOAD_INT:
                fprintf(output, "    movl $%d, %s\n", inst.x, r);
                break;
            case TAC_CALL_PUSH:
                fprintf(output, "    pushl %s\n", x);
                frame += 4;
                break;
            case TAC_CALL_PUSH_SYM:
                if (ir.symbols.data[inst.x].string != NULL) {
                    fprintf(output, "    pushl $%.*s\n",
                            PS(ir.symbols.data[inst.x]));
                } else {
                    fprintf(output, "    pushl $data_%d\n", inst.x);
                }
                frame += 4;
                break;
            case TAC_CALL_PUSH_INT:
                fprintf(output, "    pushl $%d\n", inst.x);
                frame += 4;
                break;
            case TAC_CALL_REG:
                fprintf(output, "    call *%s\n", x);
                if (inst.result)
                    fprintf(output, "    movl %%eax, %s\n", r);
                if (inst.y != 0)
                    fprintf(output, "    addl $%d, %%esp\n", inst.y*4);
                frame -= inst.y*4;
                break;
            case TAC_CALL_SYM:
                fprintf(output, "    call %.*s\n", PS(ir.symbols.data[inst.x]));
                if (inst.result != 0) {
                    fprintf(output, "    movl %%eax, %s\n", r);
                }
                if (inst.y != 0)
                    fprintf(output, "    addl $%d, %%esp\n", inst.y*4);
                frame -= inst.y*4;
                break;
            case TAC_RETURN_VAL:
                fprintf(output, "    movl %s, %%eax\n", x);
                break;
            case TAC_RETURN_INT:
                fprintf(output, "    movl $%d, %%eax\n", inst.x);
                break;
            case TAC_MOV:
                fprintf(output, "    movl %s, %s\n", x, r);
                break;
            case TAC_ADD:
                if (x != r)
                    fprintf(output, "    movl %s, %s\n", x, r);
                fprintf(output, "    addl %s, %s\n", y, r);
                break;
            case TAC_ADDI:
                if (x != r)
                    fprintf(output, "    movl %s, %s\n", x, r);
                fprintf(output, "    addl $%d, %s\n", inst.y, r);
                break;
            case TAC_SUB:
                if (x != r)
                    fprintf(output, "    movl %s, %s\n", x, r);
                fprintf(output, "    subl %s, %s\n", y, r);
                break;
            case TAC_SUBI:
                if (x != r)
                    fprintf(output, "    movl %s, %s\n", x, r);
                fprintf(output, "    subl $%d, %s\n", inst.y, r);
                break;
            case TAC_LABEL:
                fprintf(output, "    %.*s.label_%d:\n", PS(func_name), inst.x);
                break;
            case TAC_GOTO:
                fprintf(output, "    jmp %.*s.label_%d\n", PS(func_name), inst.x);
                break;
            case TAC_BIZ:
                fprintf(output, "    cmpl $0, %s\n", x);
                fprintf(output, "    jz %.*s.label_%d\n", PS(func_name), inst.y);
                break;
            case TAC_LT:
                fprintf(output, "    cmpl %s, %s\n", y, x);
                fprintf(output, "    setl %%al\n");
                fprintf(output, "    movzx %%al, %s\n", r);
                break;
            case TAC_LTI:
                fprintf(output, "    cmpl $%d, %s\n", inst.y, x);
                fprintf(output, "    setl %%al\n");
                fprintf(output, "    movzx %%al, %s\n", r);
                break;
            case TAC_EXIT:
                fprintf(output, "    jmp %.*s.epilogue\n", PS(func_name));
                break;
            case TAC_PHI:
                UNREACHABLE();
                break;
            case TAC_NOP:
                break;
            }
            free_scratch_registers(output);
        }
        arena_destroy(&arena);
        fprintf(output, "\n");
        fprintf(output, "    %.*s.epilogue:\n", PS(func_name));
        for (size_t j = 0; j < hard_registers; j++) {
            fprintf(output, "    movl %zu(%%esp), %s\n", j*4, preserved_registers[j]);
        }
        fprintf(output, "    addl $%d, %%esp\n", temps_count*4);
        fprintf(output, "    ret\n");
    }
    fprintf(output, ".section .data\n");
    for (size_t i = 0; i < ir.data.len; i++) {
        StaticVariable var = ir.data.data[i];
        String name = ir.symbols.data[var.name];
        fprintf(output, "data_");
        if (name.string != NULL)
            fprintf(output, "%.*s", PS(name));
        else
            fprintf(output, "%zu", var.name);
        fprintf(output, ": .byte ");
        generate_string(output, var.data, false);
        fprintf(output, "\n");
    }
    fprintf(output, ".section .note.GNU-stack,\"\",@progbits\n");
}
