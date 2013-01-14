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
  printf("\rAccumulator: D:%u H:%02X Carry: %u\n", m->accumulator, m->accumulator, (m->status >> 2) & 1);
}

// TODO: Update flags on Cyclo
// TODO: Implement Sub with carry
// TODO: Allow simulated clock speed 

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
        if((m->accumulator + opvalue) > 255){
          m->status |= (1 << 2);
        }

        m->accumulator += opvalue;

        if(m->accumulator == 0){
          m->status |= (1 << 1);
        }
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

      case 0x03:
        OPCODE("SBC")
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
    usleep(1000); // 1MHz/1000 = 1Khz
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

void
dump_memory(machine *m)
{
  int i;

  for(i=0; i < 200; i++){
    if((i % 0x10)==0){
      printf("\n %04X | ",i);
    }
    printf("%02X ",m->memory[i]);
  }
}


int 
main(int argc, char **argv)
{
  if(argc < 2){
    die("no program specified");
  }

  machine m;

  char *buffer;
  printf("Loading program %s\n",argv[1]);
  load_file(&m,argv[1]);
  free(buffer);

  // TODO: Add dump flag
  //dump_memory(&m);

  run(&m);
}

