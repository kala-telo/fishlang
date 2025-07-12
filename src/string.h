#ifndef STRING_H
#define STRING_H
#include <stdbool.h>

#define S(s) (String){.string = (s), .length = sizeof(s)-1}
#define PS(s) (s).length, (s).string

typedef struct {
    char *string;
    int length;
} String;

bool string_eq(String s1, String s2);
#endif // STRING_H
