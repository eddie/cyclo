#include "file.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

long load_file(char *path, char **buffer) {
    if (strlen(path) <= 0) {
        die("load_file: no path to load");
    }

    FILE *file;
    long length;

    file = fopen(path, "r");

    if (!file) {
        die("load_file: file doesn't exist");
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);

    *buffer = xmalloc(length + 1);
    fread(*buffer, 1, length, file);

    fclose(file);
    return length;
}

long load_file_uint8(char *path, uint8_t *mem[]) {

    char *buffer;
    long len = load_file(path, &buffer);

    memcpy(mem, buffer, len);

    free(buffer);
    return len;
}

void write_file(char *path, uint8_t *buffer, int length) {
    if (!path) {
        die("dump_buffer: no path specified");
    }

    if (!buffer) {
        die("dump_buffer: no buffer");
    }

    if (length <= 0) {
        die("dump_buffer: zero length buffer");
    }

    FILE *file;
    file = fopen(path, "wb");

    if (!file) {
        die("dump_buffer: couldn't open %s for writing",
            path);
    }

    fwrite(buffer, length, 1, file);
    fclose(file);
}
