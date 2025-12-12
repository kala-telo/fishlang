#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "da.h"
#include "tac.h"
#include "todo.h"

static bool ranges_intersect(int f1, int t1, int f2, int t2) {
    return !(t1 < f2 || t2 < f1);
}

Arity get_arity(TACOp op) {
    switch (op) {
    case TAC_ADD:
    case TAC_SUB:
    case TAC_LT:
    case TAC_PHI:
        return A_BINARY;
    case TAC_CALL_REG:
    case TAC_CALL_PUSH:
    case TAC_RETURN_VAL:
    case TAC_BIZ:
    case TAC_MOV:
    case TAC_ADDI:
    case TAC_SUBI:
    case TAC_LTI:
        return A_UNARY;
    case TAC_RETURN_INT:
    case TAC_LOAD_ARG:
    case TAC_LOAD_SYM:
    case TAC_LOAD_INT:
    case TAC_CALL_PUSH_INT:
    case TAC_CALL_PUSH_SYM:
    case TAC_CALL_SYM:
    case TAC_LABEL:
    case TAC_GOTO:
    case TAC_NOP:
    case TAC_EXIT:
        return A_NULLARY;
    }
    UNREACHABLE();
    return false;
}

// determines is it safe to optimize out the instruction based on its result
static bool ispure(TACOp op) {
    switch (op) {
    case TAC_ADD:
    case TAC_SUB:
    case TAC_LT:
    case TAC_MOV:
    case TAC_ADDI:
    case TAC_SUBI:
    case TAC_NOP:
    case TAC_LOAD_INT:
    case TAC_LOAD_ARG:
    case TAC_LOAD_SYM:
    case TAC_LTI:
    case TAC_PHI:
        return true;
    case TAC_CALL_REG:
    case TAC_CALL_PUSH:
    case TAC_RETURN_VAL:
    case TAC_BIZ:
    case TAC_CALL_PUSH_INT:
    case TAC_CALL_PUSH_SYM:
    case TAC_CALL_SYM:
    case TAC_LABEL:
    case TAC_RETURN_INT:
    case TAC_GOTO:
    case TAC_EXIT:
        return false;
    }
    UNREACHABLE();
    return false;
}

static bool is_set_ret(TACOp op) {
    switch (op) {
    case TAC_ADD:
    case TAC_SUB:
    case TAC_LT:
    case TAC_MOV:
    case TAC_ADDI:
    case TAC_SUBI:
    case TAC_NOP:
    case TAC_LOAD_INT:
    case TAC_LOAD_ARG:
    case TAC_LOAD_SYM:
    case TAC_LTI:
    case TAC_CALL_REG:
    case TAC_CALL_PUSH:
    case TAC_BIZ:
    case TAC_CALL_PUSH_INT:
    case TAC_CALL_PUSH_SYM:
    case TAC_CALL_SYM:
    case TAC_LABEL:
    case TAC_GOTO:
    case TAC_EXIT:
    case TAC_PHI:
        return false;
    case TAC_RETURN_VAL:
    case TAC_RETURN_INT:
        return true;
    }
    UNREACHABLE();
    return false;
}

void find_first_last_usage(TAC32Arr tac, int *first, int *last) {
    for (size_t i = 0; i < tac.len; i++) {
        if (tac.data[i].result != 0) {
            last[tac.data[i].result] = i;
            if (first[tac.data[i].result] == -1)
                first[tac.data[i].result] = i;
        }
        switch (get_arity(tac.data[i].function)) {
        case A_BINARY:
            last[tac.data[i].y] = i;
            if (first[tac.data[i].y] == -1)
                first[tac.data[i].y] = i;
            /* fallthrough */
        case A_UNARY:
            last[tac.data[i].x] = i;
            if (first[tac.data[i].x] == -1)
                first[tac.data[i].x] = i;
            break;
        case A_NULLARY:
            break;
        }
    }
}

uint16_t count_temps(TAC32Arr tac) {
    uint16_t temps_count = 0;
    for (size_t i = 0; i < tac.len; i++) {
        if (tac.data[i].result > temps_count)
            temps_count = tac.data[i].result;
    }
    return temps_count;
}

