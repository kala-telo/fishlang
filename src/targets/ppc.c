#include "../codegen.h"
#include "../tac.h"
#include "../todo.h"

// 5 is a random number tbh
#define MAX_TEMP_REGISTERS 5

#define MIN(x, y) ((x) > (y) ? (y) : (x))

// it's expected to use from 0 to like 5
// per iR instruction, so 2 for input,
// 1 for output, and maybe some for calls.
// it will use call registers, which sounds
// like questionable choice but we'll see
static uint8_t used_temp_registers = 0;
static uint32_t register_map[MAX_TEMP_REGISTERS] = {0};

static uint32_t access_call_register(FILE *output, uint32_t reg) {
    (void)output;
    const int call_base = 3;
    if (reg > 8) {
        fprintf(stderr, "Reg: %d\n", reg);
        TODO();
    }
    return reg + call_base;
}

static uint32_t access_register(FILE *output, uint32_t stack_frame,
                                uint32_t reg) {
    const int gpr_base = 14;
    if (reg == 0) return 0;

    // 19 is the amount of nonvolatile registers on powerpc
    if (reg < 18)
        return reg + gpr_base - 1;

    assert(used_temp_registers+1 < MAX_TEMP_REGISTERS);
    // 10th register is the last call register
    // it might break if code loads things into temp registers
    // before calling, needs testing
    register_map[used_temp_registers] = reg;
    uint32_t hardware_register = 10-(used_temp_registers++);
    fprintf(output, "    lwz %d,%d(1)\n",
            hardware_register,
            stack_frame-reg*4);
    return hardware_register;
}

static void free_temp_registers(FILE* output, uint32_t stack_frame) {
    for (uint8_t i = 0; i < used_temp_registers; i++) {
        uint32_t hardware_register = 10-i;
        fprintf(output, "    stw %d,%d(1)\n",
                hardware_register,
                stack_frame-register_map[i]*4);
    }
    used_temp_registers = 0;
}

void codegen_powerpc(IR ir, FILE *output) {
    for (size_t i = 0; i < ir.functions.len; i++) {
        TAC32Arr func = ir.functions.data[i].code;
        String func_name = ir.symbols.data[ir.functions.data[i].name];
        fprintf(output, ".global %.*s\n", PS(func_name));
        fprintf(output, "%.*s:\n", PS(func_name));
        int call_count = 0;
        int stack_frame = 16 + 4 * ir.functions.data[i].temps_count;
        if (stack_frame % 16 != 0)
            stack_frame += 16 - stack_frame % 16;
        fprintf(output, "    stwu 1,-%d(1)\n", stack_frame);
        fprintf(output, "    mflr 0\n");
        fprintf(output, "    stw 0, %d(1)\n", stack_frame + 4);
        int real_registers = MIN(ir.functions.data[i].temps_count, 18);
        for (int j = 0; j < real_registers; j++) {
            fprintf(output, "    stw %d, %d(1)\n",
                    access_register(output, stack_frame, j+1),
                    (real_registers - j) * 4);
        }
        fprintf(output, "\n");
        for (size_t j = 0; j < func.len; j++) {
            TAC32 inst = func.data[j];
            uint32_t r = ~0, x = ~0, y = ~0;
            switch (get_arity(inst.function)) {
            case A_BINARY:
                y = access_register(output, stack_frame, inst.y);
                /* fallthrough */
            case A_UNARY:
                x = access_register(output, stack_frame, inst.x);
                /* fallthrough */
            case A_NULLARY:
                r = access_register(output, stack_frame, inst.result);
                break;
            }
            switch (inst.function) {
            case TAC_CALL_REG:
                fprintf(output, "    mtctr %d\n", x);
                fprintf(output, "    bctrl\n");
                if (inst.result)
                    fprintf(output, "    mr %d, 3\n", r);
                call_count = 0;
                break;
            case TAC_CALL_SYM:
                fprintf(output, "    bl %.*s\n", PS(ir.symbols.data[inst.x]));
                if (inst.result)
                    fprintf(output, "    mr %d, 3\n", r);
                call_count = 0;
                break;
            case TAC_CALL_PUSH: {
                uint32_t o = access_call_register(output, inst.y-1-call_count++);
                fprintf(output, "    mr %d, %d\n", o, x);
            } break;
            case TAC_CALL_PUSH_INT: {
                uint32_t o = access_call_register(output, inst.y-1-call_count++);
                if (inst.x <= 0xFFFF) {
                    fprintf(output, "    li %d, %d\n", o, inst.x);
                } else {
                    fprintf(output, "    lis %d, %d\n", o, inst.x & 0xFFFF);
                    fprintf(output, "    ori %d, %d\n", o, inst.x >> 16);
                }
            } break;
            case TAC_CALL_PUSH_SYM: {
                uint32_t o = access_call_register(output, inst.y-1-call_count++);
                if (ir.symbols.data[inst.x].string != NULL) {
                    fprintf(output, "    lis %d, %.*s@ha\n",
                            o, PS(ir.symbols.data[inst.x]));
                    fprintf(output, "    ori %d, %d, %.*s@l\n",
                            o, o, PS(ir.symbols.data[inst.x]));
                } else {
                    fprintf(output, "    lis %d, data_%d@ha\n",
                            o, inst.x);
                    fprintf(output, "    ori %d, %d, data_%d@l\n",
                            o, o, inst.x);
                }
            } break;
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
                fprintf(output, "    mr %d, %d\n", r,
                        access_call_register(output, inst.x));
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
                fprintf(output, "    %.*s.label%d:\n", PS(func_name), inst.x);
                break;
            case TAC_GOTO:
                fprintf(output, "    b %.*s.label%d\n", PS(func_name), inst.x);
                break;
            case TAC_BIZ:
                fprintf(output, "    cmpwi %d, 0\n", x);
                fprintf(output, "    beq %.*s.label%d\n", PS(func_name), inst.y);
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
            case TAC_EXIT:
                fprintf(output, "    b %.*s.epilogue\n", PS(func_name));
                break;
            case TAC_PHI:
                UNREACHABLE();
                break;
            case TAC_NOP:
                break;
            }
            free_temp_registers(output, stack_frame);
        }
        fprintf(output, "\n");
        fprintf(output, "    %.*s.epilogue:\n", PS(func_name));
        for (int j = 0; j < real_registers; j++) {
            fprintf(output, "    lwz %d, %d(1)\n",
                    access_register(output, stack_frame, j+1),
                    (real_registers - j) * 4);
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
