#include <stdlib.h>

#include "../codegen.h"
#include "../todo.h"
#include "../da.h"

#define STDFN_COUNT 2
#define push_cmd(...) commands.strp += snprintf(commands.strp, sizeof(commands.str), __VA_ARGS__) // Adding a command to the commands buffer

typedef struct {
    short size;         // Size in 12-bit words
    uint64_t data;      // rounding up 36 bits to 64 bits (64 > 36 > 32)
} PDP8Const;

typedef struct {
    PDP8Const *data;
    size_t len, capacity;
} PDP8ConstsArr;

typedef struct {
    String name;        // Function name
    short nuses;        // Number of function uses
    bool current_page;  // Is a function in the current page?
    bool link_on_page;  // Is there a function function on the current page?
} PDP8Function;

typedef struct {
    PDP8Function *data;
    size_t len, capacity;
} PDP8FunctionsArr;

typedef struct {
    size_t name;        // Number in the symbols table
    short nuses;
    bool link_on_page;
} PDP8Data;

typedef struct {
    PDP8Data *data;
    size_t len, capacity;
} PDP8DataArr;

typedef struct {
    size_t name;        // Number in the symbols table
    short nuses;
    bool current_page;
    bool link_on_page;
} PDP8Label;

typedef struct {
    PDP8Label *data;
    size_t len, capacity;
} PDP8LabelsArr;

typedef struct {
    char str[15*30];
    char *strp;
    bool crt_const;
    bool crt_function;
    bool crt_data;
    bool crt_label;
    union {
        uint32_t const_number;
        struct {
            String name;
            bool call;
        } function;
        size_t data_name;
        struct {
            size_t name;
            bool define;
        } label;
    } data;
} PDP8CommandsBuffer;

// Replacing all strings find in src with rplc
static void replace(char *src, char *find, char *rplc) {
    char *p = NULL;
    while ((p = strstr(src, find)) != NULL) {
        memcpy(p, rplc, strlen(rplc));
    }
}

// Calculating constant size in 12-bit words
static short get_const_size(uint32_t number) {
    if (number == 0)
        return 0;
    else if (number < 1<<12)
        return 1;
    else if (number < 1<<12)
        return 2;
    else
        return 3;
}

