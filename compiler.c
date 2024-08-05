#include "file.h"
#include "util.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define DEBUG_ASSEMBLER(type, inst, width)                 \
    printf("Write %-8s %-6s(%#04x) PC:%#04x W: %d\n",      \
           type, inst->mnemonic, inst->opcode, pc, width);

#define DEBUG_ASSEMBLER_VAL(type, inst, width, val)        \
    printf(                                                \
        "Write %-8s %-6s(%#04x) PC:%#04x W: %d \t %s\n",   \
        type, inst->mnemonic, inst->opcode, pc, width,     \
        val);

#define SYNTAX_ERROR(msg) die("Syntax error: %s", msg);

#define EQUALS(a, b) (strcasecmp(a, b) == 0)

#define ASSERT_ARG_COUNT(ast_node, count, name)            \
    if (ast_node->argn != count) {                         \
        SYNTAX_ERROR("Invalid argument count");            \
    }

enum state {
    PROGRAM,
    INSTRUCTION,
    OPERAND,
    LABEL,
    COMMENT,
    DIRECTIVE,
    ADDR,
};

#define MAX_ARG_LEN 255
#define MAX_SYMBOLS 255
#define MAX_SYMBOL_LEN 255

enum tokens { TINST, TOPERAND, TLABEL, TDIR, TADDR };

struct token {
    char s_val[255];
    int i_val;
    int type;
    int curs;

    struct token *next;
    struct token *prev;
};

enum op_type { TREGISTER, TVALUE, TADDRESS, TREGADDRESS };
struct ast_operand {
    char value[MAX_ARG_LEN];
    int type;
};

enum ast_type { AST_INSTRUCTION, AST_LABEL };

struct ast_node {
    // Opcode
    char opcode[255];
    char label[255];
    unsigned int type;

    // Operands, max of 4
    size_t argn;
    struct ast_operand operands[4];

    // Additional data, e.g db 0,"Hello world", 0
    char data[255];
};

struct ast {
    struct ast_node nodes[1024];
    size_t total;
};

struct symbol {
    char name[255];
    size_t line; // AST line
};

struct symbol_table {
    struct symbol symbols[MAX_SYMBOLS];
};

unsigned int hash(const char *str, unsigned tableSize) {
    unsigned int hash = 0;
    while (*str) {
        hash = (hash << 5) + *str++;
    }
    return hash % tableSize;
}

struct symbol_table *init_symbol_table() {

    struct symbol_table *st =
        xmalloc(sizeof(struct symbol_table));

    memset(st, 0, sizeof(struct symbol_table));

    return st;
}

void add_symbol(struct symbol_table *st, const char *name,
                size_t line) {
    unsigned int index = hash(name, MAX_SYMBOLS);

    struct symbol *sym = &st->symbols[index];
    sym->line = line;
    memcpy(sym->name, name, MAX_SYMBOL_LEN);
}

size_t get_symbol_line(struct symbol_table *st,
                       const char *name) {
    unsigned int index = hash(name, MAX_SYMBOLS);
    return st->symbols[index].line;
}

// Build a line based symbol table
struct symbol_table *build_symbol_table(struct ast *ast) {
    struct symbol_table *st = init_symbol_table();

    size_t line = 0;

    // Build symbol table from ASt
    for (size_t i = 0; i < ast->total; i++) {
        struct ast_node *n = &ast->nodes[i];

        // got a label? store the line
        if (n->type == AST_LABEL) {
            add_symbol(st, n->label, line);
        } else if (n->type == AST_INSTRUCTION) {
            line++;
        }
    }

    return st;
}

void free_symbol_table(struct symbol_table *st) {
    free(st);
}

void add_list(struct token *root, struct token *next) {
    if (!root) {
        return;
    }

    struct token *tmp;

    if (root->next == NULL) {
        root->next = next;
        next->prev = root;
        return;
    }

    tmp = root->next;
    while (tmp->next != NULL) {
        tmp = tmp->next;
    }

    tmp->next = next;
    next->prev = tmp;
}

