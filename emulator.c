#include "emulator.h"
#include "file.h"
#include "util.h"
#include "video.h"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG 1

#if DEBUG
#define OPCODE(x) printf("%s \n", x);
#else
#define OPCODE(x)
#endif

struct ophandler {
    uint8_t opcode;
    char mnemonic[8];
    void (*handle16)(struct machine *m, uint8_t opcode,
                     uint16_t operand);
    void (*handle8)(struct machine *m, uint8_t opcode,
                    uint8_t op1, uint8_t op2);
};

struct ophandler handlers[255] = {0};

void register_handler16(uint8_t opcode, char *mnemonic,
                        void (*handler)(struct machine *m,
                                        uint8_t opcode,
                                        uint16_t operand))

{
    handlers[opcode].opcode = opcode;
    handlers[opcode].handle16 = handler;
    strcpy(handlers[opcode].mnemonic, mnemonic);
}
void register_handler8(uint8_t opcode, char *mnemonic,
                       void (*handler)(struct machine *m,
                                       uint8_t opcode,
                                       uint8_t op1,
                                       uint8_t op2))

{
    handlers[opcode].opcode = opcode;
    handlers[opcode].handle8 = handler;
    strcpy(handlers[opcode].mnemonic, mnemonic);
}

void register_range16(uint8_t start, uint8_t end,
                      char *mnemonic,
                      void (*handler)(struct machine *m,
                                      uint8_t opcode,

                                      uint16_t operand)) {

    for (size_t i = start; i <= end; i++) {
        register_handler16(i, mnemonic, handler);
    }
}

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
    printf("\rA:%02x B:%02x HL:%02x%02x Carry: %u\n",
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
void init(struct machine *m) {
    m->accumulator = 0;
    m->b = 0;
    m->status = 0;
    m->pc = 0x0;
    m->halted = 0;
}

void op_hlt(struct machine *m, uint8_t opcode,
            uint16_t op) {
    OPCODE("HLT");
    m->halted = 1;
}

void op_ldi(struct machine *m, uint8_t opcode, uint8_t op1,
            uint8_t op2) {

    OPCODE("LDI");

    // for now just MVI L
    m->l = op1;
}

void op_ld(struct machine *m, uint8_t opcode, uint16_t op) {
    OPCODE("LD");

    uint8_t base = opcode - 0x40;
    uint8_t oplow = base & 0x0F;
    uint8_t ophigh = base & 0xF0;
    uint8_t tmp = 0x00;

    printf("xxxxxxxxxxxxxxX: %#x (%x,%x)-> %#2x\n", opcode,
           ophigh, oplow, oplow + ophigh);

    // Shift src arg when it's on the RHS of the instruction
    // grid. Essentially overlaying this grid.
    if (oplow > 0x07) {
        // now oplow = 0-7 (B, C, D, E, H, L, M, A)
        oplow = oplow - 0x08;

        // Now normalize the dest address to stack CELA
        // ontop of BDHM
        // now ophigh = 0-7 (B, D, H, M, C, E, L, A)
        ophigh = ophigh + 0x40;
    }

    // BCDEHLAM
    switch (oplow) {
    case 0x00:
        tmp = m->b;
        break;
    case 0x01:
        tmp = m->c;
        break;

    case 0x02:
        tmp = m->d;
        break;

    case 0x03:
        tmp = m->e;
        break;

    case 0x04:
        tmp = m->h;
        break;
    case 0x05:
        tmp = m->l;
        break;
    case 0x06:
        // TODO: tmp = read_memory(m, HL);
        break;
    case 0x07:
        tmp = m->accumulator;
        break;
    }

    // BDHMCELA
    switch (ophigh) {
    case 0x00:
        m->b = tmp;
        break;
    case 0x01:
        m->d = tmp;
        break;
    case 0x02:
        m->h = tmp;
        break;
    case 0x03:
        m->m = tmp;
        // TODO: write_memory at HL
        break;
    case 0x04:
        m->c = tmp;
        break;
    case 0x05:
        m->e = tmp;
        break;
    case 0x06:
        m->l = tmp;
        break;
    case 0x07:
        m->accumulator = tmp;
        break;
    }

    printf("oooooooooooo: %#x (%x,%x)-> %#2x\n", opcode,
           ophigh, oplow, oplow + ophigh);
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

    // Check if we have a handler
    struct ophandler *oh = &handlers[opcode];
    // If debug, print opcode lookup
    if (oh) {
        OPCODE(oh->mnemonic)
        if (oh->handle8) {
            oh->handle8(m, oh->opcode, oplow, ophigh);
        } else if (oh->handle16) {
            oh->handle16(m, oh->opcode, operand);
        }
    }
    return;

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
    }
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

    register_range16(0x40, 0x7F, "MOV", op_ld);
    register_handler16(0x76, "HLT", op_hlt);
    register_handler8(0x2E, "MVILH", op_ldi);

    printf("Loading program %s\n", argv[1]);
    long n =
        load_file_uint8(argv[1], (uint8_t **)&m->memory);

    register_video_device(m);

    init(m);

    while (!m->halted) {
        step(m);
#if DEBUG
        print_machine_status(m);
#endif
    }

    print_memory(m->memory, n);
    return EXIT_SUCCESS;
}
