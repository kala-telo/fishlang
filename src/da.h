#include <assert.h>

#ifndef DA_H

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

#endif // DA_H
