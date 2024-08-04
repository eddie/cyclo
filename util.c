#include "util.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void *die(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

void *xmalloc(size_t size) {
    void *ptr;

    if (size == 0) {
        die("xmalloc: Zero size\n");
    }

    ptr = malloc(size);

    if (ptr == NULL) {
        die("xmalloc: out of memory (allocating %zu "
            "bytes)\n",
            size);
    }

    return ptr;
}

void xfree(void *ptr) {
    if (ptr == NULL) {
        die("xfree: NULL pointer given as argument \n");
    }
}

int hexchar_to_int(char c) {
    c = toupper(c);

    if (c >= 'A' && c <= 'F')
        return 10 + ((c - 17) - '0');

    return 0;
}

int htoi(const char s[]) {
    int i, n, t;
    i = n = t = 0;

    if (s[i] == '0') {
        ++i;
        if (s[i] == 'x' || s[i] == 'X')
            ++i;
        else
            --i;
    }

    while (s[i] != '\0') {

        n = n * 16;
        if (isdigit(s[i])) {
            n += s[i] - '0';

        } else {
            if ((t = hexchar_to_int(s[i])))
                n += t;
            else
                return 0;
        }

        ++i;
    }

    return n;
}
