#include <stdlib.h>

#include "../codegen.h"
#include "../todo.h"
#include "../da.h"

/*
typedef struct {
    struct {
        String *data;
        size_t len, capacity;
    } symbols;

    struct {
        typedef struct {
            size_t name;
            typedef struct {
                typedef struct {
                    uint32_t result;
                    TACOp function;
                    uint32_t x, y;
                } TAC32 *data;
                size_t len, capacity;
            } TAC32Arr code;
            uint16_t temps_count;
        } StaticFunction *data;
        size_t len, capacity;
    } functions;

    struct {
        typedef struct {
            size_t name;
            String data;
        } StaticVariable *data;
        size_t len, capacity;
    } data;
} IR;
*/

typedef struct {
    short size;
    uint32_t data;
} PDP8Const;

typedef struct {
    PDP8Const *data;
    size_t len, capacity;
} PDP8ConstArr;

typedef struct {
    size_t *data;
    size_t len, capacity;
} PDP8Stack;

static size_t create_const(Arena *arena, PDP8ConstArr *consts, uint32_t number, short *const_size) {
    *const_size = 0; // Размер константы в 12-битных словах
    
    if (number < 1<<12 || -number < 1<<12)
        *const_size = 1;
    else if (number < 1<<12 || -number < 1<<24)
        *const_size = 2;
    else
        *const_size = 3;
    
    size_t const_idx;
    bool find = false;
    for (const_idx = 0; const_idx < consts->len; const_idx++) {
        if (consts->data[const_idx].data == number){
            find = true;
            break;
        }
    }
    if (!find) {
        PDP8Const c = {.data = number, .size = *const_size};
        da_append(arena, *consts, c);
        const_idx = consts->len-1;
    }

    return const_idx;
}

static void pdp8_prepare_for_call(FILE* output, TAC32Arr func, PDP8Stack stack, IR ir, Arena *arena, PDP8ConstArr *consts) {
    short const_size;
    size_t const_idx;
    
    for (size_t i = stack.len; i-- > 0;) {
        TAC32 inst = func.data[stack.data[i]];
        uint32_t x = inst.x - 1;

        switch (inst.function) {
        case TAC_CALL_PUSH:
            fprintf(output, "\tTAD REGS+%o\n", x*3);
            fprintf(output, "\tDCA I SP1\n");
            fprintf(output, "\tTAD REGS+%o\n", x*3 + 1);
            fprintf(output, "\tDCA I SP1\n");
            fprintf(output, "\tTAD REGS+%o\n", x*3 + 2);
            fprintf(output, "\tDCA I SP1\n");
            break;
        case TAC_CALL_PUSH_INT:
            const_idx = create_const(arena, consts, inst.x, &const_size);

            for (short k = 0; k < 3; k++) {
                if (k < const_size)
                    fprintf(output, "\tTAD i%zu\n", const_idx + k);
                fprintf(output, "\tDCA I SP1\n");
            }
            break;
        case TAC_CALL_PUSH_SYM:
            /*fprintf(output, "\tTAD d%zu", inst.x);
            if (ir.symbols.data[inst.x].string != NULL)
                fprintf(output, " / %.*s", PS(ir.symbols.data[inst.x]));
            fputc('\n', output);*/
            if (ir.symbols.data[inst.x].string != NULL)
                fprintf(output, "\tTAD %.*s\n", PS(ir.symbols.data[inst.x]));
            else
                fprintf(output, "\tTAD d%u\n", inst.x);
            fprintf(output, "\tDCA I SP1\n");
            fprintf(output, "\tDCA I SP1\n");
            fprintf(output, "\tDCA I SP1\n");
            break;
        default:
            fprintf(stderr, "Function: %d\n", inst.function);
            UNREACHABLE();
        }

        fputc('\n', output);
    }
}

