#include <ctype.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/*
 * v0.0 Assembler for Cyclo CPU
 *
 * This is not ment to be pretty or efficient, it just
 * needs to work.
 *
 * eblundell@gmail.com 2013
 *
 */

enum state {
    PROGRAM,
    INSTRUCTION,
    OPERAND,
    LABEL,
    COMMENT,
    DIRECTIVE,
    ADDR,
};

enum tokens { TINST, TOPERAND, TLABEL, TDIR, TADDR };

struct token {

    char s_val[255];
    int i_val;
    int type;
    int curs;

    struct token *next;
    struct token *prev;
};

struct instruction {
    char mnemonic[5];
    int8_t opcode;
    int8_t args;
};

static struct instruction instructions[] = {
    {"ADD", 0x00},
    {"ADC", 0x01},
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
    {"HLT", 0x10},
    {"JE", 0x11},
    {"CMP", 0x12},

    // Load A,B from memory or immediate
    {"LDA", 0x14},
    {"LDB", 0x15},

    // Translated, Load register
    {"LDAB", 0x2C}, // LD a,b
    {"LDBA", 0x2D}, // LD b,a
};

struct instruction *
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

int is_multi_operand(char *mnemonic) {
    if (strcasecmp(mnemonic, "LD") == 0)
        return 1;
    return 0;
}

// TODO: convert strcasecmp to macro.
struct instruction *get_alias(char *mnemonic,
                              struct token *operand) {
    if (!operand) {
        return 0;
    }

    // Translate LDA to LDAB or LDAI
    if (strcasecmp(mnemonic, "LDA") == 0) {
        if (strcasecmp(operand->s_val, "b") == 0) {
            return mnemonic_to_instruction("LDAB");
        }
    } else if (strcasecmp(mnemonic, "LDB") == 0) {
        if (strcasecmp(operand->s_val, "a") == 0) {
            return mnemonic_to_instruction("LDBA");
        }
    }

    return 0;
};

