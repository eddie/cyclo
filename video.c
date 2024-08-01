#include "video.h"
#include <stdio.h>

// TODO: Character mode vs pixel mode.

// TODO: Have video memory
void video_write(uint16_t address, uint8_t data) {

    printf("Video written: %2X at %4X\n", data, address);
    // Instead, memory location could represent X,Y grid.

    // Can be done two ways, single address and write
    // commands and data to that address, or a command
    // address and then provide DMA range for video buffer.

    // is_cmd(address) ...
}

uint8_t video_read(uint16_t address) { return -1; }