static size_t create_const(Arena *arena, PDP8ConstsArr *consts, uint32_t number, short *const_size) {
    *const_size = get_const_size(number); // Размер константы в 12-битных словах
    
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

static bool lookup_function(PDP8FunctionsArr *functions, String name, size_t *fn_idx_result) {
    for (size_t fn_idx = 0; fn_idx < functions->len; fn_idx++) {
        if (string_eq(functions->data[fn_idx].name, name)){
            *fn_idx_result = fn_idx;
            return true;
        }
    }
    return false;
}

static bool lookup_label(PDP8LabelsArr *labels, uint32_t name, size_t *lb_idx_result) {
    for (size_t label_idx = 0; label_idx < labels->len; label_idx++) {
        PDP8Label *lb = &(labels->data[label_idx]);
        if (lb->name == name) {
            *lb_idx_result = label_idx;
            return true;
        }
    }
    return false;
}

static bool lookup_data(PDP8DataArr *data, uint32_t name, size_t *dt_idx_result) {
    for (size_t data_idx = 0; data_idx < data->len; data_idx++) {
        PDP8Data *dt = &(data->data[data_idx]);
        if (dt->name == name){
            *dt_idx_result = data_idx;
            return true;
        }
    }
    return false;
}

static size_t create_function(Arena *arena, PDP8FunctionsArr *functions, String name, bool call) {
    size_t fn_idx;
    bool find = lookup_function(functions, name, &fn_idx);

    if (find) {
        PDP8Function *fn = &(functions->data[fn_idx]);
        if ((!fn->current_page || !call) && !fn->link_on_page) {
            fn->nuses++;
            fn->link_on_page = true;
        }
    } else {
        PDP8Function f = {.name = name, .nuses = 0, .current_page = true, .link_on_page = !call};
        da_append(arena, *functions, f);
        fn_idx = functions->len-1;
    }

    return fn_idx;
}

static size_t create_data(Arena *arena, PDP8DataArr *data_arr, size_t name) {
    size_t data_idx;

    bool find = lookup_data(data_arr, name, &data_idx);

    if (find) {
        PDP8Data *dt = &(data_arr->data[data_idx]);
        if (!dt->link_on_page) {
            dt->nuses++;
            dt->link_on_page = true;
        }
    } else {
        PDP8Data d = {.name = name, .nuses = 0, .link_on_page = true};
        da_append(arena, *data_arr, d);
        data_idx = data_arr->len-1;
    }

    return data_idx;
}

static size_t create_label(Arena *arena, PDP8LabelsArr *label_arr, size_t name, bool define) {
    size_t label_idx;

    bool find = lookup_label(label_arr, name, &label_idx);

    if (find) {
        PDP8Label *lb = &(label_arr->data[label_idx]);
        if (define) {
            lb->current_page = true;
        }
        if (!lb->current_page && !lb->link_on_page) {
            lb->nuses++;
            lb->link_on_page = true;
        }
    } else {
        PDP8Label l = {.name = name, .nuses = 0, .current_page = define, .link_on_page = !define};
        da_append(arena, *label_arr, l);
        label_idx = label_arr->len-1;
    }

    return label_idx;
}

// Adding pointers to functions, data, labels and constants to the end of the page
static void add_page_end(FILE *output, PDP8FunctionsArr *functions, PDP8ConstsArr *consts, PDP8DataArr *data, PDP8LabelsArr *labels,
                         short page_num) {
    // Adding pointers to functions
    for (size_t i = 0; i < functions->len; i++) {
        PDP8Function *fn = &(functions->data[i]);
        if (fn->link_on_page)
            fprintf(output, "f%02zul%02x,\tf%02zu\n", i, fn->nuses, i);
        fn->link_on_page = false;
        fn->current_page = false;
    }

    // Adding pointers to data
    for (size_t i = 0; i < data->len; i++) {
        PDP8Data *dt = &(data->data[i]);
        if (dt->link_on_page)
            fprintf(output, "d%02zul%02x,\td%02zu\n", i, dt->nuses, i);
        dt->link_on_page = false;
    }

    // Adding constants
    for (size_t i = 0; i < consts->len; i++) {
        PDP8Const c = consts->data[i];
        fprintf(output, "i%02zup%02x,", i, page_num);
        if (c.data >> 31 & 1) {             // If the number is negative
            c.data |= (uint64_t)0xF << 32;  // add 4 bits to it at the end (add up to 36 bits)
        }
        for (short j = 0; j < c.size; j++) {
            fprintf(output, "\t%o\n", (short)(c.data >> (j * 12)) & 07777);
        }
    }
    consts->len = 0;

    // Adding pointers to labels
    for (size_t i = 0; i < labels->len; i++) {
        PDP8Label *lb = &(labels->data[i]);
        if (lb->link_on_page)
            fprintf(output, "l%02zul%02x,\tl%02zu\n", i, lb->nuses, i);
        lb->link_on_page = false;
        lb->current_page = false;
    }
}

// Adding commands from the buffer to the output file
static void add_commands(FILE *output, PDP8CommandsBuffer *commands, Arena *arena,
                         PDP8FunctionsArr *functions, PDP8ConstsArr *consts, PDP8DataArr *data, PDP8LabelsArr *labels,
                         short *page_num, short *command_num) {
    short commands_count = 0; // Number of commands in the buffer
    for (char *commandspt = commands->str; *commandspt != '\0'; commandspt++) {
        if (*commandspt == '\n')
            commands_count++;
    }

    short data_size = 0;

    for (size_t i = 0; i < functions->len; i++) {
        if (functions->data[i].link_on_page)
            data_size++;
    }

    for (size_t i = 0; i < consts->len; i++) {
        data_size += consts->data[i].size;
    }

    for (size_t i = 0; i < data->len; i++) {
        if (data->data[i].link_on_page)
            data_size++;
    }

    for (size_t i = 0; i < labels->len; i++) {
        if (labels->data[i].link_on_page)
            data_size++;
    }

    if (commands->crt_const) {
        data_size += get_const_size(commands->data.const_number);
    }
    if (commands->crt_data) {
        size_t dt_idx;
        if (lookup_data(data, commands->data.data_name, &dt_idx))
            data_size += !data->data[dt_idx].link_on_page;
        else
            data_size++;
    }
    if (commands->crt_function) {
        size_t fn_idx;
        bool find = lookup_function(functions, commands->data.function.name, &fn_idx);
        if (find) {
            if (commands->data.function.call)
                data_size += !functions->data[fn_idx].link_on_page && !functions->data[fn_idx].current_page;
            else
                data_size += !functions->data[fn_idx].link_on_page;
        } else {
            data_size += 1;
        }
    }
    if (commands->crt_label) {
        size_t lb_idx;
        bool find = lookup_label(labels, commands->data.label.name, &lb_idx);
        if (!find && !commands->data.label.define) {
            data_size++;
        } else if (find) {
            data_size += !labels->data[lb_idx].link_on_page && !labels->data[lb_idx].current_page;
        }
    }

    if (*command_num + commands_count + 2 + data_size >= 0200) {
        (*page_num)++;
        fprintf(output, "\tJMP I pl%d\n", *page_num);
        fprintf(output, "pl%d,\t%o\n", *page_num, *page_num * 0200);
        add_page_end(output, functions, consts, data, labels, *page_num-1);
        fprintf(output, "*%o\n", *page_num * 0200);
        *command_num = 0;
    }
    *command_num += commands_count;

    if (commands->crt_const) {
        char replace_str[7];
        short const_size;
        size_t const_idx = create_const(arena, consts, commands->data.const_number, &const_size);
        snprintf(replace_str, sizeof(replace_str), "i%.2zxp%.2x", const_idx & 0xFF, *page_num & 0xFF);
        replace(commands->str, "#cnst#", replace_str);

        commands->crt_const = false;
    }

    if (commands->crt_data) {
        char replace_str[7];
        size_t data_idx = create_data(arena, data, commands->data.data_name);
        snprintf(replace_str, sizeof(replace_str), "d%.2zxl%.2x", data_idx & 0xFF, data->data[data_idx].nuses & 0xFF);
        replace(commands->str, "#data#", replace_str);

        commands->crt_data = false;
    }

    if (commands->crt_function) {
        char replace_str[9];
        size_t fn_idx = create_function(arena, functions, commands->data.function.name, commands->data.function.call);
        PDP8Function *fn = &(functions->data[fn_idx]);
        if (fn->current_page && commands->data.function.call) {
            snprintf(replace_str, sizeof(replace_str), "f%.2zx     ", fn_idx & 0xFF);
        } else {
            snprintf(replace_str, sizeof(replace_str), "%c f%.2zxl%.2x", (commands->data.function.call) ? 'I' : ' ', fn_idx & 0xFF, fn->nuses & 0xFF);
        }
        replace(commands->str, "#functn#", replace_str);

        commands->crt_function = false;
    }

    if (commands->crt_label) {
        char replace_str[9];
        size_t lb_idx = create_label(arena, labels, commands->data.label.name, commands->data.label.define);
        PDP8Label *lb = &(labels->data[lb_idx]);
        if (lb->current_page || commands->data.label.define) {
            snprintf(replace_str, sizeof(replace_str), "     l%.2zx", lb_idx & 0xFF);
        } else {
            snprintf(replace_str, sizeof(replace_str), "I l%.2zxl%.2x", lb_idx & 0xFF, lb->nuses & 0xFF);
        }
        replace(commands->str, "#labell#", replace_str);

        commands->crt_label = false;
    }

    fprintf(output, "%s", commands->str);
    fputc('\n', output);

    *commands->str = '\0';
    commands->strp = commands->str;
}

static void add_store_BP(PDP8CommandsBuffer commands) {
    push_cmd("\tTAD SP\n");
    push_cmd("\tTAD P2\n");
    push_cmd("\tDCA TMP1\n");
    push_cmd("\tTAD BP\n");
    push_cmd("\tDCA I TMP1\n");
    push_cmd("\tTAD SP\n");
    push_cmd("\tDCA BP\n");
    push_cmd("\tTAD TMP1\n");
    push_cmd("\tTAD P4\n");
    push_cmd("\tDCA SP\n");
}

void codegen_pdp8(IR ir, FILE *output) {
    FILE *fstd = fopen("stdlib/pdp8.pal", "r");
    if (fstd == NULL) {
        fprintf(stderr, "Cannot open file stdlib/pdp8.pal\n");
        abort();
    }
    int c;
    while ((c = fgetc(fstd)) != EOF) {
        fputc(c, output);
    }

    Arena arena = {0};
    PDP8ConstsArr consts = {0};
    PDP8DataArr data = {0};
    PDP8LabelsArr labels = {0};
    String std_functions[STDFN_COUNT] = {
        [0] = S("puts"),
        [1] = S("printf"),
    };
    PDP8FunctionsArr functions = {0};
    for (int i = 0; i < STDFN_COUNT; i++) {
        PDP8Function f = {.name = std_functions[i], .nuses = 0, .current_page = false, .link_on_page = false};
        da_append(&arena, functions, f);
    }

    short page_num = 3;         // Page number
    short command_num = 0131;   // Command number on the page

    for (size_t i = 0; i < ir.functions.len; i++) {
        StaticFunction func = ir.functions.data[i];
        uint16_t temps_count = func.temps_count;

        PDP8CommandsBuffer commands = {0};
        commands.strp = commands.str;

        bool BP_stored = false;

        bool is_main = string_eq(ir.symbols.data[func.name], S("main"));

        size_t fn_idx = create_function(&arena, &functions, ir.symbols.data[func.name], false);
        functions.data[fn_idx].current_page = true;

        push_cmd("f%.2zx,\t0\n", fn_idx);
        push_cmd("\tTAD .-1\n");
        if (is_main) {
            push_cmd("\tSNA\n");
            push_cmd("\t JMP MAINSP\n");
        }
        push_cmd("\tDCA I BP\n");
        push_cmd("\tJMS I PRLGP\n");
        push_cmd("\t%o\n", -temps_count * 3 & 07777);
        if (is_main)
            push_cmd("MAINSP,");

        fprintf(output, "/ %.*s\n", PS(ir.symbols.data[func.name]));
        add_commands(output, &commands, &arena, &functions, &consts, &data, &labels, &page_num, &command_num);

        for (size_t j = 0; j < func.code.len; j++) {
            TAC32 inst = func.code.data[j];
            uint32_t r = inst.result-1, x = inst.x-1, y = inst.y-1;

            short const_size;

            switch (inst.function) {
            case TAC_CALL_REG:
                if (!BP_stored) {
                    add_store_BP(commands);
                    add_commands(output, &commands, &arena, &functions, &consts, &data, &labels, &page_num, &command_num);
                }

                push_cmd("\tJMS I REGS+%o\n", x*3);
                if (inst.result) {
                    push_cmd("\tTAD SP\n");
                    push_cmd("\tTAD P3\n");
                    push_cmd("\tDCA TMP2\n");
                    push_cmd("\tTAD I TMP2\n");
                    push_cmd("\tDCA REGS+%o\n", r*3);
                    push_cmd("\tTAD I TMP2\n");
                    push_cmd("\tDCA REGS+%o\n", r*3 + 1);
                    push_cmd("\tTAD I TMP2\n");
                    push_cmd("\tDCA REGS+%o\n", r*3 + 2);
                }

                BP_stored = false;
                break;
            case TAC_CALL_SYM:
                if (!BP_stored) {
                    add_store_BP(commands);
                    add_commands(output, &commands, &arena, &functions, &consts, &data, &labels, &page_num, &command_num);
                }

                push_cmd("\tJMS #functn# / %.*s\n", PS(ir.symbols.data[inst.x]));
                commands.crt_function = true;
                commands.data.function.name = ir.symbols.data[inst.x];
                commands.data.function.call = true;

                if (inst.result) {
                    push_cmd("\tTAD SP\n");
                    push_cmd("\tTAD P3\n");
                    push_cmd("\tDCA TMP2\n");
                    push_cmd("\tTAD I TMP2\n");
                    push_cmd("\tDCA REGS+%o\n", r*3);
                    push_cmd("\tTAD I TMP2\n");
                    push_cmd("\tDCA REGS+%o\n", r*3 + 1);
                    push_cmd("\tTAD I TMP2\n");
                    push_cmd("\tDCA REGS+%o\n", r*3 + 2);
                }
                
                BP_stored = false;
                break;
            case TAC_CALL_PUSH:
                if (!BP_stored) {
                    add_store_BP(commands);
                    add_commands(output, &commands, &arena, &functions, &consts, &data, &labels, &page_num, &command_num);
                    BP_stored = true;
                }

                push_cmd("\tTAD REGS+%o\n", x*3);
                push_cmd("\tDCA I SP\n");
                push_cmd("\tTAD REGS+%o\n", x*3 + 1);
                push_cmd("\tDCA I SP\n");
                push_cmd("\tTAD REGS+%o\n", x*3 + 2);
                push_cmd("\tDCA I SP\n");
                break;
            case TAC_CALL_PUSH_INT:
                if (!BP_stored) {
                    add_store_BP(commands);
                    add_commands(output, &commands, &arena, &functions, &consts, &data, &labels, &page_num, &command_num);
                    BP_stored = true;
                }

                const_size = get_const_size(inst.x);
                commands.crt_const = const_size > 0;
                commands.data.const_number = inst.x;

                for (short k = 0; k < 3; k++) {
                    if (k < const_size)
                        push_cmd("\tTAD #cnst#+%d\n", k);
                    push_cmd("\tDCA I SP\n");
                }
                break;
            case TAC_CALL_PUSH_SYM:
                if (!BP_stored) {
                    add_store_BP(commands);
                    add_commands(output, &commands, &arena, &functions, &consts, &data, &labels, &page_num, &command_num);
                    BP_stored = true;
                }

                if (ir.symbols.data[inst.x].string != NULL) {
                    push_cmd("\tTAD #functn# / %.*s\n", PS(ir.symbols.data[inst.x]));
                    commands.crt_function = true;
                    commands.data.function.name = ir.symbols.data[inst.x];
                    commands.data.function.call = false;
                } else {
                    push_cmd("\tTAD #data#\n");
                    commands.crt_data = true;
                    commands.data.data_name = inst.x;
                }
                push_cmd("\tDCA I SP\n");
                push_cmd("\tDCA I SP\n");
                push_cmd("\tDCA I SP\n");
                break;
            case TAC_LOAD_INT:
                const_size = get_const_size(inst.x);
                commands.crt_const = const_size > 0;
                commands.data.const_number = inst.x;

                for (short k = 0; k < 3; k++) {
                    if (k < const_size)
                        push_cmd("\tTAD #cnst#+%d\n", k);
                    push_cmd("\tDCA REGS+%o\n", r*3 + k);
                }
                break;
            case TAC_LOAD_ARG:
                commands.crt_const = true;
                commands.data.const_number = (5 + inst.x * 3) & 07777;
                
                push_cmd("\tTAD BP\n");
                push_cmd("\tTAD #cnst#\n");
                push_cmd("\tDCA TMP2\n");
                for (short k = 0; k < 3; k++) {
                    push_cmd("\tTAD I TMP2\n");
                    push_cmd("\tDCA REGS+%o\n", r*3 + k);
                }
                break;
            case TAC_LOAD_SYM:
                if (ir.symbols.data[inst.x].string != NULL) {
                    push_cmd("\tJMS #functn# / %.*s\n", PS(ir.symbols.data[inst.x]));
                    commands.crt_function = true;
                    commands.data.function.name = ir.symbols.data[inst.x];
                    commands.data.function.call = false;
                } else {
                    push_cmd("\tTAD #data#\n");
                    commands.crt_data = true;
                    commands.data.data_name = inst.x;
                }
                push_cmd("\tDCA REGS+%o\n", r*3);
                push_cmd("\tDCA REGS+%o\n", r*3 + 1);
                push_cmd("\tDCA REGS+%o\n", r*3 + 2);
                break;
            case TAC_MOV:
                for (short k = 0; k < 3; k++) {
                    push_cmd("\tTAD REGS+%o\n", x*3 + k);
                    push_cmd("\tDCA REGS+%o\n", r*3 + k);
                }
                break;
            case TAC_ADD:
                push_cmd("\tCLA CLL\n");
                for (short k = 0; k < 3; k++) {
                    push_cmd("\tTAD REGS+%o\n", x*3 + k);
                    push_cmd("\tTAD REGS+%o\n", y*3 + k);
                    push_cmd("\tDCA REGS+%o\n", r*3 + k);
                    if (k < 2)
                        push_cmd("\tRAL\n");
                }
                push_cmd("\tCLL\n");
                break;
            
            case TAC_SUB:
                push_cmd("\tCLA CLL\n");
                for (short k = 0; k < 3; k++) {
                    push_cmd("\tTAD REGS+%o\n", y*3 + k);
                    push_cmd("\tCIA\n");
                    push_cmd("\tTAD REGS+%o\n", x*3 + k);
                    push_cmd("\tDCA REGS+%o\n", r*3 + k);
                    if (k < 2)
                        push_cmd("\tRAL\n");
                }
                push_cmd("\tCLL\n");
                break;
            case TAC_ADDI:
            case TAC_SUBI:
                const_size = get_const_size(inst.y * (inst.function == TAC_ADDI ? 1 : -1));
                commands.data.const_number = inst.y * (inst.function == TAC_ADDI ? 1 : -1);
                commands.crt_const = const_size > 0;

                push_cmd("\tCLA CLL\n");
                for (short k = 0; k < 3; k++) {
                    push_cmd("\tTAD REGS+%o\n", x*3 + k);
                    if (k < const_size)
                        push_cmd("\tTAD #cnst#+%d\n", k);
                    push_cmd("\tDCA REGS+%o\n", r*3 + k);
                    if (k < 2)
                        push_cmd("\tRAL\n");
                }
                break;
            case TAC_LT:
                for (short k = 0; k < 3; k++) {
                    push_cmd("\tTAD REGS+%o\n", y*3 + k);
                    if (k == 0) {
                        push_cmd("\tCIA\n");
                        push_cmd("\tCLL\n");
                    } else {
                        push_cmd("\tCMA\n");
                        push_cmd("\tTAD TMP1\n");
                    }
                    push_cmd("\tTAD REGS+%o\n", x*3 + k);
                    if (k < 2) {
                        push_cmd("\tCLA RAL\n");
                        push_cmd("\tDCA TMP1\n");
                    }
                }
                push_cmd("\tDCA TMP1\n");
                push_cmd("\tDCA REGS+%o\n", r*3);
                push_cmd("\tDCA REGS+%o\n", r*3 + 1);
                push_cmd("\tDCA REGS+%o\n", r*3 + 2);
                push_cmd("\tTAD TMP1\n");
                push_cmd("\tSNL\n");
                push_cmd("\t ISZ REGS+%o\n", r*3);
                push_cmd("\tCLA CLL\n");
                break;
            case TAC_LTI:
                commands.data.const_number = -inst.y;
                commands.crt_const = get_const_size(-inst.y) > 0;
                
                push_cmd("\tCLA CLL\n");
                for (short k = 0; k < 3; k++) {
                    push_cmd("\tTAD REGS+%o\n", x*3 + k);
                    push_cmd("\tTAD #cnst#+%d\n", k);
                    if (k < 2)
                        push_cmd("\tCLA RAL\n");
                }
                push_cmd("\tDCA TMP1\n");
                push_cmd("\tDCA REGS+%o\n", r*3);
                push_cmd("\tDCA REGS+%o\n", r*3 + 1);
                push_cmd("\tDCA REGS+%o\n", r*3 + 2);
                push_cmd("\tTAD TMP1\n");
                push_cmd("\tSNL\n");
                push_cmd("\t ISZ REGS+%o\n", r*3);
                push_cmd("\tCLA CLL\n");
                break;
            case TAC_GOTO:
                commands.crt_label = true;
                commands.data.label.name = inst.x;
                commands.data.label.define = false;
                push_cmd("\tJMP #labell#\n");
                break;
            case TAC_BIZ:
                commands.crt_label = true;
                commands.data.label.name = inst.y;
                commands.data.label.define = false;
                for (short k = 0; k < 3; k++) {
                    push_cmd("\tTAD REGS+%o\n", x*3 + k);
                    push_cmd("\tSZA CLA\n");
                    push_cmd("\t JMP .+%o\n", 8 - k*3);
                }
                push_cmd("\tJMP #labell#\n");
                break;
            case TAC_LABEL:
                commands.crt_label = true;
                commands.data.label.name = inst.x;
                commands.data.label.define = true;
                push_cmd("#labell#,");
                break;
            case TAC_RETURN_VAL:
                push_cmd("\tTAD BP\n");
                push_cmd("\tTAD P2\n");
                push_cmd("\tDCA TMP2\n");
                for (short k = 0; k < 3; k++) {
                    push_cmd("\tTAD REGS+%o\n", x*3 + k);
                    push_cmd("\tDCA I TMP2\n");
                }
                break;
            case TAC_RETURN_INT:
                const_size = get_const_size(inst.x);
                commands.data.const_number = inst.x;
                commands.crt_const = const_size > 0;

                push_cmd("\tTAD BP\n");
                push_cmd("\tTAD P2\n");
                push_cmd("\tDCA TMP2\n");
                for (short k = 0; k < 3; k++) {
                    if (k < const_size)
                        push_cmd("\tTAD #cnst#+%d\n", k);
                    push_cmd("\tDCA I TMP2\n");
                }
                break;
            case TAC_NOP:
                break;
            }

            add_commands(output, &commands, &arena, &functions, &consts, &data, &labels, &page_num, &command_num);
        }
        
        if (is_main) {
            push_cmd("\tHLT\n");
        } else {
            push_cmd("\tJMS I EPLGP\n");
            push_cmd("\t%o\n", -temps_count * 3 & 07777);
        }
        add_commands(output, &commands, &arena, &functions, &consts, &data, &labels, &page_num, &command_num);
    }

    fputc('\n', output);

    add_page_end(output, &functions, &consts, &data, &labels, page_num);

    fprintf(output, "DECIMAL\n");
    for (size_t i = 0; i < data.len; i++) {
        PDP8Data dt = data.data[i];
        for (size_t j = 0; j < ir.data.len; j++) {
            StaticVariable var = ir.data.data[j];
            if (dt.name == var.name) {
                fprintf(output, "d%.2zx,", i);
                generate_string(output, var.data, true);
            }
        }
    }
    fprintf(output, "OCTAL\n");

    size_t main_idx;
    if (lookup_function(&functions, S("main"), &main_idx)) {
        fprintf(output, "*200\n");
        fprintf(output, "\tKCC\n");
        fprintf(output, "\tTPC\n");
        fprintf(output, "\tJMP I MAINP\n");
        fprintf(output, "MAINP,\tf%.2zx+1\n", main_idx);
    }

    fprintf(output,"$\n");

    arena_destroy(&arena);
}
