#include <stdio.h>
#include <stdlib.h>

#ifndef TODO_H
#define TODO_H

#define TODO()                                                                 \
    do {                                                                       \
        fprintf(stderr, "%s:%d: TODO\n", __FILE__, __LINE__);                  \
        abort();                                                               \
    } while (0)

#define UNREACHABLE()                                                          \
    do {                                                                       \
        fprintf(stderr, "%s:%d: UNREACHABLE -- It shouldn've not happen...\n", \
                __FILE__, __LINE__);                                           \
        abort();                                                               \
    } while (0)

#endif // TODO_H
