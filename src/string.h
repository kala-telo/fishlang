#ifndef STRING_H
#define STRING_H
#include <stdbool.h>
#include <string.h>

#define S(s) (String){.string = (s), .length = sizeof(s)-1}
#define PS(s) (s).length, (s).string

typedef struct {
    char *string;
    int length;
} String;

static inline bool string_eq(String s1, String s2) {
    if (s1.length != s2.length) {
        return false;
    }
    return memcmp(s1.string, s2.string, s1.length) == 0;
}
#endif // STRING_H
