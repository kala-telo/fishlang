#include <stdlib.h>

#include "../arena.h"
#include "../codegen.h"
#include "../tac.h"
#include "../todo.h"

typedef struct {
    // instruction index
    size_t *data;
    size_t len, capacity;
} X86Stack;

static const char *const reg_names[] = {
    "%ebx", "%esi", "%edi", "%ebp",
};

#define ARRLEN(xs) (sizeof(xs)/sizeof(*(xs)))
#define MIN(x, y) ((x) > (y) ? (y) : (x))

void codegen_x86_32(IR ir, FILE *output) {
    // TODO: handle spill, see the powerpc target
    // FIXME: we refer to esp multiple times, but at the same time it's
    // constantly moved by function calls, so i should consider using ebp
    // itstead. this would bring down saved registers to 3 though
    fprintf(output, ".section .text\n");
    for (size_t i = 0; i < ir.functions.len; i++) {
        uint16_t temps_count = ir.functions.data[i].temps_count;
        assert(temps_count <= ARRLEN(reg_names));
        String func_name = ir.symbols.data[ir.functions.data[i].name];
        fprintf(output, ".global %.*s\n", PS(func_name));
        fprintf(output, "%.*s:\n", PS(func_name));
        TAC32Arr func = ir.functions.data[i].code;
        fprintf(output, "    subl $%d, %%esp\n", temps_count*4);
        for (size_t j = 0; j < MIN(temps_count, ARRLEN(reg_names)); j++) {
            fprintf(output, "    movl %s, %zu(%%esp)\n", reg_names[j], j*4);
        }
        fprintf(output, "\n");
        Arena arena = {0};

        int call_count = 0;

        for (size_t j = 0; j < func.len; j++) {
            TAC32 inst = func.data[j];
            uint32_t r = inst.result - 1,
                     x = inst.x      - 1,
                     y = inst.y      - 1;
            // we restore esp several times while we could use ebp, related to
            // fixme up in the code
            switch (inst.function) {
            case TAC_LOAD_ARG:
                fprintf(output, "    movl %d(%%esp), %s\n",
                        inst.x * 4 + 4 + temps_count * 4,
                        reg_names[r]);
                break;
            case TAC_LOAD_SYM:
                if (ir.symbols.data[inst.x].string != NULL) {
                    fprintf(output, "    movl $%.*s, %s\n",
                            PS(ir.symbols.data[inst.x]), reg_names[r]);
                } else {
                    fprintf(output, "    movl $data_%d, %s\n", inst.x, reg_names[r]);
                }
                break;
            case TAC_LOAD_INT:
                fprintf(output, "    movl $%d, %s\n", inst.x, reg_names[r]);
                break;
            case TAC_CALL_PUSH:
                call_count++;
                fprintf(output, "    pushl %s\n", reg_names[x]);
                break;
            case TAC_CALL_PUSH_SYM:
                call_count++;
                if (ir.symbols.data[inst.x].string != NULL) {
                    fprintf(output, "    pushl $%.*s\n",
                            PS(ir.symbols.data[inst.x]));
                } else {
                    fprintf(output, "    pushl $data_%d\n", inst.x);
                }
                break;
            case TAC_CALL_PUSH_INT:
                call_count++;
                fprintf(output, "    pushl $%d\n", inst.x);
                break;
            case TAC_CALL_REG:
                fprintf(output, "    call *%s\n", reg_names[x]);
                if (inst.result)
                    fprintf(output, "    mov %%eax, %s\n", reg_names[r]);
                if (call_count != 0)
                    fprintf(output, "    addl $%d, %%esp\n", call_count*4);
                call_count = 0;
                break;
            case TAC_CALL_SYM:
                fprintf(output, "    call %.*s\n", PS(ir.symbols.data[inst.x]));
                if (inst.result != 0) {
                    fprintf(output, "    movl %%eax, %s\n", reg_names[r]);
                }
                if (call_count != 0)
                    fprintf(output, "    addl $%d, %%esp\n", call_count*4);
                call_count = 0;
                break;
            case TAC_RETURN_VAL:
                fprintf(output, "    movl %s, %%eax\n", reg_names[x]);
                break;
            case TAC_RETURN_INT:
                fprintf(output, "    movl $%d, %%eax\n", inst.x);
                break;
            case TAC_MOV:
                fprintf(output, "    movl %s, %s\n", reg_names[x], reg_names[r]);
                break;
            case TAC_ADD:
                if (x != r)
                    fprintf(output, "    mov %s, %s\n", reg_names[x], reg_names[r]);
                fprintf(output, "    addl %s, %s\n", reg_names[y], reg_names[r]);
                break;
            case TAC_ADDI:
                fprintf(output, "    addl $%d, %s\n", inst.y, reg_names[r]);
                break;
            case TAC_SUB:
                TODO();
                break;
            case TAC_SUBI:
                if (x != r)
                    fprintf(output, "    mov %s, %s\n", reg_names[x], reg_names[r]);
                fprintf(output, "    subl $%d, %s\n", inst.y, reg_names[r]);
                break;
            case TAC_LABEL:
                fprintf(output, "    %.*s.label_%d:\n", PS(func_name), inst.x);
                break;
            case TAC_GOTO:
                fprintf(output, "    jmp %.*s.label_%d\n", PS(func_name), inst.x);
                break;
            case TAC_BIZ:
                fprintf(output, "    cmpl $0, %s\n", reg_names[x]);
                fprintf(output, "    jz %.*s.label_%d\n", PS(func_name), inst.y);
                break;
            case TAC_LT:
                fprintf(output, "    cmpl %s, %s\n", reg_names[y], reg_names[x]);
                fprintf(output, "    setl %%al\n");
                fprintf(output, "    movzx %%al, %s\n", reg_names[r]);
                break;
            case TAC_LTI:
                fprintf(output, "    cmpl $%d, %s\n", inst.y, reg_names[x]);
                fprintf(output, "    setl %%al\n");
                fprintf(output, "    movzx %%al, %s\n", reg_names[r]);
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
        }
        arena_destroy(&arena);
        fprintf(output, "\n");
        fprintf(output, "    %.*s.epilogue:\n", PS(func_name));
        for (size_t j = 0; j < temps_count; j++) {
            fprintf(output, "    mov %zu(%%esp), %s\n", j*4, reg_names[j]);
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
