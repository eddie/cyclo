#include "video.h"
#include "emulator.h"
#include <stdio.h>
#include <string.h>

// TODO: Character mode vs pixel mode.
enum mode { GFX, CHAR };

enum cmd { WRITE = 0x01, SYNC = 0x02, CLEAR = 0x03 };

#define VIDMEM_BYTES 256
uint8_t vidmem[VIDMEM_BYTES];

// TODO: Have video memory
static void video_write(uint16_t address, uint8_t data) {

    printf("Video written: %2X at %4X\n", data, address);

    // Address 0 is our command
    if (address == 0) {
        switch (data) {
        case CLEAR:
            printf("Clearing video memory\n");
            memset(vidmem, 0, VIDMEM_BYTES);
            break;

        case SYNC:
            // write buffer to screen
            printf("Syncing buffer\n");
            break;
        }
    }

    // cmds
    // write to buffer
    // sync

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