// uses graph coloring algorithm, 0th register represents unused result
uint16_t fold_temporaries(TAC32Arr tac) {
    Arena scratch = {0};
    // +1 for 0th
    size_t temps_count = count_temps(tac)+1;

    int *first = arena_alloc(&scratch, temps_count * sizeof(int));
    memset(first, 0xff, temps_count * sizeof(int));

    int *last = arena_alloc(&scratch, temps_count * sizeof(int));
    memset(last, 0xff, temps_count * sizeof(int));

    find_first_last_usage(tac, first, last);
    struct {
        int *data;
        size_t len, capacity;
    } *graph = arena_alloc(&scratch, temps_count*sizeof(*graph));
    memset(graph, 0, temps_count*sizeof(*graph));
    bool *unused = arena_alloc(&scratch, sizeof(*unused)*temps_count);
    memset(unused, 0, temps_count*sizeof(*unused));

    for (size_t i = 1; i < temps_count; i++) {
        for (size_t j = 1; j < temps_count; j++) {
            if (ranges_intersect(first[i], last[i], first[j], last[j]) &&
                i != j)
                da_append(&scratch, graph[i], j);
        }
    }

    uint32_t *map = arena_alloc(&scratch, temps_count*sizeof(*map));
    memset(map, 0, temps_count*sizeof(*map));

    bool *used_registers = arena_alloc(&scratch, temps_count*sizeof(bool));
    for (size_t i = 1; i < temps_count; i++) {
        if (unused[i]) continue;
        memset(used_registers, 0, temps_count*sizeof(bool));
        for (size_t j = 0; j < graph[i].len; j++) {
            uint32_t color = map[graph[i].data[j]];
            used_registers[color] = true;
        }
        for (size_t j = 1; j < temps_count; j++) {
            if (!used_registers[j]) {
                map[i] = j;
                goto success;
            }
        }
        UNREACHABLE();
    success:
        continue;
    }

    #define REMAP(x) (x) = (uint32_t)map[(x)]
    for (size_t i = 0; i < tac.len; i++) {
        REMAP(tac.data[i].result);
        switch (get_arity(tac.data[i].function)) {
        case A_BINARY:
            REMAP(tac.data[i].y);
            /* fallthrough */
        case A_UNARY:
            REMAP(tac.data[i].x);
            break;
        case A_NULLARY:
            break;
        }
    }
    #undef REMAP
    arena_destroy(&scratch);
    return count_temps(tac);
}

void remove_nops(TAC32Arr *tac) {
    size_t j = 0;
    for (size_t i = 0; i < tac->len; i++) {
        if (tac->data[i].function != TAC_NOP) {
            tac->data[j++] = tac->data[i];
        }
    }
    tac->len = j;
}

void remove_instruction(TAC32Arr *tac, size_t index) {
    tac->data[index].function = TAC_NOP;
}

bool remove_unused(TAC32Arr *tac) {
    Arena arena = {0};
    uint16_t temps_count = count_temps(*tac)+1;
    int *first = arena_alloc(&arena, sizeof(*first)*temps_count);
    memset(first, 0xff, temps_count * sizeof(*first));
    int *last = arena_alloc(&arena, sizeof(*last)*temps_count);
    memset(last, 0xff, temps_count * sizeof(*last));
    bool *unused = arena_alloc(&arena, sizeof(*unused) * temps_count);
    find_first_last_usage(*tac, first, last);
    for (uint16_t i = 1; i < temps_count; i++) {
        unused[i] = first[i] == last[i];
    }
    bool changed = false;
    for (size_t i = 0; i < tac->len; i++) {
        TAC32 *inst = &tac->data[i];
        if (unused[inst->result]) {
            inst->result = 0;
            if (ispure(inst->function)) {
                remove_instruction(tac, i);
                changed = true;
            }
        }
    }
    arena_destroy(&arena);
    if (changed) {
        remove_nops(tac);
    }
    return changed;
}

