#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "arena.h"
#include "codegen.h"
#include "da.h"
#include "parser.h"
#include "string.h"
#include "todo.h"

void generate_string(FILE *output, String str) {
    bool escape = false;
    for (int i = 0; i < str.length; i++) {
        char c = str.string[i];
        if (escape) {
            switch (c) {
            case 'n':
                fprintf(output, "10, ");
                escape = false;
                break;
            case '"':
                fprintf(output, "34, ");
                escape = false;
                break;
            case 'r':
                fprintf(output, "13, ");
                escape = false;
                break;
            case 't':
                fprintf(output, "9, ");
                escape = false;
                break;
            case 'b':
                fprintf(output, "8, ");
                escape = false;
                break;
            case 'a':
                fprintf(output, "7, ");
                escape = false;
                break;
            case '0':
                fprintf(output, "0, ");
                escape = false;
                break;
            case '\\':
                fprintf(output, "92, ");
                escape = false;
                break;
            default:
                TODO();
            }
        } else {
            if (str.string[i] == '\\') {
                escape = true;
            } else {
                fprintf(output, "%d, ", c);
            }
        }
    }
    fprintf(output, "0");
}

IR codegen(Arena *arena, ASTArr ast, CodeGenCTX *ctx) {
    for (size_t i = 0; i < ast.len; i++) {
        AST node = ast.data[i];
        size_t args_stack_len = ctx->args_stack.len;
        switch (node.kind) {
        case AST_EXTERN:
            da_append(arena, ctx->ir.symbols, node.as.external.name);
            hms_put(arena, ctx->global_symbols, node.as.external.name, ctx->ir.symbols.len-1);
            // TODO
            break;
        case AST_CALL:
            assert(ctx->current_function != NULL);
            if (string_eq(node.as.call.callee, S("+"))) {
                codegen(arena, node.as.call.args, ctx);
                if (node.as.call.args.len != 2)
                    TODO();
                da_append(arena, 
                    ctx->current_function->code,
                    ((TAC32){++ctx->temp_num, TAC_ADD, da_pop(ctx->args_stack),
                             da_pop(ctx->args_stack)}));
                da_append(arena, ctx->args_stack, ctx->temp_num);
            } else if (string_eq(node.as.call.callee, S("-"))) {
                codegen(arena, node.as.call.args, ctx);
                if (node.as.call.args.len != 2)
                    TODO();
                uint16_t y = da_pop(ctx->args_stack);
                uint16_t x = da_pop(ctx->args_stack);
                da_append(arena, ctx->current_function->code,
                          ((TAC32){++ctx->temp_num, TAC_SUB, x, y}));
                da_append(arena, ctx->args_stack, ctx->temp_num);
            } else if (string_eq(node.as.call.callee, S("<"))) {
                codegen(arena, node.as.call.args, ctx);
                if (node.as.call.args.len != 2)
                    TODO();
                uint16_t y = da_pop(ctx->args_stack);
                uint16_t x = da_pop(ctx->args_stack);
                da_append(arena, ctx->current_function->code,
                          ((TAC32){++ctx->temp_num, TAC_LT, x, y}));
                da_append(arena, ctx->args_stack, ctx->temp_num);
            } else if (string_eq(node.as.call.callee, S("if"))) {
                if (node.as.call.args.len != 3)
                    TODO();
                uint16_t branch_false = ++ctx->branch_id;
                uint16_t branch_exit = ++ctx->branch_id;

                // condition
                codegen(arena, (ASTArr){&node.as.call.args.data[0], 1, 0}, ctx);
                da_append(arena, ctx->current_function->code,
                          ((TAC32){0, TAC_BIZ, da_pop(ctx->args_stack),
                                   branch_false}));

                uint32_t result = ++ctx->temp_num;
                // if true
                codegen(arena, (ASTArr){&node.as.call.args.data[1], 1, 0}, ctx);
                uint32_t result_1 = da_pop(ctx->args_stack);
                da_append(arena, ctx->current_function->code,
                          ((TAC32){result, TAC_MOV, result_1, 0}));
                da_append(arena, ctx->current_function->code,
                          ((TAC32){0, TAC_GOTO, branch_exit, 0}));

                // false
                da_append(arena, ctx->current_function->code,
                          ((TAC32){0, TAC_LABEL, branch_false, 0}));
                codegen(arena, (ASTArr){&node.as.call.args.data[2], 1, 0}, ctx);
                uint32_t result_2 = da_pop(ctx->args_stack);
                da_append(arena, ctx->current_function->code,
                          ((TAC32){result, TAC_MOV, result_2, 0}));

                // epilouege
                da_append(arena, ctx->current_function->code,
                          ((TAC32){0, TAC_LABEL, branch_exit, 0}));
                da_append(arena, ctx->args_stack, result);
            } else {
                codegen(arena, node.as.call.args, ctx);
                size_t base = ctx->args_stack.len - node.as.call.args.len;
                for (size_t i = 0; i < node.as.call.args.len; i++) {
                    da_append(arena, ctx->current_function->code,
                              ((TAC32){0, TAC_CALL_PUSH,
                                       ctx->args_stack.data[base + i], 0}));
                    ctx->args_stack.len--;
                }
                da_append(arena, ctx->ir.symbols, node.as.call.callee);
                bool found;
                uintptr_t f = 0;
                hms_get(f, ctx->global_symbols, node.as.call.callee, found);
                if (!found) {
                    hms_get(f, ctx->variables, node.as.call.callee, found);
                    assert(found);
                    da_append(arena, ctx->current_function->code,
                              ((TAC32){++ctx->temp_num, TAC_CALL_REG,
                                       (uint32_t)f, 0}));
                } else {
                    da_append(arena, ctx->current_function->code,
                              ((TAC32){++ctx->temp_num, TAC_CALL_SYM,
                                       ctx->ir.symbols.len - 1, 0}));
                }
                da_append(arena, ctx->args_stack, ctx->temp_num);
            }
            break;
        case AST_DEF:
            da_append(arena, ctx->ir.symbols, node.as.def.name);
            hms_put(arena, ctx->global_symbols, node.as.def.name,
                  ctx->ir.symbols.len - 1);
            codegen(arena, node.as.def.body, ctx);
            break;
        case AST_FUNC: {
            // assert(ctx->args_stack.len == 0);
            StaticFunction *prev_func = ctx->current_function;
            da_append(arena, ctx->ir.functions,
                      ((StaticFunction){ctx->ir.symbols.len - 1, {0}, 0}));
            ctx->current_function = &da_last(ctx->ir.functions);
            for (size_t j = 0; j < node.as.func.args.len; j++) {
                da_append(arena, ctx->current_function->code,
                          ((TAC32){++ctx->temp_num, TAC_LOAD_ARG, j, 0}));
                hms_put(arena, ctx->variables, node.as.func.args.data[j].name,
                        ctx->temp_num);
            }
            codegen(arena, node.as.func.body, ctx);
            for (size_t j = 0; j < node.as.func.args.len; j++) {
                hms_rem(ctx->variables, node.as.func.args.data[j].name);
            }
            da_append(arena, ctx->current_function->code,
                      ((TAC32){0, TAC_RETURN_VAL, da_pop(ctx->args_stack), 0}));
            ctx->current_function = prev_func;
            // assert(ctx->args_stack.len == 0);
        } break;
        case AST_VARDEF:
            assert(ctx->current_function != NULL);
            for (size_t j = 0; j < node.as.var.variables.len; j++) {
                codegen(arena, node.as.var.variables.data[j].value, ctx);
                hms_put(arena, ctx->variables,
                      node.as.var.variables.data[j].definition.name,
                      da_pop(ctx->args_stack));
            }
            codegen(arena, node.as.var.body, ctx);
            for (size_t j = 0; j < node.as.var.variables.len; j++) {
                codegen(arena, node.as.var.variables.data[j].value, ctx);
                hms_rem(ctx->variables,
                      node.as.var.variables.data[j].definition.name);
            }
            da_append(arena, ctx->current_function->code,
                      ((TAC32){0, TAC_RETURN_VAL, da_pop(ctx->args_stack), 0}));
            break;
        case AST_LIST:
            assert(ctx->current_function != NULL);
            TODO();
            break;
        case AST_NAME: {
            assert(ctx->current_function != NULL);
            uintptr_t var = 0;
            bool found;
            hms_get(var, ctx->variables, node.as.name, found);
            if (found) {
                da_append(arena, ctx->args_stack, var);
                break;
            }
            uintptr_t symbol = 0;
            hms_get(symbol, ctx->global_symbols, node.as.name, found);
            assert(found);
            da_append(arena, ctx->current_function->code,
                      ((TAC32){++ctx->temp_num, TAC_LOAD_SYM, symbol, 0}));
            da_append(arena, ctx->args_stack, ctx->temp_num);
        } break;
        case AST_NUMBER:
            assert(ctx->current_function != NULL);
            da_append(arena, 
                ctx->current_function->code,
                ((TAC32){++ctx->temp_num, TAC_LOAD_INT, node.as.number, 0}));
            da_append(arena, ctx->args_stack, ctx->temp_num);
            break;
        case AST_BOOL:
            assert(ctx->current_function != NULL);
            da_append(arena, 
                ctx->current_function->code,
                ((TAC32){++ctx->temp_num, TAC_LOAD_INT, node.as.boolean ? 0 : 1, 0}));
            da_append(arena, ctx->args_stack, ctx->temp_num);
            break;
        case AST_STRING:
            assert(ctx->current_function != NULL);
            da_append(arena, ctx->ir.symbols, (String){0});
            da_append(arena, ctx->ir.data, ((StaticVariable){ctx->ir.symbols.len - 1,
                                                      node.as.string}));
            da_append(arena, ctx->current_function->code,
                      ((TAC32){++ctx->temp_num, TAC_LOAD_SYM,
                               ctx->ir.symbols.len - 1, 0}));
            da_append(arena, ctx->args_stack, ctx->temp_num);
            break;
        }
        if (ctx->args_stack.len > args_stack_len) {
            ctx->args_stack.len = args_stack_len + 1;
        } else {
            da_append(arena, ctx->args_stack, 0);
            assert(ctx->args_stack.len > args_stack_len);
        }
    }
    return ctx->ir;
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
                fprintf(output, "    r%d = r%d()\n", r, x);
                break;
            case TAC_CALL_SYM:
                fprintf(output, "    r%d = %.*s()\n", r,
                        PS(ir.symbols.data[x]));
                call_count = 0;
                break;
            case TAC_CALL_PUSH:
                fprintf(output, "    c%d = r%d\n", (call_count++), x);
                break;
            case TAC_CALL_PUSH_INT:
                fprintf(output, "    c%d = %d\n", (call_count++), x);
                break;
            case TAC_CALL_PUSH_SYM:
                if (ir.symbols.data[x].string != NULL) {
                    fprintf(output, "    c%d = [%.*s]\n", r,
                            PS(ir.symbols.data[x]));
                } else {
                    fprintf(output, "    c%d = [data_%d]\n", (call_count++), x);
                }
                break;
            case TAC_LOAD_INT:
                fprintf(output, "    r%d = %d\n", r, x);
                break;
            case TAC_LOAD_ARG:
                fprintf(output, "    r%d = arg%d\n", r, x);
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
            case TAC_ADDI:
                fprintf(output, "    r%d = r%d + %d\n", r, x, y);
                break;
            case TAC_SUB:
                fprintf(output, "    r%d = r%d - r%d\n", r, x, y);
                break;
            case TAC_SUBI:
                fprintf(output, "    r%d = r%d - %d\n", r, x, y);
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
            case TAC_RETURN_VAL:
                fprintf(output, "    ret = r%d\n", x);
                break;
            case TAC_RETURN_INT:
                fprintf(output, "    ret = %d\n", x);
                break;
            case TAC_NOP:
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
            uint32_t r = inst.result + gpr_base, x = inst.x + gpr_base,
                     y = inst.y + gpr_base;
            switch (inst.function) {
            case TAC_CALL_REG:
                fprintf(output, "    mtctr %d\n", x);
                fprintf(output, "    bctrl\n");
                fprintf(output, "    mr %d, 3\n", r);
                break;
            case TAC_CALL_SYM:
                fprintf(output, "    bl %.*s\n", PS(ir.symbols.data[inst.x]));
                fprintf(output, "    mr %d, 3\n", r);
                call_count = 0;
                break;
            case TAC_CALL_PUSH:
                fprintf(output, "    mr %d, %d\n", (call_count++) + call_base,
                        x);
                assert(call_count < 8);
                break;
            case TAC_CALL_PUSH_INT:
                if (x <= 0xFFFF) {
                    fprintf(output, "    li %d, %d\n", (call_count++)+call_base, inst.x);
                } else {
                    fprintf(output, "    li %d, %d\n", call_count+call_base, inst.x & 0xFFFF);
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
                if (x <= 0xFFFF) {
                    fprintf(output, "    li %d, %d\n", r, inst.x);
                } else {
                    fprintf(output, "    li %d, %d\n", r, inst.x & 0xFFFF);
                    fprintf(output, "    ori %d, %d\n", r, inst.x >> 16);
                }
                break;
            case TAC_LOAD_ARG:
                fprintf(output, "    mr %d, %d\n", r, inst.x + call_base);
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
            case TAC_ADDI:
                fprintf(output, "    addi %d, %d, %d\n", r, x, inst.y);
                break;
            case TAC_SUB:
                fprintf(output, "    sub %d, %d, %d\n", r, x, y);
                break;
            case TAC_SUBI:
                fprintf(output, "    subi %d, %d, %d\n", r, x, inst.y);
                break;
            case TAC_LT:
                fprintf(output, "    cmpw %%cr0, %d, %d\n", y, x);
                fprintf(output, "    mfcr %d\n", r);
                fprintf(output, "    rlwinm %d, %d, 2, 31, 31\n", r, r);
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
                if (x <= 0xFFFF) {
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
        generate_string(output, var.data);
        fprintf(output, "\n");
    }
    fprintf(output, ".section .note.GNU-stack,\"\",@progbits\n");
}
