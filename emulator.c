#include "emulator.h"
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

typedef unsigned char uint8_t;

struct device *device_from_address(struct machine *m,
                                   uint16_t address) {
    struct device *d = m->devices[0];

    if (d) {

        if ((address >= d->mem_range[0]) &&
            (address <= d->mem_range[1])) {

            return d;
        }
    }

    return 0;
}

void write_memory(struct machine *m, uint16_t address,
                  uint8_t data) {
    if (address >= 65535) {
        die("Seg Fault!\n");
    }

    struct device *d;
    d = device_from_address(m, address);

    if (d) {
        // Normalize the address for the device
        uint16_t addr_normal = address - d->mem_range[0];
        d->write(addr_normal, data);
    }

    m->memory[address] = data;
}

uint8_t read_memory(struct machine *m, uint16_t address) {

    struct device *d = device_from_address(m, address);

    if (d) {
        return d->read(address);
    }

    return m->memory[address];
}

void load_program(struct machine *m, uint8_t *data,
                  int length) {
    if (!data) {
        die("No program, halting!");
    }

    memcpy(&m->memory, data, length);
}

void print_machine_status(struct machine *m) {
    printf("\rA:%04x B:%04x HL:%04x%04x Carry: %u\n",
           m->accumulator, m->b, m->h, m->l,
           (m->status >> 1) & 1);
}

int emu_register_device(struct machine *m,
                        struct device *dev) {

    int index = m->device_count++;
    printf("Registering device %d\n", index);
    if (index > MAX_DEVICE) {
        die("device index out of range");
    }

    m->devices[index] = dev;
    struct device *d = m->devices[index];

    printf(
        "Device registered, Name:%s, Low:%0X, High:%0X\n",
        d->name, d->mem_range[0], d->mem_range[1]);

    return 0;
}

// TODO: Update flags on Cyclo
// TODO: Implement Sub with carry
// TODO: Allow simulated clock speed
// TODO: Update all flags after appropriate operations
//
void init(struct machine *m) {
    m->accumulator = 0;
    m->b = 0;
    m->status = 0;
    m->pc = 0x0;
    m->halted = 0;
}

void step(struct machine *m) {

    // 3 Cycle Fetch
    uint8_t opcode = read_memory(m, m->pc++);
    uint8_t ophigh = read_memory(m, m->pc++);
    uint8_t oplow = read_memory(m, m->pc++);

    uint16_t operand = ophigh << 8;
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

    case 0x01:
        OPCODE("ADC")

        // TODO: Reset carry flag
        m->accumulator += operand + ((m->status >> 1) & 1);

        break;

    case 0x02:
        m->accumulator -= operand;
        OPCODE("SUB")
        break;

    case 0x03:
        OPCODE("SBC")
        break;

    case 0x14:
        m->accumulator = operand;
        OPCODE("LDA")
        break;

    case 0x15:
        m->b = operand;
        OPCODE("LDB")
        break;
    case 0x16:
        m->h = operand;
        OPCODE("LDH")
        break;

    case 0x17:
        m->l = operand;
        OPCODE("LDL")
        break;

    case 0x2C:
        m->accumulator = m->b;
        OPCODE("LDAB")
        break;

    case 0x2D:
        m->b = m->accumulator;
        OPCODE("LDBA")
        break;

    case 0x2E:
        m->accumulator = m->l;
        OPCODE("LDAL")
        break;
    case 0x2F:
        m->l = m->accumulator;
        OPCODE("LDLA")
        break;

    case 0x07:
        m->accumulator = ~m->accumulator;
        OPCODE("NOT")
        break;

    case 0x04:
        OPCODE("AND")
        m->accumulator &= operand;
        // Set the zero flag.
        if (m->accumulator == 0) {
            m->status |= 1;
        }

        break;

    case 0x05:
        OPCODE("OR")
        m->accumulator |= operand;

        // Set the zero flag.
        if (m->accumulator == 0) {
            m->status |= 1;
        }

        break;

    case 0x06:
        OPCODE("XOR")
        m->accumulator ^= operand;
        // Set the zero flag.
        if (m->accumulator == 0) {
            m->status |= 1;
        }
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

    case 0x12:
        OPCODE("JPE")
        if ((m->status >> 4) & 1) {
            m->pc = operand;
        }
        break;

    case 0x13:
        OPCODE("JPO")
        if ((m->status >> 4) & 0) {
            m->pc = operand;
        }
        break;

    case 0x11: {
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

    // TODO: Clean these up when we have more consistent
    // instruction set as we can reduce this repeated
    // code
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
    case 0x32: {
        OPCODE("INCH")
        m->h++;
        break;
    }
    case 0x33: {
        OPCODE("INCL")
        m->l++;
        break;
    }
    case 0x19:
        OPCODE("STA")
        write_memory(m, operand, m->accumulator);
        break;
    case 0x20:
        OPCODE("STB")
        write_memory(m, operand, m->b);
        break;

    case 0x21: {
        OPCODE("STAX")
        uint16_t rm = (m->h << 8) + m->l;
        write_memory(m, rm, m->accumulator);
        break;
    }

    case 0x42:
        OPCODE("PUSHA")
        m->memory[m->sp++] = m->accumulator;
        printf("\tPushed A: %04X %04X\n", m->accumulator,
               m->memory[m->sp - 1]);
        break;
    case 0x43:
        OPCODE("PUSHB")
        m->memory[m->sp++] = m->b;
        break;
    case 0x44:
        OPCODE("POPA")
        m->accumulator = m->memory[--m->sp];
        printf("\tPopped A: %04X %04X %04x\n",
               m->accumulator, m->memory[m->sp], m->sp);
        break;
    case 0x45:
        OPCODE("POPB")
        m->b = m->memory[--m->sp];
        break;

    case 0xFF:
        m->halted = 1;
        OPCODE("HLT");
        break;
    }
    }
}

void load_file(struct machine *m, char *path) {
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

void dump_memory(struct machine *m) {
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

    // usage
    // -v: Verbose
    // -s: Step

    struct machine mstack = {
        .device_count = 0,
        .sp = 0x00B0,
    };

    struct machine *m = &mstack;

    printf("Loading program %s\n", argv[1]);
    load_file(m, argv[1]);

    register_video_device(m);

    init(m);

    while (!m->halted) {
        step(m);
#if DEBUG
        print_machine_status(m);
#endif
    }

    dump_memory(m);
    return EXIT_SUCCESS;
}
