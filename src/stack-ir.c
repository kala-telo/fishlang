#include "arena.h"
#include "parser.h"
#include "todo.h"
#include "da.h"

typedef struct {
    union {
        bool boolean;
        struct {
            String name;
        } func;
        struct {
            // 4 * 64 = 256
            int64_t data[4];
            uint8_t len;
        } integer;
        int64_t comptime_int;
        String string;
    } as;
    enum {
        SIR_VAL_NOTHING,
        SIR_VAL_COMP_INT,
        SIR_VAL_FUNC,
        SIR_VAL_INT,
        SIR_VAL_BOOL,
        SIR_VAL_STRING,
    } kind;
} SIRValue;

typedef struct {
    enum {
        SIR_OP_PUSH,
        SIR_OP_ADD,
        SIR_OP_SUB,
        SIR_OP_LT,
        SIR_OP_GT,
        SIR_OP_BLOCK_BEGIN,
        SIR_OP_BLOCK_END,
        SIR_OP_IF,
        SIR_OP_CALL,
    } op;
    SIRValue arg;
} SIRInst;

typedef struct {
    SIRInst *data;
    size_t len, capacity;
} SIRFunction;

typedef struct {
    struct {
        SIRFunction *data;
        size_t len, capacity;
    } functions;
    SIRFunction global;
    SIRFunction *cur_function;
} StackIR;

static void codegen(StackIR *sir, Arena *arena, ASTArr ast) {
    if (sir->cur_function == NULL)
        sir->cur_function = &sir->global;
    for (size_t i = 0; i < ast.len; i++) {
        AST node = ast.data[i];
        switch (node.kind) {
        case AST_EXTERN:
            TODO();
            break;
        case AST_CALL:
            if (string_eq(node.as.call.callee, S("+"))) {
                codegen(sir, arena, node.as.call.args);
                if (node.as.call.args.len < 2)
                    TODO();
                for (size_t j = 0; j < node.as.call.args.len; j++) {
                    da_append(arena, *sir->cur_function, (SIRInst){SIR_OP_ADD});
                }
            } else if (string_eq(node.as.call.callee, S("-"))) {
                codegen(sir, arena, node.as.call.args);
                if (node.as.call.args.len != 2)
                    TODO();
                da_append(arena, *sir->cur_function, (SIRInst){SIR_OP_SUB});
            } else if (string_eq(node.as.call.callee, S("<")) ||
                       string_eq(node.as.call.callee, S(">"))) {
                codegen(sir, arena, node.as.call.args);
                if (node.as.call.args.len != 2)
                    TODO();
                if (string_eq(node.as.call.callee, S("<"))) {
                    da_append(arena, *sir->cur_function, (SIRInst){SIR_OP_LT});
                } else {
                    da_append(arena, *sir->cur_function, (SIRInst){SIR_OP_GT});
                }
            } else if (string_eq(node.as.call.callee, S("if"))) {
                if (node.as.call.args.len != 3)
                    TODO();
                // false
                da_append(arena, *sir->cur_function, (SIRInst){SIR_OP_BLOCK_BEGIN});
                codegen(sir, arena, (ASTArr){&node.as.call.args.data[2], 1, 0});
                da_append(arena, *sir->cur_function, (SIRInst){SIR_OP_BLOCK_END});

                // if true
                da_append(arena, *sir->cur_function, (SIRInst){SIR_OP_BLOCK_BEGIN});
                codegen(sir, arena, (ASTArr){&node.as.call.args.data[1], 1, 0});
                da_append(arena, *sir->cur_function, (SIRInst){SIR_OP_BLOCK_END});

                // condition
                codegen(sir, arena, (ASTArr){&node.as.call.args.data[0], 1, 0});

                da_append(arena, *sir->cur_function, (SIRInst){SIR_OP_IF});
            } else {
                codegen(sir, arena, node.as.call.args);
                da_append(arena, *sir->cur_function,
                          ((SIRInst){SIR_OP_PUSH, (SIRValue){
                              .as.func.name = node.as.call.callee,
                              .kind = SIR_VAL_FUNC,
                          }}));
                da_append(arena, *sir->cur_function, ((SIRInst){SIR_OP_CALL, (SIRValue){
                              .as.comptime_int = node.as.call.args.len,
                          }}));
            }
            break;
        case AST_DEF:
        case AST_VARDEF:
            // supposedly we add for blocks the ability to declare names inside
            TODO();
            codegen(sir, arena, node.as.def.body);
            break;
        case AST_FUNC: {
            SIRFunction *prev_func = sir->cur_function;
            da_append(arena, sir->functions, (SIRFunction){});
            sir->cur_function = &da_last(sir->functions);
            codegen(sir, arena, node.as.func.body);
            sir->cur_function = prev_func;
        } break;
        case AST_LIST:
            TODO();
            break;
        case AST_NAME:
            TODO();
            break;
        case AST_NUMBER:
            TODO(); // add data too
            da_append(arena, *sir->cur_function,
                      ((SIRInst){.op = SIR_OP_PUSH,
                                 .arg.as.integer.len = 32,
                                 .arg.kind = SIR_VAL_INT}));
            break;
        case AST_BOOL:
            da_append(arena, *sir->cur_function,
                      ((SIRInst){.op = SIR_OP_PUSH,
                                 .arg.as.boolean = node.as.boolean,
                                 .arg.kind = SIR_VAL_BOOL}));
            break;
        case AST_STRING:
            da_append(arena, *sir->cur_function,
                      ((SIRInst){.op = SIR_OP_PUSH,
                                 .arg.kind = SIR_VAL_STRING,
                                 .arg.as.string = node.as.string}));
            break;
        }
    }
}
