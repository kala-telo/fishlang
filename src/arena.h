#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ARENA_H
#define ARENA_H

typedef struct _Arena Arena;
struct _Arena {
    struct {
        bool arena_heap_allocated;
        bool data_heap_allocated;
    } meta;
    /* V bump allocator V */
    uint8_t *data;
    size_t allocated;
    size_t capacity;
    /* ^ bump allocator ^ */
    Arena *next;
};

// TODO: padding
static inline void* arena_alloc(Arena *arena, size_t size) {
    assert(arena != NULL);

    // we assume if there is next arena the overflow already happen
    if (arena->next)
        return arena_alloc(arena->next, size);

    if (arena->data == NULL) {
        size_t s = 4096 > size ? 4096 : size*2;
        arena->data = calloc(1, s);
        arena->capacity = s; 
        arena->meta.data_heap_allocated = true;
    }
    // if(size > arena->capacity) {
    //     fprintf(stderr, "Failed to allocate size %zu\n", size);
    //     abort();
    // }
    arena->allocated += size;
    if (arena->allocated > arena->capacity) {
        arena->allocated = arena->capacity;
        arena->next = calloc(1, sizeof(*arena));
        if (arena->next == NULL) return NULL;
        arena->next->meta.arena_heap_allocated = true;
        return arena_alloc(arena->next, size);
    }
    return &arena->data[arena->allocated-size];
}

static inline void* arena_realloc(Arena *arena, void *ptr, size_t old_size, size_t new_size) {
    size_t min = old_size > new_size ? new_size : old_size;
    void *new_ptr = arena_alloc(arena, new_size);
    if (min != 0)
        memcpy(new_ptr, ptr, min);
    return new_ptr;
}

static inline void arena_destroy(Arena *arena) {
    if (arena == NULL) return;
    arena_destroy(arena->next);
    if (arena->meta.data_heap_allocated)
        free(arena->data);
    if (arena->meta.arena_heap_allocated)
        free(arena);
}

#endif