void codegen_pdp8(IR ir, FILE *output) {
    fprintf(output, "\
*1\n\
\tHLT\n\
TMP1,\n\
*5\n\
N3,\t7775 / -3\n\
P2,\t2 / 2\n\
P3,\t3 / 3\n\
N1,\t7777 / -1\n\
*10\n\
SP1,\tSTACK-1\n\
SP2,\n\
TMP2,\n\
TMP3,\n\
TMP4,\n\
N24,\t7750 / -24\n\
N4,\t7774 / -4\n\
*20\n\
REGS,\n\
*50\n\
STACK,\n\
*300\n\
/ Пролог функции\n\
PRLG,\t0\n\
\tTAD SP2\n\
\tTAD P3\n\
\tDCA TMP2 / TMP2 - указатель на область стека c регистрами\n\
\tTAD REGS\n\
\tTAD N1\n\
\tDCA TMP3 / TMP3 - указатель на регистры для чтения\n\
\tTAD TMP3\n\
\tDCA TMP4 / TMP4 - указатель на регистры для стирания\n\
\tTAD I PRLG\n\
\tDCA TMP1 / TMP1 - счётчик\n\
PRLGL,\tTAD I TMP3\n\
\tDCA I TMP2\n\
\tDCA I TMP4\n\
\tISZ TMP1\n\
\t JMP PRLGL\n\
\tTAD SP2\n\
\tTAD N1\n\
\tDCA SP2\n\
\tISZ PRLG\n\
\tJMP I PRLG\n\
/ Эпилог функции\n\
EPLG,\t0\n\
\tTAD SP2\n\
\tTAD P3\n\
\tDCA TMP2 / TMP2 - указатель на область стека c регистрами\n\
\tTAD REGS\n\
\tTAD N1\n\
\tDCA TMP3 / TMP3 - указатель на регистры для записи\n\
\tTAD I EPLG\n\
\tDCA TMP1 / TMP1 - счётчик\n\
EPLGL,\tTAD I TMP2\n\
\tDCA I TMP3\n\
\tISZ TMP1\n\
\t JMP EPLGL\n\
\tJMP I SP2\n\
/    \\/ Сгенерированный код \\/\n\
");
    Arena arena = {0};
    PDP8ConstArr consts = {0};

    for (size_t i = 0; i < ir.functions.len; i++) {
        PDP8Stack stack = {0};

        StaticFunction func = ir.functions.data[i];
        uint16_t temps_count = func.temps_count;
        
        /*fprintf(output, "/ %.*s\n", PS(ir.symbols.data[func.name]));
        fprintf(output, "d%zu,\t0\n", func.name);*/
        fprintf(output, "%.*s,\t0\n", PS(ir.symbols.data[func.name]));
        fprintf(output, "\tTAD .-1\n");
        fprintf(output, "\tDCA I SP2\n");
        if (temps_count > 0) {
            fprintf(output, "\tJMS PRLG\n");
            fprintf(output, "\t%o\n", -temps_count & 07777);
        }
        fprintf(output, "\n");

        bool store_sp2 = true; // Первое сохраниние аргумента в стек, надо скопировать старый SP
        for (size_t j = 0; j < func.code.len; j++) {
            TAC32 inst = func.code.data[j];
            uint32_t r = inst.result-1, x = inst.x-1, y = inst.y-1;

            short const_size;
            size_t const_idx;
            uint32_t number;

            switch (inst.function) {
            case TAC_CALL_REG:
                pdp8_prepare_for_call(output, func.code, stack, ir, &arena, &consts);
                fprintf(output, "\tJMS I REGS+%o\n", x*3);
                if (inst.result) {
                    fprintf(output, "\tISZ SP2\n");
                    fprintf(output, "\tTAD I SP2\n");
                    fprintf(output, "\tDCA REGS+%o\n", r*3);
                    fprintf(output, "\tTAD I SP2\n");
                    fprintf(output, "\tDCA REGS+%o\n", r*3 + 1);
                    fprintf(output, "\tTAD I SP2\n");
                    fprintf(output, "\tDCA REGS+%o\n", r*3 + 2);
                    fprintf(output, "\tTAD SP2\n");
                    fprintf(output, "\tTAD N3\n");
                    fprintf(output, "\tDCA SP1\n");
                } else {
                    fprintf(output, "\tTAD SP2\n");
                    fprintf(output, "\tDCA SP1\n");
                }
                store_sp2 = true;
                stack.len = 0;
                break;
            case TAC_CALL_SYM:
                pdp8_prepare_for_call(output, func.code, stack, ir, &arena, &consts);
                /*fprintf(output, "\tJMS I d%zu", inst.x);
                if (ir.symbols.data[inst.x].string != NULL)
                    fprintf(output, " / %.*s()", PS(ir.symbols.data[inst.x]));
                fputc('\n', output);*/
                fprintf(output, "\tJMS I %.*s\n", PS(ir.symbols.data[inst.x]));

                if (inst.result) {
                    fprintf(output, "\tISZ SP2\n");
                    fprintf(output, "\tTAD I SP2\n");
                    fprintf(output, "\tDCA REGS+%o\n", r*3);
                    fprintf(output, "\tTAD I SP2\n");
                    fprintf(output, "\tDCA REGS+%o\n", r*3 + 1);
                    fprintf(output, "\tTAD I SP2\n");
                    fprintf(output, "\tDCA REGS+%o\n", r*3 + 2);
                    fprintf(output, "\tTAD SP2\n");
                    fprintf(output, "\tTAD N3\n");
                    fprintf(output, "\tDCA SP1\n");
                } else {
                    fprintf(output, "\tTAD SP2\n");
                    fprintf(output, "\tDCA SP1\n");
                }
                store_sp2 = true;
                stack.len = 0;
                break;
            case TAC_CALL_PUSH:
            case TAC_CALL_PUSH_INT:
            case TAC_CALL_PUSH_SYM:
                if (store_sp2) {
                    const_idx = create_const(&arena, &consts, temps_count*3 + 3, &const_size);
                    fprintf(output, "\tTAD SP1\n");
                    fprintf(output, "\tDCA SP2\n");
                    fprintf(output, "\tTAD SP1\n");
                    fprintf(output, "\tTAD i%zu\n", const_idx);
                    fprintf(output, "\tDCA SP1\n");
                    store_sp2 = false;
                }
                da_append(&arena, stack, j);
                break;
            case TAC_LOAD_INT:
                const_idx = create_const(&arena, &consts, inst.x, &const_size);

                for (short k = 0; k < 3; k++) {
                    if (k < const_size)
                        fprintf(output, "\tTAD i%zu\n", const_idx + k);
                    fprintf(output, "\tDCA REGS+%o\n", r*3 + k);
                }
                break;
            case TAC_LOAD_ARG:
                const_idx = create_const(&arena, &consts, ((inst.x+1) * 3) & 07777, &const_size);
                fprintf(output, "\tTAD i%zu\n", const_idx);
                fprintf(output, "\tCIA\n");
                fprintf(output, "\tTAD SP1\n");
                fprintf(output, "\tDCA SP1\n");
                for (short k = 0; k < 3; k++) {
                    fprintf(output, "\tTAD I SP1\n");
                    fprintf(output, "\tDCA REGS+%o\n", r*3 + k);
                }
                fprintf(output, "\tTAD i%zu\n", const_idx);
                fprintf(output, "\tTAD N3\n");
                fprintf(output, "\tTAD SP1\n");
                fprintf(output, "\tDCA SP1\n");
                break;
            case TAC_LOAD_SYM:
                /*fprintf(output, "\tTAD d%zu", inst.x);
                if (ir.symbols.data[inst.x].string != NULL)
                    fprintf(output, " / %.*s", PS(ir.symbols.data[inst.x]));
                fputc('\n', output);*/
                if (ir.symbols.data[inst.x].string != NULL)
                    fprintf(output, "\tTAD %.*s\n", PS(ir.symbols.data[inst.x]));
                else
                    fprintf(output, "\tTAD d%u\n", inst.x);
                fprintf(output, "\tDCA REGS+%o\n", r*3);
                fprintf(output, "\tDCA REGS+%o\n", r*3 + 1);
                fprintf(output, "\tDCA REGS+%o\n", r*3 + 2);
                break;
            case TAC_MOV:
                for (short k = 0; k < 3; k++) {
                    fprintf(output, "\tTAD REGS+%o\n", x*3 + k);
                    fprintf(output, "\tDCA REGS+%o\n", r*3 + k);
                }
                break;
            case TAC_ADD:
                for (short k = 0; k < 3; k++) {
                    fprintf(output, "\tTAD REGS+%o\n", x*3 + k);
                    fprintf(output, "\tTAD REGS+%o\n", y*3 + k);
                    fprintf(output, "\tDCA REGS+%o\n", r*3 + k);
                    if (k < 2)
                        fprintf(output, "\tRAL\n");
                }
                fprintf(output, "\tCLL\n");
                break;
            
            case TAC_SUB:
                for (short k = 0; k < 3; k++) {
                    fprintf(output, "\tTAD REGS+%o\n", y*3 + k);
                    fprintf(output, "\tCIA\n");
                    fprintf(output, "\tTAD REGS+%o\n", x*3 + k);
                    fprintf(output, "\tDCA REGS+%o\n", r*3 + k);
                    if (k < 2)
                        fprintf(output, "\tRAL\n");
                }
                fprintf(output, "\tCLL\n");
                break;
            case TAC_ADDI:
            case TAC_SUBI:
                const_idx = create_const(&arena, &consts, 
                                         inst.y * (inst.function == TAC_ADDI ? 1 : -1),
                                         &const_size);
                for (short k = 0; k < 3; k++) {
                    fprintf(output, "\tTAD REGS+%o\n", x*3 + k);
                    if (k < const_size)
                        fprintf(output, "\tTAD i%zu\n", const_idx + k);
                    fprintf(output, "\tDCA I REGS+%o\n", r*3 + k);
                }
                break;
            case TAC_LT:
                for (short k = 0; k < 3; k++) {
                    fprintf(output, "\tTAD REGS+%o\n", x*3 + k);
                    fprintf(output, "\tCIA\n");
                    fprintf(output, "\tTAD REGS+%o\n", y*3 + k);
                    if (k < 2)
                        fprintf(output, "\tCLA RAL\n");
                }
                fprintf(output, "\tDCA TMP1\n");
                fprintf(output, "\tDCA REGS+%o\n", r*3);
                fprintf(output, "\tDCA REGS+%o\n", r*3 + 1);
                fprintf(output, "\tDCA REGS+%o\n", r*3 + 2);
                fprintf(output, "\tTAD TMP1\n");
                fprintf(output, "\tSPA SZL\n");
                fprintf(output, "\t ISZ REGS+%o\n", r*3);
                fprintf(output, "\tCLA CLL\n");
                break;
            case TAC_GOTO:
                fprintf(output, "\tJMP l%u\n", inst.x);
                break;
            case TAC_BIZ:
                for (short k = 0; k < 3; k++) {
                    fprintf(output, "\tTAD REGS+%o\n", x*3 + k);
                    fprintf(output, "\tSZA\n");
                    fprintf(output, "\t JMP .+%o\n", 8 - k*3);
                }
                fprintf(output, "\tJMP l%u\n", inst.y);
                break;
            case TAC_LABEL:
                fprintf(output, "l%u,", inst.x);
                break;
            case TAC_RETURN_VAL:
                fprintf(output, "\tISZ SP2\n");
                for (short k = 0; k < 3; k++) {
                    fprintf(output, "\tTAD REGS+%o\n", x*3 + k);
                    fprintf(output, "\tDCA I SP2\n");
                }
                fprintf(output, "\tTAD SP2\n");
                fprintf(output, "\tTAD N4\n");
                fprintf(output, "\tDCA SP2\n");
                break;
            case TAC_RETURN_INT:
                const_idx = create_const(&arena, &consts, x, &const_size);
                fprintf(output, "\tISZ SP2\n");
                for (short k = 0; k < 3; k++) {
                    fprintf(output, "\tTAD i%zu\n", const_idx + k);
                    fprintf(output, "\tDCA I SP2\n");
                }
                fprintf(output, "\tTAD SP2\n");
                fprintf(output, "\tTAD N4\n");
                fprintf(output, "\tDCA SP2\n");
                break;
            case TAC_NOP:
                break;
            }
            fputc('\n', output);
        }
        
        if (temps_count > 0) {
            fprintf(output, "\tJMS EPLG\n");
            fprintf(output, "\t%o\n", -temps_count & 07777);
        }
        else {
            fprintf(output, "\tJMP I SP2\n");
        }
    }

    fputc('\n', output);

    for (size_t i = 0; i < ir.data.len; i++) {
        StaticVariable var = ir.data.data[i];
        String name = ir.symbols.data[var.name];
        
        if (name.string != NULL)
            fprintf(output, "/ %.*s\n", PS(name));
        fprintf(output, "d%zu,", var.name);
        /*for (int j = 0; j < var.data.length; j++) {
            fprintf(output, "\t%d\n", var.data.string[j]);
        }
        fputs("\t0\n", output);*/
        generate_string(output, var.data, true);
    }

    for (size_t i = 0; i < consts.len; i++) {
        PDP8Const c = consts.data[i];
        fprintf(output, "i%zu,", i);
        for (short j = 0; j < c.size; j++) {
            fprintf(output, "\t%o\n", (c.data >> (j * 12)) & 07777);
        }
    }

    arena_destroy(&arena);
}
