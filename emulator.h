#include <stdint.h>

#pragma once

#define MAX_DEVICE 4

struct device {

    uint16_t mem_range[2];
    char name[12];

    void (*init)();
    void (*write)(uint16_t address, uint8_t data);
    uint8_t (*read)(uint16_t address);
};

// Remember, addresses are 16bit and datapaths are 8bits
// The operand is 16bits for 16bit addressing

struct machine {

    uint8_t memory[65536];
    uint16_t pc;
    uint16_t sp;

    // Registers
    uint8_t status;
    uint8_t accumulator, b, c, d, e, h, l;

    /*
     * Status Register
     *
     * Lowest
     * 0 - Zero Flag
     * 1 - Carry Flag
     * 2 - Sign Flag
     * 3 - Overflow flag
     * 4 - Parity Fla
     */

    // Interrupt register
    uint8_t ir;

    struct device *devices[MAX_DEVICE];
    int device_count;
};

int emu_register_device(struct machine *m,
                        struct device *dev);
