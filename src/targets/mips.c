#include "../codegen.h"
#include "../todo.h"

// i really should put those in some one place
#define ALIGN(value, size) (((value)+(size)-1)&~((size)-1))
#define MIN(x, y) ((x) > (y) ? (y) : (x))
#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define ARRLEN(xs) (sizeof(xs) / sizeof(*(xs)))

// temporary registers as of per MIPS ABI
static const uint8_t TEMP_REGISTERS[] = {
    25, 24, 15, 14, 13, 12, 11, 10, 9, 8
};

#define MAX_TEMP_REGISTERS ARRLEN(TEMP_REGISTERS)

// mostly stolen from powerpc
static uint8_t used_temp_registers = 0;

static int32_t register_map[MAX_TEMP_REGISTERS] = {0};

static uint32_t access_local_call_register(FILE *output, uint32_t reg,
                                           uint32_t total_count) {
    const int call_base = 4;
    int32_t reversed = total_count-1;
    if (reversed-reg < 4)
        return reversed - reg + call_base;

    assert((size_t)used_temp_registers+1 < MAX_TEMP_REGISTERS);
    int32_t stack_offset = -reg*4-4;
    register_map[used_temp_registers] = stack_offset;
    uint32_t hardware_register = TEMP_REGISTERS[used_temp_registers++];

    fprintf(output, "    lw $%d, %d($sp)\n", hardware_register, stack_offset);
    fprintf(output, "    nop\n");
    return hardware_register;
}

static uint32_t access_call_register(FILE *output, uint32_t frame, uint32_t reg) {
    const int call_base = 4;
    if (reg < 4)
        return reg + call_base;

    assert((size_t)used_temp_registers+1 < MAX_TEMP_REGISTERS);
    int32_t stack_offset = (frame + reg*4);
    register_map[used_temp_registers] = stack_offset;
    uint32_t hardware_register = TEMP_REGISTERS[used_temp_registers++];

    fprintf(output, "    lw $%d, %d($sp)\n", hardware_register, stack_offset);
    fprintf(output, "    nop\n");
    return hardware_register;
}

static uint32_t access_register(FILE *output, uint32_t frame, uint32_t reg) {
    if (reg == 0) return 0;
    const int gpr_base = 16;

    // 8 is the amount of saved registers on mips
    if (reg <= 8)
        return reg + gpr_base - 1;

    assert((size_t)used_temp_registers+1 < MAX_TEMP_REGISTERS);
    int32_t stack_offset = frame-reg*4-4;
    register_map[used_temp_registers] = stack_offset;
    uint32_t hardware_register = TEMP_REGISTERS[used_temp_registers++];
    fprintf(output, "    lw $%d, %d($sp)\n", hardware_register, stack_offset);
    fprintf(output, "    nop\n");
    return hardware_register;
}

static void free_temp_registers(FILE* output) {
    for (uint8_t i = 0; i < used_temp_registers; i++) {
        uint32_t hardware_register = TEMP_REGISTERS[i];
        fprintf(output, "    sw $%d, %d($sp)\n",
                hardware_register,
                register_map[i]);
    }
    used_temp_registers = 0;
}

