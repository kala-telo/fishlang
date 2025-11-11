#include "../codegen.h"
#include "../todo.h"

#define ALIGN(value, size) (((value)+(size)-1)&~((size)-1))

void codegen_mips(IR ir, FILE *output) {
    // TODO: handle spill, read powerpc function
    const int call_base = 4;
    const int gpr_base = 16;
    fprintf(output, ".set noreorder\n");
    fprintf(output, ".set nomacro\n");
    fprintf(output, ".option pic0\n");
    fprintf(output, ".text\n");
    fprintf(output, ".align 2\n");
    for (size_t i = 0; i < ir.functions.len; i++) {
        TAC32Arr func = ir.functions.data[i].code;
        // 7 is the amount of saved registers on mips
        if (ir.functions.data[i].temps_count > 7)
            TODO();
        String func_name = ir.symbols.data[ir.functions.data[i].name];
        size_t temps_count = ir.functions.data[i].temps_count;
        // +1 for return address
        int frame = ALIGN(4*temps_count+1, 32);
        fprintf(output, ".global %.*s\n", PS(func_name));
        fprintf(output, "%.*s:\n", PS(func_name));
        fprintf(output, "    addiu $sp, $sp, -%d\n", frame);
        fprintf(output, "    sw $31, 28($sp)\n");
        for (size_t j = 0; j < temps_count; j++) {
            fprintf(output, "    sw $%zu, %zu($sp)\n", j+gpr_base, (frame-(j+2)*4));
        }
        fprintf(output, "\n");
        int call_count = 0;
        for (size_t j = 0; j < func.len; j++) {
            TAC32 inst = func.data[j];
            uint32_t r = inst.result + gpr_base - 1,
                     x = inst.x      + gpr_base - 1,
                     y = inst.y      + gpr_base - 1;
            switch (inst.function) {
            case TAC_LOAD_ARG:
                assert(inst.x < 5);
                fprintf(output, "    move $%d, $%d\n", r, 4+inst.x);
                break;
            case TAC_LOAD_SYM:
                if (ir.symbols.data[inst.x].string != NULL) {
                    fprintf(output, "    lui $%d, %%hi(%.*s)\n", r,
                            PS(ir.symbols.data[inst.x]));
                    fprintf(output, "    addiu $%d, %%lo(%.*s)\n", r,
                            PS(ir.symbols.data[inst.x]));
                } else {
                    fprintf(output, "    lui $%d, %%hi($data_%d)\n", r, inst.x);
                    fprintf(output, "    addiu $%d, %%lo($data_%d)\n", r,
                            inst.x);
                }
                break;
            case TAC_LOAD_INT:
                if (inst.x < 65536) {
                    fprintf(output, "    li $%d, %d\n", r, inst.x);
                } else {
                    fprintf(output, "    lui $%d, %d\n", r, inst.x&0xffff);
                    fprintf(output, "    addiu $%d, %d\n", r, inst.x>>16);
                }
                break;
            case TAC_CALL_PUSH:
                fprintf(output, "    move $%d, $%d\n", call_base + (call_count++), x);
                break;
            case TAC_CALL_PUSH_INT:
                if (inst.x < 65536) {
                    fprintf(output, "    li $%d, %d\n", call_base + call_count, inst.x);
                } else {
                    fprintf(output, "    lui $%d, %d\n", call_base + call_count, inst.x&0xffff);
                    fprintf(output, "    addiu $%d, %d\n",
                            call_base + call_count, inst.x>>16);
                }
                call_count++;
                break;
            case TAC_CALL_PUSH_SYM:
                if (ir.symbols.data[inst.x].string != NULL) {
                    fprintf(output, "    lui $%d, %%hi(%.*s)\n",
                            call_count + call_base,
                            PS(ir.symbols.data[inst.x]));
                    fprintf(output, "    addiu $%d, %%lo(%.*s)\n", call_count + call_base, PS(ir.symbols.data[inst.x]));
                } else {
                    fprintf(output, "    lui $%d, %%hi($data_%d)\n",
                            call_count + call_base, inst.x);
                    fprintf(output, "    addiu $%d, %%lo($data_%d)\n",
                            call_count + call_base, inst.x);
                }
                call_count++;
                break;
            case TAC_CALL_REG:
                fprintf(output, "    jalr $%d\n", x);
                fprintf(output, "    nop\n");
                if (inst.result)
                    fprintf(output, "    move $%d, $2\n", r);
                call_count = 0;
                break;
            case TAC_CALL_SYM:
                fprintf(output, "    jal %.*s\n", PS(ir.symbols.data[inst.x]));
                fprintf(output, "    nop\n");
                if (inst.result)
                    fprintf(output, "    move $%d, $2\n", r);
                call_count = 0;
                break;
            case TAC_RETURN_VAL:
                fprintf(output, "    move $2, $%d\n", x);
                break;
            case TAC_RETURN_INT:
                if (inst.x < 65536) {
                    fprintf(output, "    li $2, %d\n", inst.x);
                } else {
                    fprintf(output, "    lui $2, %d\n", inst.x);
                    fprintf(output, "    addiu $2, %d\n", inst.x);
                }
                break;
            case TAC_MOV:
                fprintf(output, "    move $%d, $%d\n", r, x);
                break;
            case TAC_ADD:
                fprintf(output, "    add $%d, $%d, $%d\n", r, x, y);
                break;
            case TAC_ADDI:
                fprintf(output, "    addi $%d, $%d, %d\n", r, x, inst.y);
                break;
            case TAC_SUB:
                TODO();
                break;
            case TAC_SUBI:
                fprintf(output, "    addi $%d, $%d, -%d\n", r, x, inst.y);
                break;
            case TAC_LABEL:
                fprintf(output, "    %.*s.label%d:\n", PS(func_name), inst.x);
                break;
            case TAC_GOTO:
                fprintf(output, "    j %.*s.label%d\n", PS(func_name), inst.x);
                fprintf(output, "    nop\n");
                break;
            case TAC_BIZ:
                fprintf(output, "    beqz $%d, %.*s.label%d\n", x, PS(func_name), inst.y);
                fprintf(output, "    nop\n");
                break;
            case TAC_LT:
                fprintf(output, "    slt $%d, $%d, $%d\n", r, x, y);
                break;
            case TAC_LTI:
                fprintf(output, "    slti $%d, $%d, %d\n", r, x, inst.y);
                break;
            case TAC_EXIT:
                fprintf(output, "    j %.*s.epilogue\n", PS(func_name));
                fprintf(output, "    nop\n");
                break;
            case TAC_PHI:
                UNREACHABLE();
                break;
            case TAC_NOP:
                TODO();
                break;
            }
        }
        fprintf(output, "\n");
        fprintf(output, "    %.*s.epilogue:\n", PS(func_name));
        for (size_t j = 0; j < temps_count; j++) {
            fprintf(output, "    lw $%zu, %zu($sp)\n", j+gpr_base, (frame-(j+2)*4));
        }
        fprintf(output, "    lw $31, 28($sp)\n");
        fprintf(output, "    jr $31\n");
        fprintf(output, "    addiu $sp, $sp, %d\n", frame);
    }
    fprintf(output, ".data\n");
    fprintf(output, ".align 2\n");
    for (size_t i = 0; i < ir.data.len; i++) {
        StaticVariable var = ir.data.data[i];
        String name = ir.symbols.data[var.name];
        fprintf(output, "$data_");
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