struct token *create_token(struct token *root, int type) {
    struct token *tmp = xmalloc(sizeof(struct token));

    tmp->next = NULL;
    tmp->type = type;
    tmp->i_val = 0;
    tmp->curs = 0;

    if (root) {
        add_list(root, tmp);
    }

    return tmp;
}

void astrval(struct token *t, char c) {
    if (t == NULL) {
        return;
    }
    t->s_val[t->curs++] = c;
    t->s_val[t->curs] = '\0';
}

void free_list(struct token *root) {
    if (!root) {
        return;
    }

    struct token *t = NULL;

    while (root != NULL) {

        t = root->next;
        free(root);
        root = t;
    }
}

void print_tokens(struct token *root) {
    struct token *tmp = root;

    do {
        if (tmp->type == TINST) {
            printf("Instruction: %s\n", tmp->s_val);
        } else if (tmp->type == TOPERAND) {
            printf("Operand: %s\n", tmp->s_val);
        } else if (tmp->type == TDIR) {
            printf("Directive: %s\n", tmp->s_val);
        } else if (tmp->type == TLABEL) {
            printf("Label: %s\n", tmp->s_val);
        } else if (tmp->type == TADDR) {
            printf("Address: %s\n", tmp->s_val);
        }
    } while ((tmp = tmp->next) != NULL);
}

struct token *tokenize(char *buffer) {
    int state = PROGRAM;
    int line = 0;

    struct token *t_tmp, *t_root;
    char c, *p;

    t_root = create_token(NULL, -1);

    for (p = buffer; p != buffer + strlen(buffer); p++) {

        c = *p;

        if (c == '#') {

            state = COMMENT;

        } else if (c == '.') {

            if (state == PROGRAM) {

                state = DIRECTIVE;
                t_tmp = create_token(t_root, TDIR);
            }

        } else if (c == ' ') {

            if (state == INSTRUCTION ||
                state == DIRECTIVE) {

                state = OPERAND;
                t_tmp = create_token(t_root, TOPERAND);
            }

        } else if (c == ',') {

            if (state == OPERAND) {
                // Create a new operand
                state = OPERAND;
                t_tmp = create_token(t_root, TOPERAND);
            }

        } else if (isalpha(c) || isdigit(c)) {

            if (state == COMMENT)
                continue;

            if (state == PROGRAM) {

                state = INSTRUCTION;
                t_tmp = create_token(t_root, TINST);
            }

            // TODO: Only append value on
            // directive,instruction or operand
            astrval(t_tmp, c);

        } else if (c == ':') {

            if (state == INSTRUCTION) {

                t_tmp->type = TLABEL;
                state = PROGRAM;
            }

        } else if (c == '[') {

            if (state == OPERAND) {
                state = ADDR;
                t_tmp->type = TADDR;
            }

        } else if (c == '\n') {

            state = PROGRAM;
            line++;
        }
    }

    return t_root;
}

struct ast *parse_tokens(struct token *root) {
    struct token *tmp = root;
    struct ast *ast = xmalloc(sizeof(struct ast));

    memset(ast, 0, sizeof(struct ast));

    do {

        // TODO: Directives will be handled sligthly
        // differently but this works for now
        if (tmp->type == TINST || tmp->type == TDIR) {
            struct ast_node *n = &ast->nodes[ast->total++];
            strcpy(n->opcode, tmp->s_val);
            n->type = AST_INSTRUCTION;

            struct token *op = tmp->next;

            while (op && (op->type == TOPERAND ||
                          op->type == TADDR)) {

                struct ast_operand *arg =
                    &n->operands[n->argn++];

                strcpy(arg->value, op->s_val);

                if (op->type == TOPERAND) {
                    if (isalpha(op->s_val[0]) &&
                        !isalpha(op->s_val[1])) {
                        arg->type = TREGISTER;
                    } else {
                        arg->type = TVALUE;
                    }
                } else if (op->type == TADDR) {
                    if (isalpha(op->s_val[0])) {
                        arg->type = TREGADDRESS;
                    } else {
                        arg->type = TADDRESS;
                    }
                }

                op = op->next;
            }

        } else if (tmp->type == TLABEL) {
            struct ast_node *n = &ast->nodes[ast->total++];
            strcpy(n->label, tmp->s_val);
            n->type = AST_LABEL;
        } else if (tmp->type == TADDR) {
        }
    } while ((tmp = tmp->next) != NULL);

