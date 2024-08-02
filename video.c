#include "video.h"
#include "emulator.h"
#include <stdio.h>

// TODO: Character mode vs pixel mode.

// TODO: Have video memory
static void video_write(uint16_t address, uint8_t data) {

    printf("Video written: %2X at %4X\n", data, address);
    // Instead, memory location could represent X,Y grid.

    // Can be done two ways, single address and write
    // commands and data to that address, or a command
    // address and then provide DMA range for video buffer.

    // is_cmd(address) ...
}

static uint8_t video_read(uint16_t address) {
    printf("Video read %4X \n", address);
    return -1;
}

// Device-specific initialization function
static void device_init(struct machine *m,
                        struct device *d) {
    // Perform any device-specific setup here
    printf("Initializing device in mode: gfx\n");
}

static struct device video = {.mem_range = {0xA000, 0xA7FF},
                              .name = "Video",
                              .write = video_write,
                              .read = video_read

};

int register_video_device(struct machine *m) {
    printf("Registering\n");
    device_init(m, &video);
    return emu_register_device(m, &video);
}
