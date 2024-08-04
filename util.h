
#include <stdint.h>
#include <stdio.h>

void *die(const char *fmt, ...);
void *xmalloc(size_t size);

void xfree(void *ptr);

int hexchar_to_int(char c);
int htoi(const char s[]);

void print_memory(uint8_t *memory, int size);
