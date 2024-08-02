#include "video.h"
#include "emulator.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum mode { GFX, CHAR };
enum cmd { WRITE = 0x01, SYNC = 0x02, CLEAR = 0x03 };

#define COLS 16
#define ROWS 16
#define VIDMEM_BYTES ROWS *COLS

static uint8_t vidmem[VIDMEM_BYTES];
static uint8_t data_offset = 1;

static void video_sync() {
    for (int i = 0; i < VIDMEM_BYTES; i++) {
        printf("%2X ", vidmem[i]);
        if ((i + 1) % COLS == 0) {
            printf("\n");
        }
    }
    // write(STDOUT_FILENO, "\x1b[2J", 4);
    // write(STDOUT_FILENO, "\x1b[H",3);
}

static void video_write(uint16_t address, uint8_t data) {

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
            video_sync();
            break;
        }
        return;
    }
    printf("Video written: %2X at %4X\n", data,
           address - data_offset);

    // Starting add address 2 is the video memory.
    // Laid out contiguously in memory.
    vidmem[address - data_offset] = data;
}

static uint8_t video_read(uint16_t address) {
    printf("Video read %4X \n", address);
    if (address > VIDMEM_BYTES - data_offset) {
        return 0xFF;
    }

    return vidmem[address - data_offset];
}

// Device-specific initialization function
static void device_init(struct machine *m,
                        struct device *d) {
    // Perform any device-specific setup here
    printf("video: Initializaing %d bytes in GFX mode\n",
           VIDMEM_BYTES);
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
