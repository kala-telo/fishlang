#include "string.h"
#include <assert.h>
#include <stdint.h>
#include "arena.h"

#ifndef DA_H
#define DA_H

#define da_append_empty(arena, xs)                                             \
    do {                                                                       \
        if ((xs).len + 1 > (xs).capacity) {                                    \
            size_t _da_append_old = sizeof(*(xs).data) * (xs).capacity;        \
            if ((xs).capacity != 0) {                                          \
                (xs).capacity *= 2;                                            \
            } else {                                                           \
                (xs).capacity = 4;                                             \
            }                                                                  \
            size_t _da_append_new = sizeof(*(xs).data) * (xs).capacity;        \
            (xs).data = arena_realloc((arena), (xs).data, _da_append_old,      \
                                      _da_append_new);                         \
            assert((xs).data != NULL);                                         \
        }                                                                      \
        memset(&(xs).data[(xs).len++], 0, sizeof(*(xs).data));                 \
    } while (0)

#define da_last(xs) ((xs).data[(xs).len - 1])

#define da_append(arena, xs, x)                                                \
    do {                                                                       \
        da_append_empty((arena), xs);                                          \
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

#define hms_get(ret, hm, _key, _found)                                         \
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

#define hms_put(arena, hm, _key, _value)                                       \
    do {                                                                       \
        if ((hm).table == NULL) {                                              \
            const size_t _hm_size = 64;                                        \
            (hm).table = arena_alloc((arena), _hm_size * sizeof(*(hm).table)); \
            memset((hm).table, 0, _hm_size * sizeof(*(hm).table));             \
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
        da_append_empty((arena), (hm).table[fnv_1(_key) & ((hm).length - 1)]); \
        da_last((hm).table[fnv_1(_key) & ((hm).length - 1)]).key = (_key);     \
        da_last((hm).table[fnv_1(_key) & ((hm).length - 1)]).value = (_value); \
    } while (0)

#define hms_rem(hm, _key)                                                      \
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

#define hm_get(ret, hm, _key, _found)                                          \
    do {                                                                       \
        if ((hm).table == NULL) {                                              \
            _found = 0;                                                        \
            break;                                                             \
        }                                                                      \
        String _hm_skey = (String){(void *)&(_key), sizeof(_key)};             \
        uint32_t _hm_hash = fnv_1(_hm_skey) & ((hm).length - 1);               \
        (_found) = 0;                                                          \
        for (size_t i = 0; i < (hm).table[_hm_hash].len; i++) {                \
            String _hm_iter_key = (String){                                    \
                (void *)&(hm).table[_hm_hash].data[i].key, sizeof(_key)};      \
            if (string_eq(_hm_iter_key, (_hm_skey))) {                         \
                (ret) = (hm).table[_hm_hash].data[i].value;                    \
                (_found) = 1;                                                  \
                break;                                                         \
            }                                                                  \
        }                                                                      \
    } while (0)

#define hm_put(arena, hm, _key, _value)                                        \
    do {                                                                       \
        if ((hm).table == NULL) {                                              \
            const size_t _hm_size = 64;                                        \
            (hm).table = arena_alloc((arena), _hm_size * sizeof(*(hm).table)); \
            memset((hm).table, 0, _hm_size * sizeof(*(hm).table));             \
            (hm).length = _hm_size;                                            \
        }                                                                      \
        int break_out = 0;                                                     \
        String _hm_skey = (String){(void *)&(_key), sizeof(_key)};             \
        uint32_t _hm_hash = fnv_1(_hm_skey) & ((hm).length - 1);               \
        for (size_t i = 0; i < (hm).table[_hm_hash].len; i++) {                \
            if (string_eq((String){(void *)&(hm).table[_hm_hash].data[i].key,  \
                                   sizeof(_key)},                              \
                          _hm_skey)) {                                         \
                (hm).table[_hm_hash].data[i].value = (_value);                 \
                break_out = 1;                                                 \
                break;                                                         \
            }                                                                  \
        }                                                                      \
        if (break_out)                                                         \
            break;                                                             \
        da_append_empty((arena), (hm).table[_hm_hash]);                        \
        da_last((hm).table[_hm_hash]).key = (_key);                            \
        da_last((hm).table[_hm_hash]).value = (_value);                        \
    } while (0)

#define hm_rem(hm, _key)                                                      \
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
