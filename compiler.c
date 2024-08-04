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

struct ast_node {
    // Opcode
    char opcode[255];
    char label[255];

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

struct assembly {
    uint8_t *buffer;
    int16_t buf_len;
};

struct ast *parse_tokens(struct token *root) {
    struct token *tmp = root;
    struct ast *ast = xmalloc(sizeof(struct ast));

    memset(ast, 0, sizeof(struct ast));

    do {

        if (tmp->type == TINST) {
            struct ast_node *n = &ast->nodes[ast->total++];
            strcpy(n->opcode, tmp->s_val);

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

        } else if (tmp->type == TOPERAND) {
        } else if (tmp->type == TDIR) {
        } else if (tmp->type == TLABEL) {
            struct ast_node *n = &ast->nodes[ast->total++];
            strcpy(n->label, tmp->s_val);
        } else if (tmp->type == TADDR) {
        }
    } while ((tmp = tmp->next) != NULL);

    return ast;
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

    printf("Parsing %s\n", argv[1]);

    char *buffer;
    load_file(argv[1], &buffer);
    xfree(buffer);

    struct token *root;
    root = tokenize(buffer);

    struct ast *ast = parse_tokens(root);

    for (size_t i = 0; i <= ast->total; i++) {
        struct ast_node *n = &ast->nodes[i];
        printf("%5s %s ", n->opcode, n->label);
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

    free(ast);
    // 1. Tokenize.
    // 2. Parse and build the AST
    // 3. Generate symbol table
    // 4. Generate binary output

    // Parse now.
    free_list(root);

    return 0;
}
