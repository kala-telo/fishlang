#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "string.h"

#ifndef DA_H
#define DA_H

#define da_append_empty(xs)                                                    \
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
        memset(&(xs).data[(xs).len++], 0, sizeof(*(xs).data));                 \
    } while (0)

#define da_last(xs) ((xs).data[(xs).len - 1])

#define da_append(xs, x)                                                       \
    do {                                                                       \
        da_append_empty(xs);                                                   \
        da_last(xs) = (x);                                                     \
    } while (0)

#define da_pop(xs) (assert((xs).len > 0), (xs).data[--(xs).len])

static inline uint32_t fnv_1(String str) {
    uint32_t hash = 0x811c9dc5;
    for (int i = 0; i < str.length; i++) {
        hash *= 0x01000193;
        hash ^= str.string[i];
    }
    return hash;
}

#define hmget(ret, hm, _key, _found)                                           \
    do {                                                                       \
        if ((hm).table == NULL) {                                              \
            _found = 0;                                                        \
            break;                                                             \
        }                                                                      \
        (_found) = 0;                                                          \
        for (size_t i = 0;                                                     \
             i < (hm).table[fnv_1(_key) & ((hm).length - 1)].len; i++) {       \
            if (string_eq(                                                     \
                    (hm).table[fnv_1(_key) & ((hm).length - 1)].data[i].key,   \
                    (_key))) {                                                 \
                (ret) =                                                        \
                    (hm).table[fnv_1(_key) & ((hm).length - 1)].data[i].value; \
                (_found) = 1;                                                  \
                break;                                                         \
            }                                                                  \
        }                                                                      \
        if (!(_found))                                                         \
            (_found) = 0;                                                      \
    } while (0)

#define hmput(hm, _key, _value)                                                \
    do {                                                                       \
        if ((hm).table == NULL) {                                              \
            const size_t _hm_size = 64;                                        \
            (hm).table = calloc(_hm_size, sizeof(*(hm).table));                \
            (hm).length = _hm_size;                                            \
        }                                                                      \
        int break_out = 0;                                                     \
        for (size_t i = 0;                                                     \
             i < (hm).table[fnv_1(_key) & ((hm).length - 1)].len; i++) {       \
            if (string_eq(                                                     \
                    (hm).table[fnv_1(_key) & ((hm).length - 1)].data[i].key,   \
                    (_key))) {                                                 \
                (hm).table[fnv_1(_key) & ((hm).length - 1)].data[i].value =    \
                    (_value);                                                  \
                break_out = 1;                                                 \
                break;                                                         \
            }                                                                  \
        }                                                                      \
        if (break_out)                                                         \
            break;                                                             \
        da_append_empty((hm).table[fnv_1(_key) & ((hm).length - 1)]);          \
        da_last((hm).table[fnv_1(_key) & ((hm).length - 1)]).key = (_key);     \
        da_last((hm).table[fnv_1(_key) & ((hm).length - 1)]).value = (_value); \
    } while (0)

#define hmfree(hm)                                                             \
    do {                                                                       \
        for (size_t i = 0; i < (hm).length; i++) {                             \
            if ((hm).table[i].data != NULL) {                                  \
                free((hm).table[i].data);                                      \
                (hm).table[i].data = NULL;                                     \
            }                                                                  \
        }                                                                      \
        free((hm).table);                                                      \
        (hm).table = NULL;                                                     \
        (hm).length = 0;                                                       \
    } while (0);

#define hmrem(hm, _key)                                                        \
    do {                                                                       \
        if ((hm).table == NULL) {                                              \
            const size_t _hm_size = 64;                                        \
            (hm).table = calloc(_hm_size, sizeof(*(hm).table));                \
            (hm).length = _hm_size;                                            \
        }                                                                      \
        for (size_t i = 0;                                                     \
             i < (hm).table[fnv_1(_key) & ((hm).length - 1)].len; i++) {       \
            if (string_eq(                                                     \
                    (hm).table[fnv_1(_key) & ((hm).length - 1)].data[i].key,   \
                    (_key))) {                                                 \
                memset(                                                        \
                    &(hm).table[fnv_1(_key) & ((hm).length - 1)].data[i].key,  \
                    0,                                                         \
                    sizeof((hm).table[fnv_1(_key) & ((hm).length - 1)]         \
                               .data[i]                                        \
                               .key));                                         \
                break;                                                         \
            }                                                                  \
        }                                                                      \
    } while (0)

#endif // DA_H
