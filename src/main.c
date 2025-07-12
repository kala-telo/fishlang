#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "da.h"
#include "string.h"
#include "lexer.h"
#include "parser.h"
#include "tac.h"

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
} StaticFunction;

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

IR codegen(ASTArr ast) {
    static IR ir = { 0 };
    static size_t current_function = 0;
    static bool in_args = false;
    static uint16_t temp_num = 0;

    // kinda an ungly hack but it keeps arglist less noisy.
    // maybe i'll make codegen into separate function instead l8r
    static bool root_taken = false;
    bool is_root = false;
    if (!root_taken) {
        is_root = true;
        root_taken = true;
    }

    for (int i = 0; i < ast.len; i++) {
        AST node = ast.data[i];
        switch (node.kind) {
        case AST_CALL: {
            bool in_args_orig = in_args;
            in_args = true;
            codegen(node.as.call.args);
            in_args = in_args_orig;
            size_t r = ++temp_num;
            size_t x = ++temp_num;
            da_append(ir.symbols, node.as.call.callee);
            da_append(ir.functions.data[current_function].code,
                      ((TAC32){r, TAC_CALL_SYM, ir.symbols.len-1}));
            if (in_args) {
                da_append(ir.functions.data[current_function].code,
                          ((TAC32){0, TAC_CALL_PUSH, temp_num}));
            }
        } break;
        case AST_FUNCDEF:
            da_append(ir.symbols, node.as.func.name);
            da_append(ir.functions, ((StaticFunction){ir.symbols.len-1}));
            current_function = ir.functions.len - 1;
            codegen(node.as.func.body);
            break;
        case AST_LIST:
            TODO();
        case AST_NAME:
            TODO();
        case AST_STRING:
            da_append(ir.symbols, (String){0});
            da_append(ir.data, ((StaticVariable){ir.symbols.len-1, node.as.string}));
            da_append(ir.functions.data[current_function].code,
                      ((TAC32){++temp_num, TAC_LOAD_SYM, ir.symbols.len - 1}));
            if (in_args) {
                da_append(ir.functions.data[current_function].code,
                          ((TAC32){0, TAC_CALL_PUSH, temp_num}));
            }
            break;
        }
    }
    IR r = ir;
    // reseting the static variable
    if (is_root) memset(&ir, 0, sizeof(ir));
    return r;
}

void free_ir(IR *ir) {
    #define FFREE(x) do { if ((x) != NULL) { free(x); (x) = NULL; } } while(0)
    for (size_t i = 0; i < ir->functions.len; i++)
        FFREE(ir->functions.data[i].code.data);
    FFREE(ir->data.data);
    FFREE(ir->symbols.data);
    FFREE(ir->functions.data);
    #undef FFREE
}

void codegen_powerpc(IR ir, FILE* output) {
    const int call_base = 3;
    const int gpr_base = 14;
    for (size_t i = 0; i < ir.functions.len; i++) {
        TAC32Arr func = ir.functions.data[i].code;
        printf(".global %.*s\n", PS(ir.symbols.data[ir.functions.data[i].name]));
        printf("%.*s:\n", PS(ir.symbols.data[ir.functions.data[i].name]));
        int call_count = 0;
        printf("    stwu 1,-16(1)\n"); // TODO: unhardcode 16-byte allocation
        printf("    mflr 0\n");
        printf("    stw 0, 20(1)\n");
        printf("\n");
        for (size_t j = 0; j < func.len; j++) {
            TAC32 inst = func.data[j];
            switch (inst.function) {
            case TAC_CALL_REG:
                TODO();
                break;
            case TAC_CALL_SYM:
                // if 
                printf("    bl %.*s\n", PS(ir.symbols.data[inst.x]));
                call_count = 0;
                break;
            case TAC_CALL_PUSH:
                printf("    mr %d, %d\n", (call_count++)+call_base, inst.x+gpr_base);
                assert(call_count < 8);
                break;
            case TAC_LOAD_INT:
                if (inst.x < 65536) {
                    printf("    li %d, %d\n", inst.result, inst.x);
                }
                break;
            case TAC_LOAD_SYM:
                if (ir.symbols.data[inst.x].string != NULL) {
                    printf("    lis %d, %.*s@ha\n", inst.result + gpr_base,
                           PS(ir.symbols.data[inst.x]));
                    printf("    ori %d, %d, %.*s@l\n", inst.result + gpr_base,
                           inst.result + gpr_base, PS(ir.symbols.data[inst.x]));
                } else {
                    printf("    lis %d, data_%d@ha\n", inst.result + gpr_base,
                           inst.x);
                    printf("    ori %d, %d, data_%d@l\n",
                           inst.result + gpr_base, inst.result + gpr_base,
                           inst.x);
                }
                break;
            case TAC_ADD:
                printf("    r%d = r%d + r%d\n", inst.result, inst.x,
                       inst.y);
                break;
            }
        }
        printf("\n");
        printf("    lwz 0,20(1)\n");
        printf("    mtlr 0\n");
        printf("    blr\n");
    }
    for (size_t i = 0; i < ir.data.len; i++) {
        StaticVariable var = ir.data.data[i];
        String name = ir.symbols.data[var.name];
        printf("data_");
        if (name.string != NULL)
            printf("%.*s", PS(name));
        else
            printf("%zu", var.name);
        printf(": .byte ");
        for (size_t i = 0; i < var.data.length; i++) {
            printf("%d, ", var.data.string[i]);
        }
        printf("0\n");
    }
    printf(".section    .note.GNU-stack,\"\",@progbits\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file.fsh>\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (f == NULL) {
        fprintf(stderr, "Couldn't open file %s\n", argv[1]);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    int size = ftell(f);
    fseek(f, 0, SEEK_SET);

    String str = { 0 };
    str.string = malloc(size*sizeof(char));
    str.length = size;

    fread(str.string, 1, size, f);
    fclose(f);

    Lexer lex = {
        // -1 for \0, though it still must be in memory for
        // lexer to not read into unallocated memory
        .length = size-1, 
        .position = str.string,
    };

    ASTArr body = { 0 };
    parse(&lex, &body);

    // dump_ast(body, 0);
    IR ir = codegen(body);
    for (size_t i = 0; i < ir.functions.len; i++)
        foldVRegisters(ir.functions.data[i].code);
    codegen_powerpc(ir, stdout);
    free_ir(&ir);

    // it's not like you really need to free it
    // but i wanted to make ASAN and valgrind happy
    free_ast(&body);
    free(body.data);
    free(str.string);
}