bool constant_propagation(TAC32Arr *tac) {
    bool changed = false;
    Arena arena = {0};
    if (count_temps(*tac) == 0) return false;
    bool *is_constant = arena_alloc(&arena, sizeof(*is_constant) * count_temps(*tac));
    struct {
        uint32_t v;
        enum {
            INT,
            SYM,
        } kind;
    } *constant_value =
        arena_alloc(&arena, sizeof(*constant_value) * count_temps(*tac));
    memset(is_constant, 0, sizeof(*is_constant)*count_temps(*tac));
    for (size_t i = 0; i < tac->len; i++) {
        TAC32 *inst = &tac->data[i];
        uint16_t r = inst->result;
        switch (inst->function) {
        case TAC_LOAD_SYM:
            constant_value[r].kind = SYM;
            is_constant[r] = true;
            constant_value[r].v = inst->x;
            break;
        case TAC_LOAD_INT:
            constant_value[r].kind = INT;
            is_constant[r] = true;
            constant_value[r].v = inst->x;
            break;
        case TAC_LOAD_ARG:
        case TAC_MOV:
        case TAC_LT:
        case TAC_LTI:
        case TAC_ADD:
        case TAC_ADDI:
        case TAC_SUB:
        case TAC_SUBI:
        case TAC_CALL_REG:
        case TAC_CALL_PUSH:
        case TAC_CALL_PUSH_INT:
        case TAC_CALL_PUSH_SYM:
        case TAC_CALL_SYM:
            is_constant[r] = false;
        case TAC_NOP:
        case TAC_BIZ:
        case TAC_LABEL:
        case TAC_RETURN_INT:
        case TAC_RETURN_VAL:
        case TAC_GOTO:
        case TAC_EXIT:
        case TAC_PHI:
            break;
        }
        switch (inst->function) {
        case TAC_ADD:
            if (is_constant[inst->x] && !is_constant[inst->y]) {
                if (constant_value[inst->x].kind == SYM) break;
                inst->function = TAC_ADDI;
                uint32_t c = inst->x;
                inst->x = inst->y;
                inst->y = constant_value[c].v;
                changed = true;
            } else if (!is_constant[inst->x] && is_constant[inst->y]) {
                if (constant_value[inst->y].kind == SYM) break;
                inst->function = TAC_ADDI;
                inst->y = constant_value[inst->y].v;
                changed = true;
            } else if (is_constant[inst->x] && is_constant[inst->y]) {
                if (constant_value[inst->x].kind == SYM ||
                    constant_value[inst->y].kind == SYM) break;
                inst->function = TAC_LOAD_INT;
                inst->x = constant_value[inst->x].v + constant_value[inst->y].v;
                changed = true;
            }
            break;
        case TAC_ADDI:
            if (is_constant[inst->x]) {
                inst->function = TAC_LOAD_INT;
                inst->x = inst->y + constant_value[inst->x].v;
                changed = true;
            }
            break;
        case TAC_SUB:
            if (constant_value[inst->x].kind == SYM) break;
            if (!is_constant[inst->x] && is_constant[inst->y]) {
                inst->function = TAC_SUBI;
                inst->y = constant_value[inst->y].v;
                changed = true;
            }
            break;
        case TAC_CALL_PUSH:
            if (!is_constant[inst->x]) break;
            switch (constant_value[inst->x].kind) {
            case INT:
                inst->function = TAC_CALL_PUSH_INT;
                break;
            case SYM:
                inst->function = TAC_CALL_PUSH_SYM;
                break;
            }
            inst->x = constant_value[inst->x].v;
            changed = true;
            break;
        case TAC_LT:
            if (!is_constant[inst->x] && is_constant[inst->y]) {
                if (constant_value[inst->y].kind != INT) break;
                inst->function = TAC_LTI;
                inst->y = constant_value[inst->y].v;
                changed = true;
            }
            break;
        case TAC_MOV:
            if (is_constant[inst->x]) {
                switch (constant_value[inst->x].kind) {
                case INT:
                    inst->function = TAC_LOAD_INT;
                    break;
                case SYM:
                    inst->function = TAC_LOAD_SYM;
                    break;
                }
                inst->x = constant_value[inst->x].v;
                changed = true;
            }
            break;
        case TAC_RETURN_VAL:
            if (!is_constant[inst->x]) break;
            switch (constant_value[inst->x].kind) {
            case INT:
                inst->function = TAC_RETURN_INT;
                break;
            case SYM:
                TODO();
                break;
            }
            inst->x = constant_value[inst->x].v;
            changed = true;
            break;
        default:
            break;
        }
    }
    arena_destroy(&arena);
    return changed;
}

static void insert_instruction(Arena *arena, TAC32Arr *tac, size_t index, TAC32 inst) {
    da_append_empty(arena, *tac);
    size_t i = tac->len-1;
    while (i-- > index) {
        tac->data[i+1] = tac->data[i];
    }
    tac->data[index] = inst;
}

