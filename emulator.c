#include "video.h"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG 1

void *die(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

#if DEBUG
#define OPCODE(x) printf(x "\n");
#else
#define OPCODE(x)
#endif

#define MAX_DEVICE 4

typedef unsigned char uint8_t;

struct device {

    uint16_t mem_range[2];
    char name[12];

    void (*write)(uint16_t address, uint8_t data);
    uint8_t (*read)(uint16_t address);
};

// Remember, addresses are 16bit and datapaths are 8bits
// The operand is 16bits for 16bit addressing

typedef struct {

    uint8_t memory[65536];
    uint16_t pc;

    // Registers
    uint16_t accumulator;
    uint8_t status;

    uint16_t a, b, c, d, e, f, g;

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

    int device_count;
    struct device devices[MAX_DEVICE];

} machine;

struct device *device_from_address(machine *m,
                                   int16_t address) {
    struct device *d;
    d = &m->devices[0];

    if (d) {

        if ((address >= d->mem_range[0]) &&
            (address <= d->mem_range[1])) {

            return d;
        }
    }

    return 0;
}

void write_memory(machine *m, int16_t address,
                  uint8_t data) {
    if (address >= 65536) {
        die("Seg Fault!\n");
    }

    struct device *d;
    d = device_from_address(m, address);

    if (d) {
        // Normalize the address for the device
        int16_t addr_normal = address - d->mem_range[0];
        d->write(addr_normal, data);
    }

    m->memory[address] = data;
}

uint8_t read_memory(machine *m, int16_t address) {
    if (address >= 65536) {
        die("Seg fault!");
    }

    struct device *d = device_from_address(m, address);

    if (d) {
        return d->read(address);
    }

    return m->memory[address];
}

void load_program(machine *m, uint8_t *data, int length) {
    if (!data) {
        die("No program, halting!");
    }

    memcpy(&m->memory, data, length);
}

void print_machine_status(machine *m) {

#if DEBUG
    printf("\rA:%04x B:%04x Carry: %u\n", m->accumulator,
           m->b, (m->status >> 1) & 1);
#endif
}

void register_device(machine *m, char *d_name, int d_index,
                     uint16_t m_low, uint16_t m_high,
                     void (*write)(uint16_t address,
                                   uint8_t data),
                     uint8_t (*read)(uint16_t address)) {

    if (d_index > MAX_DEVICE) {
        die("device index out of range");
    }

    struct device *d = &m->devices[d_index];

    d->mem_range[0] = m_low;
    d->mem_range[1] = m_high;

    strcpy(d->name, d_name);

    d->write = write;
    d->read = read;

    printf(
        "Device registered, Name:%s, Low:%0X, High:%0X\n",
        d->name, d->mem_range[0], d->mem_range[1]);

    m->device_count++;
}

// TODO: Update flags on Cyclo
// TODO: Implement Sub with carry
// TODO: Allow simulated clock speed

void run(machine *m) {
    m->accumulator = 0;
    m->status = 0;
    m->pc = 0x0;

    int running = 1;

    uint8_t opcode, oplow, ophigh;
    uint16_t operand;

    while (running) {
        // 3 Cycle Fetch
        opcode = read_memory(m, m->pc++);
        ophigh = read_memory(m, m->pc++);
        oplow = read_memory(m, m->pc++);

        operand = ophigh << 8;
        operand = operand + oplow;

#if DEBUG
        printf("OP HIGH: %02X OP LOW: %02X OPERAND: %04X\n",
               ophigh, oplow, operand);
        printf("[%02X]: op: %02X %04X \t", m->pc, opcode,
               operand);
#endif

        switch (opcode) {

        case 0x00:

            if ((m->accumulator + operand) > 65535) {

                m->status |= (1 << 1);
                m->accumulator &= operand;

            } else {
                m->accumulator += operand;

                if (m->accumulator == 0) {
                    m->status |= 1;
                }
            }

            OPCODE("ADD");
            break;

        // Should we unset the carry flag after?
        case 0x01:
            OPCODE("ADC")
            m->accumulator +=
                operand + ((m->status >> 1) & 1);
            break;

        case 0x02:
            m->accumulator -= operand;
            OPCODE("SUB")
            break;

        case 0x03:
            OPCODE("SBC")
            break;

        // LDA: Load immediate value into accumulator
        // TODO: Check immediate vs memory based working.
        case 0x14:
            m->accumulator = operand;
            OPCODE("LDA")
            break;

        case 0x15:
            m->b = oplow;
            OPCODE("LDB")
            break;

        case 0x2C:
            m->accumulator = m->b;
            OPCODE("LDAB")
            break;

        case 0x2D:
            m->b = m->accumulator;
            OPCODE("LDBA")
            break;

        case 0x07:
            m->accumulator = ~m->accumulator;
            OPCODE("NOT")
            break;

        case 0x04:
            OPCODE("AND")
            m->accumulator &= operand;
            break;

        case 0x05:
            OPCODE("OR")
            m->accumulator |= operand;
            break;

        case 0x06:
            OPCODE("XOR")
            m->accumulator ^= operand;
            break;

        // Load value from memory to accumulator
        case 0x09:
            OPCODE("LDM")
            m->accumulator = read_memory(m, operand);
            break;

        // Store value in accumulator to memory
        case 0x0A:
            OPCODE("STM")
            write_memory(m, operand, m->accumulator);
            break;

        case 0x0B:
            OPCODE("JMP")
            printf("Jumping to %#4x", operand);
            m->pc = operand;
            break;

        case 0x0C:
            OPCODE("JPI")
            m->pc = read_memory(m, operand);
            break;

        case 0x0D:
            OPCODE("JPZ")
            // First bit set. jump
            if (m->status & 1) {
                m->pc = operand;
            }
            break;

        case 0x0E:
            OPCODE("JPM")
            if ((m->accumulator >> 8) & 1) {
                m->pc = operand;
            }
            break;

        case 0x0F:
            OPCODE("JPC")
            if ((m->status >> 1) & 1) {
                m->pc = operand;
            }
            break;

        case 0x11:
            OPCODE("JE")
            break;

        case 0x12: {
            OPCODE("CMP")
            // Set the carry flag, this is wrong
            int8_t tmp;
            tmp = (uint8_t)(m->accumulator - operand);
            if (tmp == 0) {
                m->status |= 1;
            } else {
                m->status &= 0;
            }
            break;

        case 0x30: {
            OPCODE("INCA")
            m->accumulator++;
            break;
        }
        case 0x31: {
            OPCODE("INCB")
            m->b++;
            break;
        }
        case 0xFF:
            running = 0;
            OPCODE("HLT");
            break;
        }
        }

        print_machine_status(m);
        // usleep(1000); // 1MHz/1000 = 1Khz
    }
}

void load_file(machine *m, char *path) {
    if (strlen(path) <= 0) {
        die("load_file: no path to load");
    }

    FILE *file;
    char *buffer;
    long length;

    file = fopen(path, "rb");

    if (!file) {
        die("load_file: %s file doesn't exist", path);
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer = malloc(length + 1);
    fread(buffer, 1, length, file);

    fclose(file);

    memcpy(&m->memory, buffer, length);
    free(buffer);
}

void dump_memory(machine *m) {
    int i;

    for (i = 0; i < 200; i++) {
        if ((i % 0x10) == 0) {
            printf("\n %04X | ", i);
        }
        printf("%02X ", m->memory[i]);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        die("no program specified");
    }

    machine m;

    printf("Loading program %s\n", argv[1]);
    load_file(&m, argv[1]);

    // TODO: Move to video
    register_device(&m, "video", 0, 0xA000, 0xA7FF,
                    video_write, video_read);

    run(&m);
}
