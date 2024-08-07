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
    printf("Registering %s %02X\n", mnemonic, opcode);
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
    printf("Registering %s %02X\n", mnemonic, opcode);
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
void register_range8(
    uint8_t start, uint8_t end, char *mnemonic,
    void (*handler)(struct machine *m, uint8_t opcode,

                    uint8_t op1, uint8_t op2)) {

    for (size_t i = start; i <= end; i++) {
        register_handler8(i, mnemonic, handler);
    }
}

void register_hrange8(
    uint8_t start, uint8_t endh, char *mnemonic,
    void (*handler)(struct machine *m, uint8_t opcode,

                    uint8_t op1, uint8_t op2)) {

    // From the start address increment the range (H) bits.
    for (uint8_t i = start; i <= endh; i += 0x10) {
        register_handler8(i, mnemonic, handler);
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
    printf("A:%02x B:%02x HL:%02x%02x PC: %#02x SP: %#02x "
           "C: %u\n\n",
           m->accumulator, m->b, m->h, m->l, m->pc, m->sp,
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

// Taking a grid of instructions, we can see it splits into
// quadrants. The first two quadrants are contained within
// 0x00 and 0x7F.
//
// The first quadrant is for BDHM and the second quadrant
// for CELA
//
// For a given opcode we typically want to convert part of
// it to a register. Which involess subtracting from an
// offset (the first instruction of the group) and taking
// the top 4 bits.
//
// We then want to typically align register bits so that
// they are congurent, e.g B,D,H,M,C,E,L,A 0x00-> 0x07
//
// We do this by adding the group size to the register high
// bits, and subtracting the offset from the low bits.
// Graphically this would look like translating the group
// and laying it below the initial group
uint8_t decompose_opcode(uint8_t opcode,
                         uint8_t group_start,
                         uint8_t group_offset,
                         uint8_t group_distance, uint8_t *h,
                         uint8_t *l) {

    opcode = opcode - group_start;

    uint8_t low = opcode & 0x0F;
    uint8_t high = opcode & 0xF0;

    if (low > 0x07) {
        low = low - group_distance;
        high = high + group_offset;
    }

    *h = high;
    *l = low;
    return high + low;
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
    m->halted = 1;
}

void op_ldi(struct machine *m, uint8_t opcode, uint8_t op1,
            uint8_t op2) {

    uint8_t low = opcode & 0x0F;

    // Stack MVI C/E/L/A ontop of B/D/H/M
    if (low == 0x0E) {
        opcode -= 0x08;
        opcode += 0x40;
    }

    // Shift B/D/H/M/C/E/L/A left on the 8080 chart
    opcode -= 0x06;

    // Now we have BDHMCELA as 0x10 0x20 and so on.
    switch (opcode) {
    case 0x0:
        m->b = op1;
        break;
    case 0x10:
        m->d = op1;
        break;
    case 0x20:
        m->h = op1;
        break;
    case 0x30:
        m->m = op1;
        break;
    case 0x40:
        m->c = op1;
        break;
    case 0x50:
        m->e = op1;
        break;
    case 0x60:
        m->l = op1;
        break;
    case 0x70:
        m->accumulator = op1;
        break;
    }
}

// Once we have normalized an opcode src register into 0x0
// -> 0x07 we use the following routine to fetch a value
// from the machine
uint8_t decode_src_value(struct machine *m, uint8_t oplow) {
    // BCDEHLAM
    switch (oplow) {
    case 0x00:
        return m->b;
    case 0x01:
        return m->c;
    case 0x02:
        return m->d;
        break;
    case 0x03:
        return m->e;
        break;
    case 0x04:
        return m->h;
        break;
    case 0x05:
        return m->l;
        break;
    case 0x06: {
        uint16_t hl = (m->h << 8) + m->l;
        return read_memory(m, hl);
    }
    case 0x07:
        return m->accumulator;
    }
}

void op_ld(struct machine *m, uint8_t opcode, uint16_t op) {

    uint8_t tmp = 0x00;
    uint16_t hl = (m->h << 8) + m->l;

    uint8_t ophigh, oplow;
    uint8_t opnew = decompose_opcode(opcode, 0x40, 0x40,
                                     0x08, &ophigh, &oplow);
    printf("------------------__>%x %x\n", opnew,
           oplow + ophigh);

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
        write_memory(m, hl, tmp);
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
}

void op_alu_reg(struct machine *m, uint8_t opcode,
                uint8_t op1, uint8_t op2) {

    uint16_t hl = (m->h << 8) + m->l;
    uint8_t high, low;

    decompose_opcode(opcode, 0x80, 0x40, 0x08, &high, &low);

    // Fetch src register value from machine
    uint8_t tmp = decode_src_value(m, low);

    printf("ALU: %02X %02X %02X\n", low, high, tmp);

    // Update status bits
    switch (high) {
    case 0x00:
        m->accumulator += tmp;
        break;
    case 0x10:
        m->accumulator -= tmp;
        break;
    case 0x20:
        m->accumulator &= tmp;
        break;
    case 0x30:
        m->accumulator |= tmp;
        break;
    case 0x40:
        // TODO: Clear status flag
        m->accumulator += tmp + ((m->status >> 1) & 1);
        break;
    case 0x50:
        // SBB
        break;
    case 0x60:
        m->accumulator ^= tmp;
        break;
    case 0x70: {
        // TODO: CHECK IMPLEM!
        uint8_t it = 0x0;
        it = (uint8_t)(m->accumulator - tmp);
        if (it == 0) {
            m->status |= 1;
        } else {
            m->status &= 0;
        }
        break;
    }
    }

    // TODO: Handle status register
}

void op_alu_inc_dec(struct machine *m, uint8_t opcode,
                    uint8_t op1, uint8_t op2) {

    uint8_t high, low;
    int8_t dir = 0;

    // INR
    if ((opcode & 0x0F) == 0x04 ||
        (opcode & 0x0F) == 0x0C) {

        decompose_opcode(opcode, 0x04, 0x40, 0x08, &high,
                         &low);
        dir = 1;
        // DCR
    } else if ((opcode & 0x0F) == 0x05 ||
               (opcode & 0x0F) == 0x0D) {
        decompose_opcode(opcode, 0x05, 0x40, 0x08, &high,
                         &low);
        dir = -1;
    }

    switch (high) {
    case 0x00:
        m->b += dir;
        break;
    case 0x10:
        m->d += dir;
        break;
    case 0x20:
        m->h += dir;
    case 0x30:
        m->m += dir;
    case 0x40:
        m->c += dir;
        break;
    case 0x50:
        m->e += dir;
        break;
    case 0x60:
        m->l += dir;
    case 0x70:
        m->accumulator += dir;
    }
}

void step(struct machine *m) {

    // 3 Cycle Fetch
    uint8_t opcode = read_memory(m, m->pc++);
    uint8_t ophigh = read_memory(m, m->pc++);
    uint8_t oplow = read_memory(m, m->pc++);

    uint16_t operand = ophigh << 8;
    operand = operand + oplow;

#if DEBUG
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
    register_hrange8(0x06, 0x36, "MVILH", op_ldi);
    register_hrange8(0x0E, 0x3E, "MVILH", op_ldi);

    register_range8(0x80, 0x87, "ADD", op_alu_reg);
    register_range8(0x88, 0x8F, "ADDC", op_alu_reg);

    register_range8(0x90, 0x97, "SUB", op_alu_reg);
    register_range8(0x98, 0x9F, "SBB", op_alu_reg);

    register_range8(0xA0, 0xA7, "ANA", op_alu_reg);
    register_range8(0xA8, 0xAF, "XRA", op_alu_reg);

    register_range8(0xB0, 0xB7, "ORA", op_alu_reg);
    register_range8(0xB8, 0xBF, "CMP", op_alu_reg);

    register_hrange8(0x04, 0x34, "INR", op_alu_inc_dec);
    register_hrange8(0x0C, 0x3C, "INR", op_alu_inc_dec);
    register_hrange8(0x05, 0x35, "DCR", op_alu_inc_dec);
    register_hrange8(0x0D, 0x3D, "INR", op_alu_inc_dec);

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
