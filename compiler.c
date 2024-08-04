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
    strcpy(sym->name, name);
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

void dump_tokens(struct token *root) {
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

// Following a common pattern in the 8080 instruction state
// some src/dest pairs have a fixed pattern. .eg A Last, B
// first. This can be reused.
uint8_t reg_to_offset(char reg) {
    switch (reg) {
    case 'a':
    case 'A':
        return 0x07;
    case 'b':
    case 'B':
        return 0x00;
    case 'h':
    case 'H':
        return 0x04;
    case 'l':
    case 'L':
        return 0x05;
    }
    return 0x00;
}

// first operand to offset from base instruction
// e.g mov a,b where does mova start from mov.
// relative to first base instruction
// mov = 0x40
uint8_t ld_reg_dest_offset(char reg) {

    // LD
    switch (reg) {
    case 'a':
    case 'A':
        return 0x38;
    case 'b':
    case 'B':
        return 0x40;
    case 'h':
    case 'H':
    case 'l':
    case 'L':
        return 0x28;
    }
    return 0x00;
}

uint8_t ldi_reg_dest_offset(char reg) {

    uint8_t base = 0x06;

    // LDI base 0x06
    switch (reg) {
    case 'b':
    case 'B':
        return 0x06;
    case 'a':
    case 'A':
        return 0x3E;
    case 'h':
    case 'H':
        return 0x26;
    case 'l':
    case 'L':
        return 0x2E;
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
    X(LD, 0x40),   X(ADD, 0x80),

    X(ADDA, 0x87), X(ADDB, 0x80),
    X(ADDH, 0x84), X(ADDL, 0x85),

    X(ADI, 0xC6)};

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

// TODO: JMPs using symbol table lookup.
// Calculate address using line * (instruction
// size + operand sizes)
void translate(struct ast *ast, struct symbol_table *st) {

    uint16_t address = 0;

    for (size_t i = 0; i < ast->total; i++) {

        struct ast_node *n = &ast->nodes[i];
        struct ast_operand *op = &n->operands[0];
        struct ast_operand *op2 = &n->operands[1];

        // Handle LD X,X register to register or value
        if (EQUALS(n->opcode, "LD")) {

            ASSERT_ARG_COUNT(n, 2, "LD");

            struct instruction *inst =
                lookup_base_mnem("LD");

            // LDI
            if (op2->type == TVALUE) {
                uint8_t dst =
                    ldi_reg_dest_offset(op->value[0]);
                uint8_t opcode = dst;
                printf("%4x: ldi(%x) %c %s \n", address,
                       opcode, op->value[0], op2->value);
            } else {
                uint8_t dst =
                    ld_reg_dest_offset(op->value[0]);
                uint8_t src = reg_to_offset(op2->value[0]);
                uint8_t opcode = inst->opcode + src + dst;

                printf("%4x: ld%c(%x) %c  \n", address,
                       op->value[0], opcode, op2->value[0]);
            }

            address += 24;
        } else if (EQUALS(n->opcode, "ADD")) {

            ASSERT_ARG_COUNT(n, 1, "ADD");

            // ADD x -> ADI
            if (op->type == TVALUE) {

                struct instruction *inst =
                    lookup_base_mnem("ADI");

                printf("%4x: addi(%x) %s\n", address,
                       inst->opcode, op->value);

            } else {
                struct instruction *inst =
                    lookup_base_mnem("ADD");
                uint8_t dst = reg_to_offset(op->value[0]);
                uint8_t opcode = inst->opcode + dst;

                printf("%4x: add%c(%x) \n", address,
                       op->value[0], opcode);
            }

            address += 24;
        }
    }
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

    translate(ast, st);

    free_symbol_table(st);

    free(ast);
    free_list(root);

    return 0;
}
