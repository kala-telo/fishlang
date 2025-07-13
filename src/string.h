#ifndef STRING_H
#define STRING_H
#include <stdbool.h>

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
    for (int i = 0; i < s1.length; i++) {
        if (s1.string[i] != s2.string[i])
            return false;
    }
    return true;
}
#endif // STRING_H
