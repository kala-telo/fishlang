#include "string.h"
#include <stdbool.h>
#include <stddef.h>

bool string_eq(String s1, String s2) {
    if (s1.length != s2.length) {
        return false;
    }
    for (int i = 0; i < s1.length; i++) {
        if (s1.string[i] != s2.string[i])
            return false;
    }
    return true;
}