    return ast;
}

enum { SINST = 1, SOP = 1 };

uint8_t is_cela(char reg) {
    switch (reg) {
    case 'c':
    case 'C':
    case 'e':
    case 'E':
    case 'l':
    case 'L':
    case 'a':
    case 'A':
        return 1;
    }
    return 0;
}

// Following a common pattern in the 8080 instruction state
// some src/dest pairs have a fixed pattern. .eg A Last, B
// first. This can be reused.
uint8_t reg_to_offset(char reg) {
    uint8_t base = 0;

    switch (reg) {
    case 'b':
    case 'B':
        return 0x00;
        break;
    case 'h':
    case 'H':
        return 0x04;
        break;
    case 'l':
    case 'L':
        return 0x05;
    case 'm':
    case 'M':
        return 0x06;
    case 'a':
    case 'A':
        return 0x07;
        break;
    }
}

uint8_t is_bdhm(char reg) {
    switch (reg) {
    case 'b':
    case 'B':
    case 'd':
    case 'D':
    case 'h':
    case 'H':
    case 'm':
    case 'M':
        return 1;
    }
    return 0;
}

// Looking at the table of instructions, this partitions a
// register into two groups. BDHM and CELA. This is used to
// calculate the offset from the base instruction vertically
uint8_t bdhm_cela_multiplier(char reg) {
    switch (reg) {
    case 'b':
    case 'B':
    case 'c':
    case 'C':
        return 0x00;
    case 'd':
    case 'D':
    case 'e':
    case 'E':
        return 0x10;

    case 'h':
    case 'H':
    case 'l':
    case 'L':
        return 0x20;
    case 'a':
    case 'A':
    case 'm':
    case 'M':
        return 0x30;
    }
}

uint8_t inc_reg_dest_offset(char reg) {

    switch (reg) {
    case 'b':
    case 'B':
    case 'h':
    case 'H':
        return 0x04;
    case 'a':
    case 'A':
    case 'l':
    case 'L':
        return 0x0C;
    }
    return 0x00;
}

struct instruction {
    char mnemonic[6];
    uint8_t opcode;
};

