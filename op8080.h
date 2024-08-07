
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
struct instruction {
    char mnemonic[6];
    uint8_t opcode;
};

#define X(name, value)                                     \
    { #name, value }

// Base instructions
struct instruction instructions[] = {
    X(LD, 0x40),   X(ADD, 0x80),  X(OR, 0xB0),
    X(AND, 0xA0),  X(SUB, 0x90),  X(ADC, 0x88),
    X(SBB, 0x98),  X(XOR, 0xA8),

    X(HLT, 0x76),  X(ADI, 0xC6),

    X(JMP, 0xC3),  X(JNZ, 0xC2),  X(JZ, 0xCA),
    X(JNC, 0xD2),  X(JC, 0xDA),   X(JM, 0xFA),
    X(JP, 0xF2),   X(JPO, 0xE2),  X(JPE, 0xEA),
    X(CALL, 0xCD),

    X(LXI, 0x01),  X(STAX, 0x02), X(STA, 0x32),
    X(LDA, 0x3A),  X(LHLD, 0x2A),

    X(CMP, 0xB8),  X(CPI, 0xFE)};

struct instruction *lookup_base_mnem(char *mnemonic) {

    int total =
        sizeof(instructions) / sizeof(struct instruction);

    // TODO: Handle no mnemonic found!
    for (int i = 0; i < total; i++) {
        if (strcasecmp(mnemonic,
                       instructions[i].mnemonic) == 0) {
            return &instructions[i];
        }
    }

    return 0x0;
}