void *die(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

void *xmalloc(size_t size) {
    void *ptr;

    if (size == 0) {
        die("xmalloc: Zero size\n");
    }

    ptr = malloc(size);

    if (ptr == NULL) {
        die("xmalloc: out of memory (allocating %zu "
            "bytes)\n",
            size);
    }

    return ptr;
}

void xfree(void *ptr) {
    if (ptr == NULL) {
        die("xfree: NULL pointer given as argument \n");
    }
}

int hexchar_to_int(char c) {
    c = toupper(c);

    if (c >= 'A' && c <= 'F')
        return 10 + ((c - 17) - '0');

    return 0;
}

int htoi(const char s[]) {
    int i, n, t;
    i = n = t = 0;

    if (s[i] == '0') {
        ++i;
        if (s[i] == 'x' || s[i] == 'X')
            ++i;
        else
            --i;
    }

    while (s[i] != '\0') {

        n = n * 16;
        if (isdigit(s[i])) {
            n += s[i] - '0';

        } else {
            if ((t = hexchar_to_int(s[i])))
                n += t;
            else
                return 0;
        }

        ++i;
    }

    return n;
}

void dump_buffer(char *path, uint8_t *buffer, int length) {
    if (!path) {
        die("dump_buffer: no path specified");
    }

    if (!buffer) {
        die("dump_buffer: no buffer");
    }

    if (length <= 0) {
        die("dump_buffer: zero length buffer");
    }

    FILE *file;
    file = fopen(path, "wb");

    if (!file) {
        die("dump_buffer: couldn't open %s for writing",
            path);
    }

    fwrite(buffer, length, 1, file);
    fclose(file);
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

void dump_list(struct token *root) {
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

void load_file(char *path, char **buffer) {
    if (strlen(path) <= 0) {
        die("load_file: no path to load");
    }

    FILE *file;
    long length;

    file = fopen(path, "r");

    if (!file) {
        die("load_file: file doesn't exist");
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);

    *buffer = xmalloc(length + 1);
    fread(*buffer, 1, length, file);

    fclose(file);
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

int16_t calculate_code_size(struct token *root) {
    int16_t base;
    base = 0x00;

    while (root) {

        if (root->type == TINST) {
            base += 3; // 3 Bytes per instruction
        }
        root = root->next;
    }

    return base;
}

int is_direct(char *mnemonic) {
    if ((strcasecmp("STM", mnemonic) == 0) ||
        (strcasecmp("LDM", mnemonic) == 0)) {

        return 1;
    }

    return 0;
}

int16_t calculate_data_size(struct token *root) {
    int16_t base;
    base = 0x00;

    while (root) {

        if (root->type == TINST) {

            // We only want to increase data size
            // for non jump instructions (except JPI)
            // and HLT

            // TODO: Resolve
            if (!is_direct(root->s_val)) {
                base += 1;
            }
        }
        root = root->next;
    }

    return base;
}

int16_t lookup_label_address(struct token *root,
                             char *label) {
    if (!root) {
        die("lookup_label_address: no tokens");
    }

    while (root) {

        if (root->type == TLABEL) {
            if (strcasecmp(label, root->s_val) == 0) {
                return (int16_t)root->i_val;
            }
        }

        root = root->next;
    }

    die("lookup_label_address: label not found");
    return -1;
}

int is_label(char *s_val) {
    return !((s_val[0] == '0') && (s_val[1] == 'x'));
}

struct assembly {
    uint8_t *buffer;
    int16_t buf_len;
};

struct assembly *assemble(struct token *tokens) {
    if (!tokens) {
        die("assemble: no tokens");
    }

    struct token *root = tokens;
    struct assembly *build =
        xmalloc(sizeof(struct assembly));

    int16_t data_offset = calculate_code_size(tokens);
    int16_t data_length = calculate_data_size(tokens);

    int16_t pc = 0x0;
    int16_t dc = data_offset;

    // Allocate buffer for assembly
    build->buf_len =
        data_offset * sizeof(int8_t) + data_length;
    build->buffer = xmalloc(build->buf_len);

    uint8_t *memory = build->buffer;
    int16_t operand = 0x0;

    // Three types of instructions to handle. Direct
    // memory instructions, register based instructions,
    // and address based.

    while (root) {

        if (root->type == TLABEL) {
            // Store address in i_value of token HACK
            root->i_val = (int16_t)pc;
            continue;
        }

        if (root->type == TINST) {

            struct instruction *ins =
                mnemonic_to_instruction(root->s_val);

            struct token *op = root->next;

            if (!ins) {
                die("Unknown instruction %s", root->s_val);
            }

            // Handle single operation, register based
            // translated instructions
            struct instruction *alias =
                get_alias(ins->mnemonic, op);

            // Use the alias if we have one, and skip this
            // operand.
            // TODO: Handle mutliple operands.
            //} else if (op && op->next &&
            //           op->next->type == TOPERAND) {
            //    printf("Got operands %s %s\n",
            //    op->s_val,
            //           op->next->s_val);
            if (op && alias) {
                printf("Have alias: %s\n", alias->mnemonic);
                memory[pc++] = alias->opcode;

                // Skip next operand
                root = root->next;
                continue;

            } else if (op) {
                memory[pc++] = ins->opcode;
            }

            // Store first operand.
            if (op && op->type == TOPERAND) {
                operand = (int16_t)htoi(op->s_val);

                // Check if instruction is direct
                if (is_direct(root->s_val)) {

                    memory[pc++] =
                        (int8_t)(operand >> 8) & 0xFF;
                    memory[pc++] = (int8_t)(operand);

                } else {

                    // Store the value in the data
                    // segment and return
                    memory[pc++] = (int8_t)(dc >> 8) &
                                   0xFF; // Store high of
                                         // mem address
                    memory[pc++] =
                        (int8_t)dc; // Store low of
                                    // mem address
                    memory[dc++] =
                        (int8_t)operand; // Store 8bit value
                                         // in memory
                }

            } else if (op && op->type == TADDR) {

                operand = (int16_t)htoi(op->s_val);

                // Check if address is a label or
                // not
                if (is_label(op->s_val)) {

                    // nop
                    memory[pc] = 0x00;
                    memory[pc + 1] = 0x00;

                    op->i_val = (int16_t)pc;
                    pc += 2; // 16bits

                } else {

                    memory[pc] = (int16_t)memory[operand];
                    pc += 2;
                }

            } else {

                memory[pc++] = 0x00;
                memory[pc++] = 0x00;
            }
        }

        root = root->next;
    }

    // Now we need to update label references
    root = tokens;
    int16_t addr;

    while (root) {

        if (root->type == TADDR && is_label(root->s_val)) {

            addr =
                lookup_label_address(tokens, root->s_val);

            memory[(int8_t)root->i_val] =
                (int8_t)(addr >> 8) & 0xFF; // High
            memory[(int8_t)root->i_val + 1] =
                (int8_t)addr; // Low
        }
        root = root->next;
    }

    printf("Data Offset: %d Length:%d\n", data_offset,
           data_length);

    return build;
}

int free_assembly(struct assembly *build) {
    if (!build) {
        return -1;
    }
    free(build->buffer);
    free(build);
    return 0;
}

int main(int argc, char *argv[argc + 1]) {
    if (argc < 2) {
        die("no assembly file specified");
    }

    if (argc < 3) {
        die("no output file specified");
    }

    printf("Assembling %s\n", argv[1]);

    char *buffer;
    load_file(argv[1], &buffer);
    xfree(buffer);

    struct token *root;
    root = tokenize(buffer);

    dump_list(root);
    struct assembly *build = assemble(root);

    dump_buffer(argv[2], build->buffer, build->buf_len);

    free_list(root);
    free_assembly(build);

    return 0;
}
