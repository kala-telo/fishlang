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

uint16_t fold_temporaries(TAC32Arr tac) {
    size_t temps_count = 0;
    for (size_t i = 0; i < tac.len; i++) {
        if (tac.data[i].result >= temps_count)
            temps_count = tac.data[i].result + 1;
    }

    int *first = malloc(temps_count * sizeof(int));
    memset(first, 0xff, temps_count * sizeof(int));

    int *last = malloc(temps_count * sizeof(int));
    memset(last, 0xff, temps_count * sizeof(int));

    for (size_t i = 0; i < tac.len; i++) {
        if (tac.data[i].result != 0) {
            last[tac.data[i].result] = i;
            if (first[tac.data[i].result] == -1)
                first[tac.data[i].result] = i;
        }
        switch (tac.data[i].function) {
        // Binary
        case TAC_LT:
        case TAC_ADD:
        case TAC_SUB:
            last[tac.data[i].y] = i;
            if (first[tac.data[i].y] == -1)
                first[tac.data[i].y] = i;
            /* fallthrough */
        // Unary
        case TAC_CALL_REG:
        case TAC_CALL_PUSH:
        case TAC_BIZ:
        case TAC_MOV:
        case TAC_RETURN_VAL:
            last[tac.data[i].x] = i;
            if (first[tac.data[i].x] == -1)
                first[tac.data[i].x] = i;
            break;
        // Have no inputs as variables
        case TAC_LOAD_INT:
        case TAC_LOAD_SYM:
        case TAC_LOAD_ARG:
        case TAC_CALL_SYM:
        case TAC_GOTO:
        case TAC_LABEL:
            break;
        }
    }

    struct {
        int *data;
        size_t len, capacity;
    } *graph;
    graph = calloc(temps_count, sizeof(*graph));

    for (size_t i = 1; i < temps_count; i++) {
        for (size_t j = 1; j < temps_count; j++) {
            if (ranges_intersect(first[i], last[i], first[j], last[j]) &&
                i != j)
                da_append(graph[i], j);
        }
    }
    free(first);
    first = NULL;
    free(last);
    last = NULL;

    uint16_t *map;
    map = malloc(temps_count*sizeof(*map));
    memset(map, 0xff, temps_count*sizeof(*map));

    uint16_t new_temps_count = 0;
    bool *used_registers = malloc(temps_count*sizeof(bool));
    for (size_t i = 1; i < temps_count; i++) {
        memset(used_registers, 0, temps_count*sizeof(bool));
        for (size_t j = 0; j < graph[i].len; j++) {
                uint8_t color = map[graph[i].data[j]];
            if (color == 0xff)
                continue;
            used_registers[color] = true;
        }
        for (size_t j = 0; j < temps_count; j++) {
            if (!used_registers[j]) {
                map[i] = j;
                if (new_temps_count < j)
                    new_temps_count = j; 
                goto success;
            }
        }
        // i think it should be unreachable
        abort();
    success:
        continue;
    }
    free(used_registers);

    for (size_t i = 0; i < temps_count; i++) {
        free(graph[i].data);
        graph[i].data = NULL;
    }
    free(graph);

    #define REMAP(x) (x) = (x) ? (uint32_t)map[(x)] : 0;
    for (size_t i = 0; i < tac.len; i++) {
        REMAP(tac.data[i].result);
        switch (tac.data[i].function) {
        // Binary
        case TAC_ADD:
        case TAC_SUB:
        case TAC_LT:
            REMAP(tac.data[i].y);
            /* fallthrough */
        // Unary
        case TAC_CALL_REG:
        case TAC_CALL_PUSH:
        case TAC_RETURN_VAL:
        case TAC_BIZ:
        case TAC_MOV:
            REMAP(tac.data[i].x);
            break;
        // Have no inputs as variables
        case TAC_LOAD_INT:
        case TAC_LOAD_SYM:
        case TAC_LOAD_ARG:
        case TAC_CALL_SYM:
        case TAC_LABEL:
        case TAC_GOTO:
            break;
        }
    }
    #undef REMAP
    free(map);
    return new_temps_count;
}