#define X(name, value)                                     \
    { #name, value }

// Base instructions
struct instruction instructions[] = {
    X(LD, 0x40),  X(ADD, 0x80), X(OR, 0xB0),  X(AND, 0xA0),
    X(SUB, 0x90), X(ADC, 0x88), X(SBB, 0x98), X(XOR, 0xA8),

    X(HLT, 0x76), X(ADI, 0xC6),

    X(CMP, 0xB8), X(CPI, 0xFE)};

static inline struct instruction *
lookup_base_mnem(char *mnemonic) {

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

size_t calculate_machine_code_len(struct ast *ast,
                                  struct symbol_table *st) {
    size_t len = 0;

    // Naive, 3 bytes per line
    // TODO: Calculate storage size of db values
    len += 3 * ast->total;

    // Add HLT
    len += 1;

    return len;
}

struct assembly {
    uint8_t *buffer;
    uint16_t len;
};

#define WRITE_OPERAND(memory, operand)                     \
    memory[address++] = (operand >> 8) & 0xFF;             \
    memory[address++] = operand;

#define WRITE_OPERAND_STR(memory, opvalue)                 \
    uint16_t operand = htoi(opvalue);                      \
    memory[address++] = (operand >> 8) & 0xFF;             \
    memory[address++] = operand;

#define WRITE_NOOPERAND(memory)                            \
    memory[address++] = 0x00;                              \
    memory[address++] = 0x00;

#define WRITE_OPCODE(memory, opcode)                       \
    memory[address++] = opcode;

// TODO: JMPs using symbol table lookup.
// Calculate address using line * (instruction
// size + operand sizes)
struct assembly *translate(struct ast *ast,
                           struct symbol_table *st) {

    struct assembly *build =
        xmalloc(sizeof(struct assembly));

    // Allocate buffer for assembly
    build->len = calculate_machine_code_len(ast, st);
    build->buffer = xmalloc(build->len);

    memset(build->buffer, 0, build->len);

    uint8_t *memory = build->buffer;
    uint16_t address = 0;

    // Fixed lookup for ADD/ADI, CMP/CPI etc
    uint8_t immediate_lookup[255];
    immediate_lookup[0xb8] = 0xfe; // ADD -> ADI
    immediate_lookup[0x80] = 0xc6; // CMP -> CPI
    immediate_lookup[0x90] = 0xDE; // SUB -> SBI
    immediate_lookup[0x88] = 0xCE; // ADC -> ACI
    immediate_lookup[0xA0] = 0xE6; // AND -> ADI
    immediate_lookup[0xB0] = 0xF6; // OR  -> ORI
    immediate_lookup[0xA8] = 0xEE; // XOR -> XRI

    for (size_t i = 0; i < ast->total; i++) {

        struct ast_node *n = &ast->nodes[i];
        struct ast_operand *op = &n->operands[0];
        struct ast_operand *op2 = &n->operands[1];

        // Handle LD X,X register to register or value
        if (EQUALS(n->opcode, "LD")) {

            ASSERT_ARG_COUNT(n, 2, "LD");

            struct instruction *inst =
                lookup_base_mnem(n->opcode);

            // LDI
            if (op2->type == TVALUE) {
                uint8_t opcode = 0;

                if (is_bdhm(op->value[0])) {
                    opcode += 0x06;
                } else {
                    opcode += 0x0E;
                }
                opcode +=
                    bdhm_cela_multiplier(op->value[0]);
                printf("%4x: ldi(%x) %c %s \n", address,
                       opcode, op->value[0], op2->value);

                WRITE_OPCODE(memory, opcode);
                WRITE_OPERAND_STR(memory, op->value);
            } else {
                // Row multiplier for inst grid.
                uint8_t dst =
                    bdhm_cela_multiplier(op->value[0]);
                uint8_t src = reg_to_offset(op2->value[0]);

                // Right hand side
                if (is_cela(op->value[0])) {
                    src += 0x08;
                }

                uint8_t opcode = inst->opcode + src + dst;

                printf("%4x: ld%c(%x) %c  \n", address,
                       op->value[0], opcode, op2->value[0]);

                WRITE_OPCODE(memory, opcode);
                WRITE_NOOPERAND(memory);
            }

        } else if (EQUALS(n->opcode, "ADD") ||
                   EQUALS(n->opcode, "ADC") ||
                   EQUALS(n->opcode, "SUB") ||
                   EQUALS(n->opcode, "SBB") ||
                   EQUALS(n->opcode, "CMP") ||
                   EQUALS(n->opcode, "XOR") ||
                   EQUALS(n->opcode, "OR") ||
                   EQUALS(n->opcode, "ANA")) {

            ASSERT_ARG_COUNT(n, 1, n->opcode);

            struct instruction *inst =
                lookup_base_mnem(n->opcode);

            uint8_t i_code = immediate_lookup[inst->opcode];

            // Intermediate value
            if (op->type == TVALUE) {

                uint16_t opcode = i_code;

                printf("%4x: %s(%x) %s\n", address,
                       inst->mnemonic, opcode, op->value);

                WRITE_OPCODE(memory, opcode);
                WRITE_OPERAND_STR(memory, op->value);

            } else {
                uint8_t dst = reg_to_offset(op->value[0]);
                uint8_t opcode = inst->opcode + dst;

                printf("%4x: %s%c(%x) \n", address,
                       inst->mnemonic, op->value[0],
                       opcode);

                WRITE_OPCODE(memory, opcode);
                WRITE_NOOPERAND(memory);
            }
        } else if (EQUALS(n->opcode, "INC") ||
                   EQUALS(n->opcode, "DEC")) {

            // INRA 0x3C INRL 0x2C
            // INRB 0x04 INRH 0x24
            // DECA 0x3D DECL 0x2D
            // DECB 0x05 DECH 0x25
            if (op->type != TREGISTER) {
                // TODO: This should be done in the
                // parser?
                SYNTAX_ERROR("Invalid operand");
            }

            uint8_t opcode =
                inc_reg_dest_offset(op->value[0]);

            if (EQUALS(n->opcode, "DEC")) {
                opcode += 1;
            }

            opcode += reg_to_offset(op->value[0]);
            printf("%4x: %s%c(%x) \n", address, n->opcode,
                   op->value[0], opcode);

            WRITE_OPCODE(memory, opcode);
            WRITE_NOOPERAND(memory);
        }
    }

    // TODO:
    // * JMP/JZ/JNZ/JPO/JPE etc.
    // * CALL
    // * RET
    // * PUSH/POP
    // * IN/OUT
    // * XCHG
    // * XRA
    // * ORA
    // * INR DCR (A/B/H/L)
    // * STAX
    // * INX
    // * LXI SP/H
    // * CMP

    // Write HLT
    struct instruction *inst = lookup_base_mnem("HLT");
    WRITE_OPCODE(memory, inst->opcode);
    WRITE_NOOPERAND(memory);

    return build;
}

void free_assembly(struct assembly *build) {
    free(build->buffer);
    free(build);
}

void print_ast(struct ast *ast) {

    for (size_t i = 0; i <= ast->total; i++) {
        struct ast_node *n = &ast->nodes[i];
        char *val =
            n->type == AST_LABEL ? n->label : n->opcode;

        printf("%zu: %5s ", i, val);
        printf("(%zu)\t", n->argn);

        for (size_t j = 0; j < n->argn; j++) {
            char type = '\0';
            if (n->operands[j].type == TREGISTER) {
                type = 'R';
            } else if (n->operands[j].type == TADDR) {
                type = '*';
            } else if (n->operands[j].type == TREGADDRESS) {
                type = 'I';
            } else {
                type = 'V';
            }
            printf("%c: %s\t", type, n->operands[j].value);
        }
        printf("\n");
    }
}

void print_symbols(struct symbol_table *st) {

    for (size_t x = 0; x <= MAX_SYMBOLS; x++) {
        if (strlen(st->symbols[x].name) > 1) {
            printf("S: %s Line: %zu\n", st->symbols[x].name,
                   st->symbols[x].line);
        }
    }
}

int main(int argc, char *argv[argc + 1]) {
    if (argc < 2) {
        die("no assembly file specified");
    }

    if (argc < 3) {
        die("no output file specified");
    }

    printf("Parsing %s\n", argv[1]);

    char *buffer;
    load_file(argv[1], &buffer);
    xfree(buffer);

    struct token *root;
    root = tokenize(buffer);

    struct ast *ast = parse_tokens(root);
    struct symbol_table *st = build_symbol_table(ast);

    printf("\nAST\n");
    print_ast(ast);
    printf("\nSymbols\n");
    print_symbols(st);
    printf("\nMachine code\n");

    struct assembly *build = translate(ast, st);
    print_memory(build->buffer, build->len);

    free_symbol_table(st);

    free(ast);
    free_list(root);
    free_assembly(build);

    return 0;
}
