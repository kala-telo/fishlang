#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "codegen.h"
#include "da.h"
#include "parser.h"
#include "string.h"
#include "todo.h"


void generate_string(FILE *output, String str, bool pdp8) {
    bool escape = false;
    for (int i = 0; i < str.length; i++) {
        char c = str.string[i];
        if (escape) {
            if (pdp8)
                fputc('\t', output);
            switch (c) {
            case 'n':
                fprintf(output, "10");
                escape = false;
                break;
            case '"':
                fprintf(output, "34");
                escape = false;
                break;
            case 'r':
                fprintf(output, "13");
                escape = false;
                break;
            case 't':
                fprintf(output, "9");
                escape = false;
                break;
            case 'b':
                fprintf(output, "8");
                escape = false;
                break;
            case 'a':
                fprintf(output, "7");
                escape = false;
                break;
            case '0':
                fprintf(output, "0");
                escape = false;
                break;
            case '\\':
                fprintf(output, "92");
                escape = false;
                break;
            default:
                TODO();
            }
            if (pdp8)
                fputc('\n', output);
            else
                fputs(", ", output);
        } else {
            if (str.string[i] == '\\') {
                escape = true;
            } else {
                if (pdp8)
                    fprintf(output, "\t%d\n", c & 07777);
                else
                    fprintf(output, "%d, ", c);
            }
        }
    }
    if (pdp8)
        fprintf(output, "\t0\n");
    else
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
                if (node.as.call.args.len < 2)
                    TODO();
                size_t j = node.as.call.args.len-2;
                do {
                    da_append(arena, ctx->current_function->code,
                              ((TAC32){++ctx->temp_num, TAC_ADD,
                                       da_pop(ctx->args_stack),
                                       da_pop(ctx->args_stack)}));
                    da_append(arena, ctx->args_stack, ctx->temp_num);
                } while (j-- > 0);
            } else if (string_eq(node.as.call.callee, S("-"))) {
                codegen(arena, node.as.call.args, ctx);
                if (node.as.call.args.len != 2)
                    TODO();
                uint16_t y = da_pop(ctx->args_stack);
                uint16_t x = da_pop(ctx->args_stack);
                da_append(arena, ctx->current_function->code,
                          ((TAC32){++ctx->temp_num, TAC_SUB, x, y}));
                da_append(arena, ctx->args_stack, ctx->temp_num);
            } else if (string_eq(node.as.call.callee, S("<")) ||
                       string_eq(node.as.call.callee, S(">"))) {
                codegen(arena, node.as.call.args, ctx);
                if (node.as.call.args.len != 2)
                    TODO();
                uint16_t y;
                uint16_t x;
                if (string_eq(node.as.call.callee, S("<"))) {
                    y = da_pop(ctx->args_stack);
                    x = da_pop(ctx->args_stack);
                } else {
                    x = da_pop(ctx->args_stack);
                    y = da_pop(ctx->args_stack);
                }
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
                          ((TAC32){0, TAC_GOTO, branch_exit, 0}));

                // false
                da_append(arena, ctx->current_function->code,
                          ((TAC32){0, TAC_LABEL, branch_false, 0}));
                codegen(arena, (ASTArr){&node.as.call.args.data[2], 1, 0}, ctx);
                uint32_t result_2 = da_pop(ctx->args_stack);

                // epilouege
                da_append(arena, ctx->current_function->code,
                          ((TAC32){0, TAC_LABEL, branch_exit, 0}));
                da_append(arena, ctx->current_function->code,
                          ((TAC32){result, TAC_PHI, result_1, result_2}));
                da_append(arena, ctx->args_stack, result);
            } else {
                codegen(arena, node.as.call.args, ctx);
                size_t base = ctx->args_stack.len - node.as.call.args.len;
                size_t len = node.as.call.args.len;
                for (size_t i = len; i --> 0 ;) {
                    da_append(arena, ctx->current_function->code,
                              ((TAC32){0, TAC_CALL_PUSH,
                                       ctx->args_stack.data[base + i], len}));
                    ctx->args_stack.len--;
                }
                da_append(arena, ctx->ir.symbols, node.as.call.callee);
                bool found;
                uintptr_t f = 0;
                hms_get(f, ctx->global_symbols, node.as.call.callee, found);
                if (!found) {
                    hms_get(f, ctx->variables, node.as.call.callee, found);
                    if (!found) {
                        fprintf(stderr, "%.*s\n", PS(node.as.call.callee));
                        TODO();
                    }
                    da_append(arena, ctx->current_function->code,
                              ((TAC32){++ctx->temp_num, TAC_CALL_REG,
                                       (uint32_t)f, len}));
                } else {
                    da_append(arena, ctx->current_function->code,
                              ((TAC32){++ctx->temp_num, TAC_CALL_SYM,
                                       ctx->ir.symbols.len - 1, len}));
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
            if (!node.as.func.ret_type_void)
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
                ((TAC32){++ctx->temp_num, TAC_LOAD_INT, node.as.boolean ? 1 : 0, 0}));
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
