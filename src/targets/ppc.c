#include "../codegen.h"
#include "../tac.h"
#include "../todo.h"

void codegen_powerpc(IR ir, FILE *output) {
    // TODO: handle spill:
    // fold_temporaries minimizes their usage by reusing,
    // but it threats "register space" as nearly infinite amount of registers
    // on real machine we would use the first N avalible as real registers,
    // and other as "in RAM registers", by loading their value dynamically from
    // stack frame
    const int call_base = 3;
    const int gpr_base = 14;
    for (size_t i = 0; i < ir.functions.len; i++) {
        TAC32Arr func = ir.functions.data[i].code;
        // 19 is the amount of nonvolatile registers on powerpc
        if (ir.functions.data[i].temps_count > 18)
            TODO();
        fprintf(output, ".global %.*s\n",
                PS(ir.symbols.data[ir.functions.data[i].name]));
        fprintf(output, "%.*s:\n",
                PS(ir.symbols.data[ir.functions.data[i].name]));
        int call_count = 0;
        int stack_frame = 16 + 4 * ir.functions.data[i].temps_count;
        if (stack_frame % 16 != 0)
            stack_frame += 16 - stack_frame % 16;
        fprintf(output, "    stwu 1,-%d(1)\n", stack_frame);
        fprintf(output, "    mflr 0\n");
        fprintf(output, "    stw 0, %d(1)\n", stack_frame + 4);
        for (int j = 0; j < ir.functions.data[i].temps_count; j++) {
            fprintf(output, "    stw %d, %d(1)\n", j + gpr_base,
                    stack_frame - j * 4);
        }
        fprintf(output, "\n");
        for (size_t j = 0; j < func.len; j++) {
            TAC32 inst = func.data[j];
            uint32_t r = inst.result + gpr_base - 1,
                     x = inst.x      + gpr_base - 1,
                     y = inst.y      + gpr_base - 1;
            switch (inst.function) {
            case TAC_CALL_REG:
                fprintf(output, "    mtctr %d\n", x);
                fprintf(output, "    bctrl\n");
                if (inst.result)
                    fprintf(output, "    mr %d, 3\n", r);
                break;
            case TAC_CALL_SYM:
                fprintf(output, "    bl %.*s\n", PS(ir.symbols.data[inst.x]));
                if (inst.result)
                    fprintf(output, "    mr %d, 3\n", r);
                call_count = 0;
                break;
            case TAC_CALL_PUSH:
                fprintf(output, "    mr %d, %d\n", (call_count++) + call_base,
                        x);
                assert(call_count < 8);
                break;
            case TAC_CALL_PUSH_INT:
                if (inst.x <= 0xFFFF) {
                    fprintf(output, "    li %d, %d\n", (call_count++)+call_base, inst.x);
                } else {
                    fprintf(output, "    lis %d, %d\n", call_count+call_base, inst.x & 0xFFFF);
                    fprintf(output, "    ori %d, %d\n", (call_count++)+call_base, inst.x >> 16);
                }
                assert(call_count < 8);
                break;
            case TAC_CALL_PUSH_SYM:
                if (ir.symbols.data[inst.x].string != NULL) {
                    fprintf(output, "    lis %d, %.*s@ha\n",
                            call_count + call_base,
                            PS(ir.symbols.data[inst.x]));
                    fprintf(output, "    ori %d, %d, %.*s@l\n",
                            call_count + call_base, call_count + call_base,
                            PS(ir.symbols.data[inst.x]));
                } else {
                    fprintf(output, "    lis %d, data_%d@ha\n",
                            call_count + call_base, inst.x);
                    fprintf(output, "    ori %d, %d, data_%d@l\n",
                            call_count + call_base, call_count + call_base,
                            inst.x);
                }
                call_count++;
                break;
            case TAC_LOAD_INT:
                if (!inst.result) break;
                if (inst.x <= 0xFFFF) {
                    fprintf(output, "    li %d, %d\n", r, inst.x);
                } else {
                    fprintf(output, "    lis %d, %d\n", r, inst.x & 0xFFFF);
                    fprintf(output, "    ori %d, %d\n", r, inst.x >> 16);
                }
                break;
            case TAC_LOAD_ARG:
                if (!inst.result) break;
                fprintf(output, "    mr %d, %d\n", r, inst.x + call_base);
                break;
            case TAC_LOAD_SYM:
                if (!inst.result) break;
                if (ir.symbols.data[inst.x].string != NULL) {
                    fprintf(output, "    lis %d, %.*s@ha\n", r,
                            PS(ir.symbols.data[inst.x]));
                    fprintf(output, "    ori %d, %d, %.*s@l\n", r, r,
                            PS(ir.symbols.data[inst.x]));
                } else {
                    fprintf(output, "    lis %d, data_%d@ha\n", r, inst.x);
                    fprintf(output, "    ori %d, %d, data_%d@l\n", r, r,
                            inst.x);
                }
                break;
            case TAC_MOV:
                if (!inst.result) break;
                fprintf(output, "    mr %d, %d\n", r, x);
                break;
            case TAC_ADD:
                if (!inst.result) break;
                fprintf(output, "    add %d, %d, %d\n", r, x, y);
                break;
            case TAC_ADDI:
                if (!inst.result) break;
                fprintf(output, "    addi %d, %d, %d\n", r, x, inst.y);
                break;
            case TAC_SUB:
                if (!inst.result) break;
                fprintf(output, "    sub %d, %d, %d\n", r, x, y);
                break;
            case TAC_SUBI:
                if (!inst.result) break;
                fprintf(output, "    subi %d, %d, %d\n", r, x, inst.y);
                break;
            case TAC_LT:
                if (!inst.result) break;
                fprintf(output, "    cmpw %%cr0, %d, %d\n", y, x);
                fprintf(output, "    mfcr %d\n", r);
                fprintf(output, "    rlwinm %d, %d, 2, 31, 31\n", r, r);
                break;
            case TAC_LTI:
                if (!inst.result) break;
                fprintf(output, "    cmpwi %%cr0, %d, %d\n", x, inst.y);
                fprintf(output, "    mfcr %d\n", r);
                fprintf(output, "    rlwinm %d, %d, 1, 31, 31\n", r, r);
                break;
            case TAC_LABEL:
                fprintf(output, ".label%d:\n", inst.x);
                break;
            case TAC_GOTO:
                fprintf(output, "    b .label%d\n", inst.x);
                break;
            case TAC_BIZ:
                fprintf(output, "    cmpwi %d, 0\n", x);
                fprintf(output, "    beq .label%d\n", inst.y);
                break;
            case TAC_RETURN_VAL:
                fprintf(output, "    mr 3, %d\n", x);
                break;
            case TAC_RETURN_INT:
                if (inst.x <= 0xFFFF) {
                    fprintf(output, "    li 3, %d\n", inst.x);
                } else {
                    fprintf(output, "    li 3, %d\n", inst.x & 0xFFFF);
                    fprintf(output, "    ori 3, %d\n", inst.x >> 16);
                }
                break;
            case TAC_NOP:
                break;
            }
        }
        fprintf(output, "\n");
        for (int j = 0; j < ir.functions.data[i].temps_count; j++) {
            fprintf(output, "    lwz %d, %d(1)\n", j + gpr_base,
                    stack_frame - j * 4);
        }
        fprintf(output, "    lwz 0, %d(1)\n", stack_frame + 4);
        fprintf(output, "    mtlr 0\n");
        fprintf(output, "    addi 1,1,%d\n", stack_frame);
        fprintf(output, "    blr\n");
    }
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
