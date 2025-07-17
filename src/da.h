#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "string.h"

#ifndef DA_H
#define DA_H

#define da_append(xs, x)                                                       \
    do {                                                                       \
        if ((xs).len + 1 > (xs).capacity) {                                    \
            if ((xs).capacity != 0) {                                          \
                (xs).capacity *= 2;                                            \
            } else {                                                           \
                (xs).capacity = 4;                                             \
            }                                                                  \
            (xs).data =                                                        \
                realloc((xs).data, sizeof(*(xs).data) * (xs).capacity);        \
            assert((xs).data != NULL);                                         \
        }                                                                      \
        (xs).data[(xs).len++] = (x);                                           \
    } while (0)

#define da_last(xs) ((xs).data[(xs).len - 1])

#define da_pop(xs) (assert((xs).len > 0), (xs).data[--(xs).len])


typedef struct {
    String key;
    uintptr_t value;
} HMEntry;

typedef struct {
    HMEntry *data;
    size_t len, capacity;
} Bucket;

typedef struct {
    Bucket *table;
    size_t length;
} HashMap;

#define HM_EMPTY (~(uintptr_t)0)

static inline uint32_t fnv_1(String str) {
    uint32_t hash = 0x811c9dc5;
    for (int i = 0; i < str.length; i++) {
        hash *= 0x01000193;
        hash ^= str.string[i];
    }
    return hash;
}

static inline uintptr_t hmget(HashMap hm, String key) {
    if (hm.table == NULL)
        return HM_EMPTY;
    Bucket bucket = hm.table[fnv_1(key) & (hm.length - 1)];
    for (size_t i = 0; i < bucket.len; i++) {
        if (string_eq(bucket.data[i].key, key)) {
            return bucket.data[i].value;
        }
    }
    return HM_EMPTY;
}

static inline void hmput(HashMap* restrict hm, String key, uintptr_t value) {
    if (hm->table == NULL) {
        // 64 is a nice constant, power of 2, should be enough
        // for variables
        const size_t size = 64;
        hm->table = calloc(size, sizeof(*hm->table));
        hm->length = size;
    }
    Bucket *bucket = &hm->table[fnv_1(key) & (hm->length - 1)];
    for (size_t i = 0; i < bucket->len; i++) {
        if (string_eq(bucket->data[i].key, key)) {
            bucket->data[i].value = value;
            return;
        }
    }
    da_append(*bucket, ((HMEntry){key, value}));
}

static inline void hmfree(HashMap* hm) {
    for (size_t i = 0; i < hm->length; i++) {
        if (hm->table[i].data != NULL) {
            free(hm->table[i].data);
            hm->table[i].data  = NULL;
        }
    }
    free(hm->table);
}

#endif // DA_H
