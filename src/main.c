#include "da.h"
#include "lexer.h"
#include "parser.h"
#include "string.h"
#include "tac.h"
#include "todo.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    uint16_t temps_count;
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

typedef struct {
    IR ir;
    size_t current_function;
    uint16_t temp_num;
    uint16_t branch_id;
    struct {
        uint32_t *data;
        size_t len, capacity;
    } args_stack;
} CodeGenCTX;

void free_ctx(CodeGenCTX *ctx) {
#define FREE(x)                                                                \
    do {                                                                       \
        if ((x) != NULL) {                                                     \
            free(x);                                                           \
            (x) = NULL;                                                        \
        }                                                                      \
    } while (0)
    FREE(ctx->args_stack.data);
    FREE(ctx);
#undef FREE
}

IR codegen(ASTArr ast, CodeGenCTX *ctx) {
    for (size_t i = 0; i < ast.len; i++) {
        AST node = ast.data[i];
        size_t args_stack_len = ctx->args_stack.len;
        switch (node.kind) {
        case AST_CALL:
            if (string_eq(node.as.call.callee, S("+"))) {
                codegen(node.as.call.args, ctx);
                if (node.as.call.args.len != 2)
                    TODO();
                da_append(
                    ctx->ir.functions.data[ctx->current_function].code,
                    ((TAC32){++ctx->temp_num, TAC_ADD, da_pop(ctx->args_stack),
                             da_pop(ctx->args_stack)}));
            } else if (string_eq(node.as.call.callee, S("<"))) {
                codegen(node.as.call.args, ctx);
                if (node.as.call.args.len != 2)
                    TODO();
                da_append(
                    ctx->ir.functions.data[ctx->current_function].code,
                    ((TAC32){++ctx->temp_num, TAC_LT, da_pop(ctx->args_stack),
                             da_pop(ctx->args_stack)}));
            } else if (string_eq(node.as.call.callee, S("if"))) {
                if (node.as.call.args.len != 3)
                    TODO();
                uint16_t branch_false = ++ctx->branch_id;
                uint16_t branch_exit = ++ctx->branch_id;

                // condition
                codegen((ASTArr){&node.as.call.args.data[0], 1, 0}, ctx);
                da_append(ctx->ir.functions.data[ctx->current_function].code,
                          ((TAC32){0, TAC_BIZ, da_pop(ctx->args_stack),
                                   branch_false}));

                uint32_t result = ++ctx->temp_num;
                // if true
                codegen((ASTArr){&node.as.call.args.data[1], 1, 0}, ctx);
                da_append(ctx->ir.functions.data[ctx->current_function].code,
                          ((TAC32){0, TAC_GOTO, branch_exit, 0}));
                uint32_t result_1 = da_pop(ctx->args_stack);
                da_append(ctx->ir.functions.data[ctx->current_function].code,
                          ((TAC32){result, TAC_MOV, result_1, 0}));

                // false
                da_append(ctx->ir.functions.data[ctx->current_function].code,
                          ((TAC32){0, TAC_LABEL, branch_false, 0}));
                codegen((ASTArr){&node.as.call.args.data[2], 1, 0}, ctx);
                uint32_t result_2 = da_pop(ctx->args_stack);
                da_append(ctx->ir.functions.data[ctx->current_function].code,
                          ((TAC32){result, TAC_MOV, result_2, 0}));

                // epilouege
                da_append(ctx->ir.functions.data[ctx->current_function].code,
                          ((TAC32){0, TAC_LABEL, branch_exit, 0}));
                da_append(ctx->args_stack, result);
            } else {
                codegen(node.as.call.args, ctx);
                for (size_t i = 0; i < node.as.call.args.len; i++) {
                    size_t base = ctx->args_stack.len - node.as.call.args.len;
                    da_append(
                        ctx->ir.functions.data[ctx->current_function].code,
                        ((TAC32){0, TAC_CALL_PUSH,
                                 ctx->args_stack.data[base + i], 0}));
                }
                da_append(ctx->ir.symbols, node.as.call.callee);
                da_append(ctx->ir.functions.data[ctx->current_function].code,
                          ((TAC32){++ctx->temp_num, TAC_CALL_SYM,
                                   ctx->ir.symbols.len - 1, 0}));
            }
            da_append(ctx->args_stack, ctx->temp_num);
            break;
        case AST_FUNCDEF:
            da_append(ctx->ir.symbols, node.as.func.name);
            da_append(ctx->ir.functions,
                      ((StaticFunction){ctx->ir.symbols.len - 1, {0}, 0}));
            ctx->current_function = ctx->ir.functions.len - 1;
            codegen(node.as.func.body, ctx);
            break;
        case AST_LIST:
            TODO();
            break;
        case AST_NAME:
            TODO();
            break;
        case AST_NUMBER:
            da_append(
                ctx->ir.functions.data[ctx->current_function].code,
                ((TAC32){++ctx->temp_num, TAC_LOAD_INT, node.as.number, 0}));
            da_append(ctx->args_stack, ctx->temp_num);
            break;
        case AST_STRING:
            da_append(ctx->ir.symbols, (String){0});
            da_append(ctx->ir.data, ((StaticVariable){ctx->ir.symbols.len - 1,
                                                      node.as.string}));
            da_append(ctx->ir.functions.data[ctx->current_function].code,
                      ((TAC32){++ctx->temp_num, TAC_LOAD_SYM,
                               ctx->ir.symbols.len - 1, 0}));
            da_append(ctx->args_stack, ctx->temp_num);
            break;
        }
        if (ctx->args_stack.len > args_stack_len) {
            ctx->args_stack.len = args_stack_len+1;
        } else {
            TODO();
        }
    }
    return ctx->ir;
}

