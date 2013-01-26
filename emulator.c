#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

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

#define OPCODE(x) printf(x"\n");
#define MAX_DEVICE 4

typedef unsigned char uint8_t;

typedef struct {

  int16_t mem_range[2];
  char name[12];

  void (*write)(int16_t address, uint8_t data);
  uint8_t (*read)(int16_t address);

} device;

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

  int device_count;
  device devices[MAX_DEVICE];

} machine;

device* 
device_from_address(machine *m, int16_t address)
{
  device *d;
  d = &m->devices[0];

  if(d){

    if( (address >= d->mem_range[0]) &&
        (address <= d->mem_range[1])) {

      return d;
    }
  }

  return 0;
}

void 
write_memory(machine *m,int16_t address, uint8_t data)
{
  if(address >= 65536) {
    die("Seg Fault!\n");
  }

  device *d;
  d = device_from_address(m,address);

  if(d) {
    d->write(address,data);
  }

  m->memory[address] = data;
}

uint8_t
read_memory(machine *m, int16_t address)
{
  if(address >= 65536) {
    die("Seg fault!");
  }

  device *d;
  d = device_from_address(m,address);

  if(d) {
    return d->read(address);
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
  printf("\rAccumulator: D:%u H:%02X Carry: %u\n", m->accumulator, m->accumulator, (m->status >> 1) & 1);
}

void
register_device(machine *m, char *d_name, int d_index,
  int16_t m_low, int16_t m_high,
  void (*write)(int16_t address, uint8_t data),
  uint8_t (*read)(int16_t address))
{

  if(d_index > MAX_DEVICE) {
    die("device index out of range");
  }

  device *d = &m->devices[d_index];

  d->mem_range[0] = m_low;
  d->mem_range[1] = m_high;

  strcpy(d->name,d_name);

  d->write = write;
  d->read = read;

  printf("Device registered, Name:%s, Low:%0X, High:%0X\n",d->name,d->mem_range[0],d->mem_range[1]);

  m->device_count++;
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

          m->status |= (1 << 1);
          m->accumulator &= opvalue;

        } else {
          m->accumulator += opvalue;

          if(m->accumulator == 0){
            m->status |= 1;
          }
        }

        OPCODE("ADD");
        break;

      // Should we unset the carry flag after?
      case 0x01:
        OPCODE("ADC")
        m->accumulator += opvalue + ( (m->status >> 1) & 1 ); 
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
        printf("Storing memory: Addr: %04X Value: %04X\n",operand,m->accumulator);
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
        if( (m->status >> 1) & 1 ) {
          m->pc = operand;
        }
        break;

      case 0x10:
        running = 0;
        OPCODE("HLT");
        break;

    }
  
    print_machine_status(m);
    //usleep(1000); // 1MHz/1000 = 1Khz
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
    die("load_file: %s file doesn't exist",path);
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

// TODO: Have video memory

void
video_write(int16_t address, uint8_t data)
{
  //printf("Video written: %0X\n",data);
  putchar(data);
}

uint8_t
video_read(int16_t address)
{

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

  register_device(&m,"video",0,0xA000,0xA7FF,video_write,video_read);

  run(&m);
}

