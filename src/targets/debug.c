#include <stdlib.h>

#include "../codegen.h"

void codegen_debug(IR ir, FILE *output) {
    for (size_t i = 0; i < ir.functions.len; i++) {
        TAC32Arr func = ir.functions.data[i].code;
        fprintf(output, "%.*s %d {\n",
                PS(ir.symbols.data[ir.functions.data[i].name]),
                ir.functions.data[i].temps_count);
        int call_count = 0;
        for (size_t j = 0; j < func.len; j++) {
            TAC32 inst = func.data[j];
            uint32_t r = inst.result-1, x = inst.x-1, y = inst.y-1;
            switch (inst.function) {
            case TAC_CALL_REG:
                if (inst.result)
                    fprintf(output, "    r%d = ", r);
                else
                    fprintf(output, "    ");
                fprintf(output, "r%d()\n", x);
                break;
            case TAC_CALL_SYM:
                if (inst.result)
                    fprintf(output, "    r%d = ", r);
                else
                    fprintf(output, "    ");
                fprintf(output, "%.*s()\n", PS(ir.symbols.data[inst.x]));
                call_count = 0;
                break;
            case TAC_CALL_PUSH:
                fprintf(output, "    c%d = r%d\n", (call_count++), x);
                break;
            case TAC_CALL_PUSH_INT:
                fprintf(output, "    c%d = %d\n", (call_count++), inst.x);
                break;
            case TAC_CALL_PUSH_SYM:
                if (ir.symbols.data[inst.x].string != NULL) {
                    fprintf(output, "    c%d = [%.*s]\n", inst.result,
                            PS(ir.symbols.data[inst.x]));
                } else {
                    fprintf(output, "    c%d = [data_%d]\n", (call_count++),
                            inst.x);
                }
                break;
            case TAC_LOAD_INT:
                fprintf(output, "    r%d = %d\n", r, inst.x);
                break;
            case TAC_LOAD_ARG:
                fprintf(output, "    r%d = arg%d\n", r, inst.x);
                break;
            case TAC_LOAD_SYM:
                if (ir.symbols.data[inst.x].string != NULL) {
                    fprintf(output, "    r%d = [%.*s]\n", r,
                            PS(ir.symbols.data[inst.x]));
                } else {
                    fprintf(output, "    r%d = [data_%d]\n", r, inst.x);
                }
                break;
            case TAC_MOV:
                fprintf(output, "    r%d = r%d\n", r, x);
                break;
            case TAC_ADD:
                fprintf(output, "    r%d = r%d + r%d\n", r, x, y);
                break;
            case TAC_ADDI:
                fprintf(output, "    r%d = r%d + %d\n", r, x, inst.y);
                break;
            case TAC_SUB:
                fprintf(output, "    r%d = r%d - r%d\n", r, x, y);
                break;
            case TAC_SUBI:
                fprintf(output, "    r%d = r%d - %d\n", r, x, inst.y);
                break;
            case TAC_LT:
                fprintf(output, "    r%d = r%d < r%d\n", r, x, y);
                break;
            case TAC_LTI:
                fprintf(output, "    r%d = r%d < %d\n", r, x, inst.y);
                break;
            case TAC_GOTO:
                fprintf(output, "    b label_%d\n", inst.x);
                break;
            case TAC_BIZ:
                fprintf(output, "    biz r%d, label_%d\n", x, inst.y);
                break;
            case TAC_LABEL:
                fprintf(output, "label_%d:\n", inst.x);
                break;
            case TAC_RETURN_VAL:
                fprintf(output, "    ret = r%d\n", x);
                break;
            case TAC_RETURN_INT:
                fprintf(output, "    ret = %d\n", inst.x);
                break;
            case TAC_NOP:
                // i had to debug it so it's helpful
                fprintf(output, "    nop\n");
                break;
            }
        }
        fprintf(output, "}\n");
    }
    for (size_t i = 0; i < ir.data.len; i++) {
        StaticVariable var = ir.data.data[i];
        String name = ir.symbols.data[var.name];
        fprintf(output, "data_");
        if (name.string != NULL)
            fprintf(output, "%.*s", PS(name));
        else
            fprintf(output, "%zu", var.name);
        fprintf(output, " { ");
        generate_string(output, var.data);
        fprintf(output, "}\n");
    }
}