void codegen_mips(IR ir, FILE *output) {
    fprintf(output, ".set noreorder\n");
    fprintf(output, ".set nomacro\n");
    fprintf(output, ".option pic0\n");
    fprintf(output, ".text\n");
    fprintf(output, ".align 2\n");
    for (size_t i = 0; i < ir.functions.len; i++) {
        TAC32Arr func = ir.functions.data[i].code;
        String func_name = ir.symbols.data[ir.functions.data[i].name];
        size_t temps_count = ir.functions.data[i].temps_count;
        // +1 for return address
        int frame = ALIGN(4*(temps_count+1), 16);
        fprintf(output, ".global %.*s\n", PS(func_name));
        fprintf(output, "%.*s:\n", PS(func_name));
        fprintf(output, "    addiu $sp, $sp, -%d\n", frame);
        fprintf(output, "    sw $ra, %d($sp)\n", frame-4);
        int real_registers = MIN(ir.functions.data[i].temps_count, 8);
        for (int j = 0; j < real_registers; j++) {
            fprintf(output, "    sw $%d, %d($sp)\n",
                    access_register(output, frame, j+1),
                    // one +1 is for $ra,
                    // second +1 is for address itself
                    // (since the last frame element is frame-4)
                    frame - (j + 1 + 1) * 4);
        }
        fprintf(output, "\n");
        int call_count = 0;
        for (size_t j = 0; j < func.len; j++) {
            TAC32 inst = func.data[j];
            uint32_t r = ~0, x = ~0, y = ~0;
            switch (get_arity(inst.function)) {
            case A_BINARY:
                y = access_register(output, frame, inst.y);
                /* fallthrough */
            case A_UNARY:
                x = access_register(output, frame, inst.x);
                /* fallthrough */
            case A_NULLARY:
                r = access_register(output, frame, inst.result);
                break;
            }
            switch (inst.function) {
            case TAC_LOAD_ARG:
                fprintf(output, "    move $%d, $%d\n", r,
                        access_call_register(output, frame, inst.x));
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
                fprintf(output, "    li $%d, %d\n", r, inst.x);
                break;
            case TAC_CALL_PUSH: {
                uint32_t o = access_local_call_register(output, call_count++, inst.y);
                fprintf(output, "    move $%d, $%d\n", o, x);
            } break;
            case TAC_CALL_PUSH_INT: {
                uint32_t o = access_local_call_register(output, call_count++, inst.y);
                fprintf(output, "    li $%d, %d\n", o, inst.x);
            } break;
            case TAC_CALL_PUSH_SYM: {
                uint32_t o = access_local_call_register(output, call_count++, inst.y);
                if (ir.symbols.data[inst.x].string != NULL) {
                    fprintf(output, "    lui $%d, %%hi(%.*s)\n",
                            o, PS(ir.symbols.data[inst.x]));
                    fprintf(output, "    addiu $%d, %%lo(%.*s)\n",
                            o, PS(ir.symbols.data[inst.x]));
                } else {
                    fprintf(output, "    lui $%d, %%hi($data_%d)\n",
                            o, inst.x);
                    fprintf(output, "    addiu $%d, %%lo($data_%d)\n",
                            o, inst.x);
                }
            } break;
            case TAC_CALL_REG:
                fprintf(output, "    addiu $sp, $sp, -%d\n", MAX(inst.y, 4)*4);
                fprintf(output, "    jalr $%d\n", x);
                fprintf(output, "    nop\n");
                if (inst.result)
                    fprintf(output, "    move $%d, $2\n", r);
                fprintf(output, "    addiu $sp, $sp, %d\n", MAX(inst.y, 4)*4);
                call_count = 0;
                break;
            case TAC_CALL_SYM:
                fprintf(output, "    addiu $sp, $sp, -%d\n", MAX(inst.y, 4)*4);
                fprintf(output, "    jal %.*s\n", PS(ir.symbols.data[inst.x]));
                fprintf(output, "    nop\n");
                if (inst.result)
                    fprintf(output, "    move $%d, $2\n", r);
                fprintf(output, "    addiu $sp, $sp, %d\n", MAX(inst.y, 4)*4);
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
                fprintf(output, "    sub $%d, $%d, $%d\n", r, x, y);
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
            free_temp_registers(output);
        }
        fprintf(output, "\n");
        fprintf(output, "    %.*s.epilogue:\n", PS(func_name));
        fprintf(output, "    lw $ra, %d($sp)\n", frame-4);
        for (int j = 0; j < real_registers; j++) {
            fprintf(output, "    lw $%d, %d($sp)\n",
                    access_register(output, frame, j+1),
                    // one +1 is for $ra,
                    // second +1 is for address itself
                    // (since the last frame element is frame-4)
                    (frame - (j + 1 + 1) * 4));
        }
        fprintf(output, "    jr $ra\n");
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
