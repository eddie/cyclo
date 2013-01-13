#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

void*
die(const char *fmt, ...)
{
  va_list args;
  va_start(args,fmt);

  vfprintf(stderr,fmt,args);
  fprintf(stderr,"\n");
  va_end(args);
  exit(1);
}

#define OPCODE(x) //printf(x"\n");

typedef unsigned char uint8_t;

// Remember, addresses are 16bit and datapaths are 8bits
// The operand is 16bits for 16bit addressing

typedef struct {
  
  uint8_t memory[65536];
  int16_t pc;
  
  uint8_t accumulator;
  uint8_t status;   

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
  
  uint8_t ir;

} machine;

void 
write_memory(machine *m,int16_t address, uint8_t value)
{
  if(address >= 65536) {
    die("Seg Fault!\n");
  }

  m->memory[address] = value;
}

uint8_t
read_memory(machine *m, int16_t address)
{
  if(address >= 65536) {
    die("Seg fault!");
  }

  return m->memory[address];
}

void 
load_program(machine *m, uint8_t *data,int length)
{
  if(!data) {
    die("No program, halting!");
  }

  memcpy(&m->memory,data,length);
}

void 
print_machine_status(machine *m)
{
  printf("Accumulator: %u\n", m->accumulator);

  printf("Carry: %u", (m->status >> 2) & 1);
  
  putchar('\n');
}


void 
run(machine *m)
{
  m->accumulator = 0;
  m->status = 0;
  m->pc = 0x0;
  
  int running = 1;
  
  uint8_t opcode,oplow,ophigh;
  int16_t operand;
  uint8_t opvalue;

  while(running)
  {
    
    // 3 Cycle Fetch
    opcode = read_memory(m,m->pc++);
    ophigh = read_memory(m,m->pc++);
    oplow  = read_memory(m,m->pc++);
  
    operand = (ophigh << 8) + oplow;
    opvalue = read_memory(m,operand);

    switch(opcode){

      case 0x00:
        // TODO: This really needs checking.. >255 / >= 255
        if((m->accumulator + opvalue) > 255){
          m->status |= (1 << 2);
        }

        m->accumulator += opvalue;
        OPCODE("ADD");
        break;

      case 0x01:
        OPCODE("ADC")
        m->accumulator += opvalue + ( (m->status >> 2) & 1 ); // TODO: Test this 
        break;
    
      case 0x02:
        m->accumulator -= opvalue;
        OPCODE("SUB")
        break;

      case 0x08:
        m->accumulator = oplow;
        OPCODE("LDI")
        break;

      case 0x07:
        m->accumulator = ~m->accumulator;
        OPCODE("NOT")
        break;

      case 0x04:
        OPCODE("AND")
        m->accumulator &= opvalue;
        break;
      
      case 0x05:
        OPCODE("OR")
        m->accumulator |= opvalue;
        break;
    
      case 0x06:
        OPCODE("XOR")
        m->accumulator ^= opvalue;
        break;

      case 0x09:
        OPCODE("LDM")
        m->accumulator = opvalue;
        break;

      case 0x0A:
        OPCODE("STM")
        write_memory(m,operand,m->accumulator);
        break;
      
      case 0x0B:
        OPCODE("JMP")
        m->pc = operand;
        break;

      case 0x0C:
        OPCODE("JPI")
        m->pc = opvalue;
        break;

      case 0x0D:
        OPCODE("JPZ")
        if( m->status & 1 ) {
          m->pc = operand;
        }
        break;
      
      case 0x0E:
        OPCODE("JPM")
        if( (m->accumulator >> 8) & 1 ) {
          m->pc = operand;
        }
        break;
      
      case 0x0F:
        OPCODE("JPC")
        if( (m->status >> 2) & 1 ) {
          m->pc = operand;
        }
        break;

      case 0x10:
        running = 0;
        OPCODE("HLT");
        break;

    }
  
    print_machine_status(m);
  }
}

void
load_file(machine *m,char *path)
{
  if(strlen(path) <= 0) {
    die("load_file: no path to load");
  }

  FILE *file;
  char *buffer;
  long length;

  file = fopen(path,"rb");

  if(!file) {
    die("load_file: file doesn't exist");
  }
  
  fseek(file,0,SEEK_END);
  length = ftell(file);
  fseek(file,0,SEEK_SET);
  
  buffer = malloc(length+1);
  fread(buffer, 1,length, file);
  
  fclose(file);

  memcpy(&m->memory,buffer,length);
}


int 
main(int argc, char **argv)
{
  machine m;
  /*
  uint8_t *program = malloc(sizeof(uint8_t)*100);
  
  // Program to count up to 255
  program[0] = 0x08;
  program[1] = 0x00;
  program[2] = 0x00;
  
  program[3] = 0x00; // A
  program[4] = 0x00;
  program[5] = 0x14;
  
  program[6] = 0x0F; 
  program[7] = 0x00;
  program[8] = 0x0C;  // JPC B

  program[9] = 0x0B;
  program[10] = 0x00;
  program[11] = 0x03; // JMP A

  program[12] = 0x10;

  // Stack Memory
  program[20] = 0x0A;
  program[21] = 0x03;
    load_program(&m,buffer,100);
  */

  
  char *buffer;
  load_file(&m,"hello.out");
  free(buffer);

  run(&m);
}

