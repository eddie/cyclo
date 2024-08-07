#include <stdint.h>

long load_file(char *path, char **buffer);
long load_file_uint8(char *path, uint8_t **mem);

void write_file(char *path, uint8_t *buffer, int length);
