#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tac.h"
#include "da.h"

static bool ranges_intersect(int f1, int t1, int f2, int t2) {
    return !(t1 < f2 || t2 < f1);
}

typedef enum {
    A_NULLARY,
    A_UNARY,
    A_BINARY,
} Arity;

static Arity get_arity(TACOp op) {
    switch (op) {
    case TAC_ADD:
    case TAC_SUB:
    case TAC_LT:
        return A_BINARY;
    case TAC_CALL_REG:
    case TAC_CALL_PUSH:
    case TAC_RETURN_VAL:
    case TAC_BIZ:
    case TAC_MOV:
    case TAC_ADDI:
    case TAC_SUBI:
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
        return A_NULLARY;
    }
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
        default:
        return false;
    }
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

// TODO: try to remember tf am i doing here with 0th register and assign some as
// "unused", so it wouldn't do register pressure and generate unneccessary
// movement
//
// UPD: map structure on line 100 seems to relate to this
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
    // memset(map, 0xff, temps_count*sizeof(*map));
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
        // i think it should be unreachable
        abort();
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
    for (uint16_t i = 0; i < temps_count; i++) {
        unused[i] = first[i] == last[i];
    }
    bool changed = false;
    for (size_t i = 0; i < tac->len; i++) {
        TAC32 inst = tac->data[i];
        if (ispure(inst.function) && unused[inst.result]) {
            remove_instruction(tac, i);
            changed = true;
        }
    }
    arena_destroy(&arena);
    if (changed) {
        remove_nops(tac);
    }
    return changed;
}

bool peephole_optimization(TAC32Arr *tac) {
    if (tac->len < 2) return false;
    bool changed = true;
    for (size_t i = 0; i < tac->len-1; i++) {
        TAC32 inst1 = tac->data[i + 0];
        TAC32 inst2 = tac->data[i + 1];
        // rX = <symbol>
        // CALL_PUSH_SYM rX
        if (       inst1.function == TAC_LOAD_SYM &&
                   inst2.function == TAC_CALL_PUSH &&
                   inst2.x == inst1.result) {
            tac->data[i] = inst2;
            tac->data[i].function = TAC_CALL_PUSH_SYM;
            tac->data[i].x = inst1.x;
            remove_instruction(tac, i+1);
        // rX = <int>
        // CALL_PUSH_INT rX
        } else if (inst1.function == TAC_LOAD_INT &&
                   inst2.function == TAC_CALL_PUSH &&
                   inst2.x == inst1.result) {
            tac->data[i] = inst2;
            tac->data[i].function = TAC_CALL_PUSH_INT;
            tac->data[i].x = inst1.x;
            remove_instruction(tac, i+1);
        // rX = <int>
        // ret = rX
        } else if (inst1.function == TAC_LOAD_INT &&
                   inst2.function == TAC_RETURN_VAL &&
                   inst2.x == inst1.result) {
            tac->data[i] = inst2;
            tac->data[i].function = TAC_RETURN_INT;
            tac->data[i].x = inst1.x;
            remove_instruction(tac, i+1);
        // rY = <int>
        // rZ = rX - rY
        } else if (inst1.function == TAC_LOAD_INT &&
                   inst2.function == TAC_SUB &&
                   inst2.y == inst1.result) {
            tac->data[i] = inst2;
            tac->data[i].function = TAC_SUBI;
            tac->data[i].y = inst1.x;
            remove_instruction(tac, i+1);
        // rY = <int>
        // rZ = rX + rY
        } else if (inst1.function == TAC_LOAD_INT &&
                   inst2.function == TAC_ADD &&
                   inst2.y == inst1.result) {
            tac->data[i] = inst2;
            tac->data[i].function = TAC_ADDI;
            tac->data[i].y = inst1.x;
            remove_instruction(tac, i+1);
        } else {
            changed = false;
        }
    }
    remove_nops(tac);
    return changed;
}