void free_ir(IR *ir) {
#define FREE(x)                                                                \
    do {                                                                       \
        if ((x) != NULL) {                                                     \
            free(x);                                                           \
            (x) = NULL;                                                        \
        }                                                                      \
    } while (0)
    for (size_t i = 0; i < ir->functions.len; i++)
        FREE(ir->functions.data[i].code.data);
    FREE(ir->data.data);
    FREE(ir->symbols.data);
    FREE(ir->functions.data);
#undef FREE
}

void codegen_debug(IR ir, FILE *output) {
    for (size_t i = 0; i < ir.functions.len; i++) {
        TAC32Arr func = ir.functions.data[i].code;
        fprintf(output, "%.*s {\n",
                PS(ir.symbols.data[ir.functions.data[i].name]));
        int call_count = 0;
        for (size_t j = 0; j < func.len; j++) {
            TAC32 inst = func.data[j];
            uint32_t r = inst.result, x = inst.x, y = inst.y;
            switch (inst.function) {
            case TAC_CALL_REG:
                TODO();
                break;
            case TAC_CALL_SYM:
                fprintf(output, "    %.*s()\n", PS(ir.symbols.data[x]));
                call_count = 0;
                break;
            case TAC_CALL_PUSH:
                fprintf(output, "    c%d = r%d\n", (call_count++), x);
                break;
            case TAC_LOAD_INT:
                fprintf(output, "    r%d = %d\n", r, x);
                break;
            case TAC_LOAD_SYM:
                if (ir.symbols.data[x].string != NULL) {
                    fprintf(output, "    r%d = [%.*s]\n", r,
                            PS(ir.symbols.data[x]));
                } else {
                    fprintf(output, "    r%d = [data_%d]\n", r, x);
                }
                break;
            case TAC_MOV:
                fprintf(output, "    r%d = r%d\n", r, x);
                break;
            case TAC_ADD:
                fprintf(output, "    r%d = r%d + r%d\n", r, x, y);
                break;
            case TAC_LT:
                fprintf(output, "    r%d = r%d < r%d\n", r, x, y);
                break;
            case TAC_GOTO:
                fprintf(output, "    b label_%d\n", x);
                break;
            case TAC_BIZ:
                fprintf(output, "    biz r%d, label_%d\n", x, y);
                break;
            case TAC_LABEL:
                fprintf(output, "label_%d:\n", x);
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
        bool escape = false;
        for (int i = 0; i < var.data.length; i++) {
            char c = var.data.string[i];
            if (escape) {
                switch (c) {
                case 'n':
                    fprintf(output, "10, ");
                    escape = false;
                    break;
                default:
                    TODO();
                }
            } else {
                if (var.data.string[i] == '\\') {
                    escape = true;
                } else {
                    fprintf(output, "%d, ", var.data.string[i]);
                }
            }
        }
        fprintf(output, "0 }\n");
    }
}

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
        if (ir.functions.data[i].temps_count > 19)
            TODO();
        fprintf(output, ".global %.*s\n",
                PS(ir.symbols.data[ir.functions.data[i].name]));
        fprintf(output, "%.*s:\n",
                PS(ir.symbols.data[ir.functions.data[i].name]));
        int call_count = 0;
        fprintf(output,
                "    stwu 1,-16(1)\n"); // TODO: unhardcode 16-byte allocation
        fprintf(output, "    mflr 0\n");
        fprintf(output, "    stw 0, 20(1)\n");
        fprintf(output, "\n");
        for (size_t j = 0; j < func.len; j++) {
            TAC32 inst = func.data[j];
            uint32_t r = inst.result + gpr_base, x = inst.x + gpr_base,
                     y = inst.y + gpr_base;
            switch (inst.function) {
            case TAC_CALL_REG:
                TODO();
                break;
            case TAC_CALL_SYM:
                fprintf(output, "    bl %.*s\n", PS(ir.symbols.data[inst.x]));
                call_count = 0;
                break;
            case TAC_CALL_PUSH:
                fprintf(output, "    mr %d, %d\n", (call_count++) + call_base,
                        x);
                assert(call_count < 8);
                break;
            case TAC_LOAD_INT:
                if (x < 65536) {
                    fprintf(output, "    li %d, %d\n", r, inst.x);
                } else {
                    TODO();
                }
                break;
            case TAC_LOAD_SYM:
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
                fprintf(output, "    mr %d, %d\n", r, x);
                break;
            case TAC_ADD:
                fprintf(output, "    add %d, %d, %d\n", r, x, y);
                break;
            case TAC_LT:
                fprintf(output, "    cmpw %%cr0, %d, %d\n", x, y);
                fprintf(output, "    mfcr %d\n", r);
                fprintf(output, "    rlwinm %d, %d, 2, 31, 1\n", r, r);
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
            }
        }
        fprintf(output, "\n");
        fprintf(output, "    lwz 0,20(1)\n");
        fprintf(output, "    mtlr 0\n");
        fprintf(output, "    addi 1,1,16\n");
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
        bool escape = false;
        for (int i = 0; i < var.data.length; i++) {
            char c = var.data.string[i];
            if (escape) {
                switch (c) {
                case 'n':
                    fprintf(output, "10, ");
                    escape = false;
                    break;
                default:
                    TODO();
                }
            } else {
                if (var.data.string[i] == '\\') {
                    escape = true;
                } else {
                    fprintf(output, "%d, ", var.data.string[i]);
                }
            }
        }
        fprintf(output, "0\n");
    }
    fprintf(output, ".section    .note.GNU-stack,\"\",@progbits\n");
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

    String str = {0};
    str.string = malloc(size * sizeof(char));
    str.length = size;

    fread(str.string, 1, size, f);
    fclose(f);

    Lexer lex = {
        // -1 for \0, though it still must be in memory for
        // lexer to not read into unallocated memory
        .length = size - 1,
        .position = str.string,
    };

    ASTArr body = {0};
    while (peek_token(&lex).kind != LEX_END)
        parse(&lex, &body);

    // dump_ast(body, 0);
    CodeGenCTX *ctx = calloc(1, sizeof(CodeGenCTX));
    IR ir = codegen(body, ctx);
    free_ctx(ctx);

    for (size_t i = 0; i < ir.functions.len; i++)
        ir.functions.data[i].temps_count =
            fold_temporaries(ir.functions.data[i].code);
    codegen_powerpc(ir, stdout);
    // codegen_debug(ir, stdout);

    // it's not like you really need to free it
    // but i wanted to make ASAN and valgrind happy
    free_ir(&ir);
    free_ast(&body);
    free(body.data);
    free(str.string);
}
