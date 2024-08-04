
#pragma once

#include <stdint.h>
#include <string.h>
#include <strings.h>

struct instruction {
    char mnemonic[6];
    uint8_t opcode;
};

#define X(name, value)                                     \
    { #name, value }

// TODO: Define the alias map.
// This file should expose actual opcodes, not the assembly
// equiv

// TODO: Re-order these based on 8080
// TODO: ADDA ADDB ADDL ADDH
// ADD A -> 0x87, ADD B = 0x86..
static struct instruction instructions[] = {
    X(ADD, 0x00),
    X(ADC, 0x01),
    {"SUB", 0x02},
    {"SBC", 0x03},
    {"AND", 0x04},
    {"OR", 0x05},
    {"XOR", 0x06},
    {"NOT", 0x07},
    {"LDM", 0x09},
    {"STM", 0x0A},

    {"JMP", 0x0B},
    {"JPI", 0x0C},
    {"JPZ", 0x0D},
    {"JPM", 0x0E},
    {"JPC", 0x0F},
    {"CMP", 0x11},
    {"JPE", 0x12},
    {"JPO", 0x13},

    {"HLT", 0xFF},

    // Load A,B from memory or immediate
    {"LDA", 0x14},
    {"LDB", 0x15},
    {"LDH", 0x16},
    {"LDL", 0x17},

    {"STA", 0x19},
    {"STB", 0x20},

    // Store value in A into (HL)
    {"STAX", 0x21},

    // Translated, Load register
    {"LDAB", 0x2C}, // LD a,b
    {"LDBA", 0x2D}, // LD b,a
    {"LDAL", 0x2E}, // LD a,l
    {"LDLA", 0x2F}, // LD l,a

    // pusedo instructions
    {"INC", 0xFF},
    {"INCA", 0x30},
    {"INCB", 0x31},
    {"INCH", 0x32},
    {"INCL", 0x33},

    {"POP", 0xFF},
    {"PUSH", 0xFF},
    {"PUSHA", 0x42},
    {"PUSHB", 0x43},
    {"POPA", 0x44},
    {"POPB", 0x45},

};

static inline struct instruction *
mnemonic_to_instruction(char *mnemonic) {

    int total =
        sizeof(instructions) / sizeof(struct instruction);

    for (int i = 0; i < total; i++) {
        if (strcasecmp(mnemonic,
                       instructions[i].mnemonic) == 0) {
            return &instructions[i];
        }
    }

    return 0;
}