bool return_lifting(Arena *arena, TAC32Arr *tac) {
    if (tac->len < 3) return false;
    size_t last = tac->len-1;
    if (!is_set_ret(tac->data[last].function))
        return false;
    if (tac->data[last-1].function != TAC_PHI) 
        return false;
    if (tac->data[last-2].function != TAC_LABEL) 
        return false;
    uint32_t label = tac->data[last-2].x;
    TAC32 ret = tac->data[last];
    TAC32 phi = tac->data[last-1];
    remove_instruction(tac, last-2);
    for (size_t i = 0; i < tac->len; i++) {
        if (tac->data[i].function != TAC_GOTO)
            continue;
        if (tac->data[i].x != label)
            continue;
        // TAC32 prev_inst = tac->data[i-1];
        // if (prev_inst.result == phi.x) {
        //     ret.x = phi.x;
        //     phi2 = phi.y;
        // } else if (prev_inst.result == phi.y) {
        //     ret.x = phi.y;
        //     phi2 = phi.x;
        // } else continue;
        for (size_t j = i; j --> 0; ) {
            TAC32 *inst = &tac->data[j];
            if (inst->result == phi.x) {
                insert_instruction(arena, tac, j+1, (TAC32){phi.result, TAC_MOV, phi.x, 0});
                i++;
            } else if (inst->result == phi.y) {
                insert_instruction(arena, tac, j+1, (TAC32){phi.result, TAC_MOV, phi.y, 0});
                i++;
            }
        }
        tac->data[i] = ret;
        insert_instruction(arena, tac, i+1, (TAC32){0, TAC_EXIT, 0, 0});
    }
    // ret.x = phi.result;
    insert_instruction(arena, tac, tac->len-2, ret);
    insert_instruction(arena, tac, tac->len-2, (TAC32){0, TAC_EXIT, 0, 0});

    // removing old ret
    tac->len -= 1;

    return true;
}

void remove_phi(Arena *arena, TAC32Arr *tac) {
    TAC32 phi;
    do {
        phi.function = TAC_NOP;
        for (size_t i = tac->len; i-- > 0; ) {
            TAC32 *inst = &tac->data[i];
            if (phi.function == TAC_NOP) {
                if (inst->function == TAC_PHI) {
                    phi = *inst;
                    inst->function = TAC_NOP;
                }
                continue;
            }
            if (inst->result == phi.x) {
                insert_instruction(arena, tac, i+1, (TAC32){phi.result, TAC_MOV, phi.x, 0});
            } else if (inst->result == phi.y) {
                insert_instruction(arena, tac, i+1, (TAC32){phi.result, TAC_MOV, phi.y, 0});
            }
        }
    } while (phi.function != TAC_NOP);
    remove_nops(tac);
}

void try_tail_call_optimization(Arena *arena, StaticFunction *func, String *names) {
    (void)arena;
    (void)func;
    (void)names;
    // TAC32 inst = func->code.data[func->code.len-1];
    // if (inst.function != TAC_CALL_SYM ||
    //     !string_eq(names[inst.x], names[func->name])) {
    //     return;
    // }
    // uint32_t *addr = NULL;
    // da_append(arena, func->code, ((TAC32){0, TAC_GOTO, 0, 0}));
    // addr = &da_last(func->code).x;
    // size_t i = func->code.len-1;
    // uint32_t label_id = 0;
    // while (i-- > 1) {
    //     func->code.data[i] = func->code.data[i-1];
    //     if (func->code.data[i].function == TAC_LABEL &&
    //         func->code.data[i].x > label_id) {
    //         label_id = func->code.data[i].x;
    //     }
    // }
    // func->code.data[0] = (TAC32){0, TAC_LABEL, label_id, 0};
    // *addr = label_id;
}

bool peephole_optimization(TAC32Arr *tac) {
    (void)tac;
    return false;
#if 0
    if (tac->len < 2) return false;
    bool changed = true;
    for (size_t i = 0; i < tac->len-1; i++) {
        TAC32 inst1 = tac->data[i + 0];
        TAC32 inst2 = tac->data[i + 1];
        changed = false;
    }
    remove_nops(tac);
    return changed;
#endif
}
