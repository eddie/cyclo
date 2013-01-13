#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/*
 * v0.0 Assembler for Cyclo CPU
 * 
 * This is not ment to be pretty or efficient, it just needs to work.
 *
 * eblundell@gmail.com 2013
 *
 */

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

void*
xmalloc(size_t size)
{
  void *ptr;

  if(size == 0){
    die("xmalloc: Zero size\n");
  }

  ptr = malloc(size);
  
  if(ptr == NULL){
    die("xmalloc: out of memory (allocating %zu bytes)\n",size);
  }
  
  return ptr;
}

void
xfree(void *ptr)
{
  if(ptr == NULL){
    die("xfree: NULL pointer given as argument \n");
  }
}

typedef unsigned char uint8_t;

int hexchar_to_int(char c)
{
  int i = 0;
  c = toupper(c);
  
  if(c >= 'A' && c <= 'F')
    return 10 + ((c-17)-'0');

  return 0;
}

int htoi(const char s[])
{

  int i,n,t;
  i = n = t=0;

  if(s[i] == '0'){
    ++i;
    if(s[i] == 'x' || s[i] == 'X')
      ++i;
    else
      --i;
  }

  while(s[i] != '\0'){

    n = n * 16;
    if(isdigit(s[i])){
      n += s[i] - '0';
      
    }else{
      if((t = hexchar_to_int(s[i])))
        n += t;
      else
        return 0;
    }
    
    ++i;
  }
    
  return n;
}



struct token{

  char s_val[255];
  int i_val;
  int type;
  int curs;
  
  struct token *next;
};


void 
add_list(struct token *root,struct token *next)
{
  if(!root){
    return;
  }

  struct token *tmp;
  
  if(root->next == NULL){
    root->next = next;
    return;  
  }
  
  tmp = root->next;
  while(tmp->next != NULL){
    tmp = tmp->next;
  }

  tmp->next = next;
}

struct token *
create_token(struct token *root, int type)
{
  struct token *tmp = xmalloc(sizeof(struct token));
  
  tmp->next = NULL;
  tmp->type = type;
  tmp->i_val = 0;
  tmp->curs = 0;

  if(root){
    add_list(root,tmp);
  }

  return tmp;
}

void 
astrval(struct token *t, char c)
{
  if(t == NULL){
    return;
  }
  t->s_val[t->curs++] = c;
  t->s_val[t->curs+1] = '\0';
}

void 
free_list(struct token *root){

  if(!root){
    return;
  }

  struct token *t = NULL;

  while(root != NULL){

    t = root->next;
    free(root);
    root = t;
  }
}


enum state { PROGRAM,INSTRUCTION,OPERAND,LABEL,COMMENT,DIRECTIVE,ADDR};
enum tokens{ TINST,TOP,TLABEL,TDIR ,TADDR};

void 
dump_list(struct token *root)
{
  struct token *tmp = root;

  do{

    if(tmp->type == TINST){
      printf("Instruction: %s\n",tmp->s_val);
    }else if(tmp->type == TOP){
      printf("Operand: %s\n",tmp->s_val);
    }else if(tmp->type == TDIR){
      printf("Directive: %s\n",tmp->s_val);
    }else if(tmp->type == TLABEL){
      printf("Label: %s\n",tmp->s_val);
    }else if(tmp->type == TADDR){
      printf("Address: %s\n",tmp->s_val);
    }
  }while((tmp = tmp->next) != NULL);

}

void
load_file(char *path, char **buffer)
{
  if(strlen(path) <= 0) {
    die("load_file: no path to load");
  }

  FILE *file;
  long length;

  file = fopen(path,"r");

  if(!file) {
    die("load_file: file doesn't exist");
  }
  
  fseek(file,0,SEEK_END);
  length = ftell(file);
  fseek(file,0,SEEK_SET);
  
  *buffer = xmalloc(length+1);
  fread(*buffer, 1,length, file);
  
  fclose(file);
}


struct token*
tokenize(char *buffer)
{
  int state = PROGRAM;
  int line = 0;

  struct token *t_tmp, *t_root;
  char c,*p;

  t_root = create_token(NULL,-1);

  for(p = buffer; p != buffer+strlen(buffer); p++) {

    c = *p;

    if(c == '#') {

      state = COMMENT;

    } else if(c == '.') {
      
      if(state == PROGRAM){

        state = DIRECTIVE;
        t_tmp = create_token(t_root,TDIR);

      }

    }else if(c == ' ') {

      if(state == INSTRUCTION || state == DIRECTIVE) {

        state = OPERAND;
        t_tmp = create_token(t_root,TOP);

      }

    }else if(isalpha(c) || isdigit(c)) {

      if(state == COMMENT) continue;

      if(state == PROGRAM) {

        state = INSTRUCTION;
        t_tmp = create_token(t_root,TINST);

      }

      astrval(t_tmp,c);

    }else if(c == ':') {

      if(state == INSTRUCTION) {

        t_tmp->type = TLABEL;
        state = PROGRAM;

      }

    }else if(c == '[') {

      if(state = OPERAND){

        state = ADDR;
        t_tmp->type = TADDR;
      }

    }else if(c == '\n') {

      state = PROGRAM;
      line++;

    }

  }

  return t_root; 
}

int16_t 
calculate_code_size(struct token *root)
{
  int16_t base;
  base = 0x00;

  while(root){

    if( root->type == TINST ){
      base += 0x18;
    }
    root = root->next;
  }
  
  return base;
}

int16_t 
calculate_data_size(struct token *root)
{
  int16_t base;
  base = 0x00;

  while(root){

    if( root->type == TOP ){

      // We only want to increase data size 
      // for non jump instructions (except JPI)
      // and HLT

      // Not proud of this code. Needs changing.
      if( (strcasecmp("ADD",root->s_val) == 0) ||
          (strcasecmp("ADC",root->s_val) == 0) ||
          (strcasecmp("SUB",root->s_val) == 0) ||
          (strcasecmp("SBC",root->s_val) == 0) ||
          (strcasecmp("AND",root->s_val) == 0) ||
          (strcasecmp("OR", root->s_val) == 0) ||
          (strcasecmp("XOR",root->s_val) == 0)) {
        
        
        base += 0x08;
      }
    }
    root = root->next;
  }
  
  return base;
}


int8_t 
mnemonic_to_bytecode(char *mnemonic){

  // strcasecmp
  if(strcasecmp(mnemonic,"ADD")==0) return 0x00;
  if(strcasecmp(mnemonic,"ADC")==0) return 0x01;
  if(strcasecmp(mnemonic,"SUB")==0) return 0x02;
  if(strcasecmp(mnemonic,"SBC")==0) return 0x03;
  if(strcasecmp(mnemonic,"AND")==0) return 0x04;
  if(strcasecmp(mnemonic,"OR" )==0) return 0x05;
  if(strcasecmp(mnemonic,"XOR")==0) return 0x06;
  if(strcasecmp(mnemonic,"NOT")==0) return 0x07;
  if(strcasecmp(mnemonic,"LDI")==0) return 0x08;
  if(strcasecmp(mnemonic,"LDM")==0) return 0x09;
  if(strcasecmp(mnemonic,"STM")==0) return 0x0A;
  if(strcasecmp(mnemonic,"JMP")==0) return 0x0B;
  if(strcasecmp(mnemonic,"JPI")==0) return 0x0C;
  if(strcasecmp(mnemonic,"JPZ")==0) return 0x0D;
  if(strcasecmp(mnemonic,"JPM")==0) return 0x0E;
  if(strcasecmp(mnemonic,"JPC")==0) return 0x0F;
  if(strcasecmp(mnemonic,"HLT")==0) return 0x10;

  die("mnemonic unknown");
  return -1; // Never reached keeps compiler quiet
}

char*
assemble(struct token *root)
{
  if(!root) {
    die("assemble: no tokens");
  }

  int16_t data_offset = calculate_code_size(root);
  int16_t data_length = calculate_data_size(root);
  data_length = 200;

  printf("Data Offset: %d Length:%d\n",data_offset,data_length);

  int16_t pc = 0x0;
  int16_t dc = data_offset;

  int8_t *memory = xmalloc(data_offset * sizeof(int8_t) + data_length); 

  int8_t byte_code = 0x0;
  int16_t operand = 0x0;

  struct token *tmp;

  while(root) {

    if(root->type == TINST) {
      byte_code = mnemonic_to_bytecode(root->s_val);
      memory[pc++] = byte_code;

      // Have an operand for instruction
      if(root->next) {
        tmp = root->next;

        // It needs to be stored and an address returned
        if(tmp->type == TOP) {
          operand = htoi(root->next->s_val);

          memory[pc++] = (int8_t)dc;            // Store the memory location
          memory[dc++] = (int8_t)operand >> 8;  // Store high data in memory
          memory[dc++] = (int8_t)operand;       // Store low data in memory
        }
      }
    }

    root = root->next;
  }

  // Calculate estimate size of buffer from
  // token count.

  // Calculate Data offset

  // Loop through tokens, convert mnemonics to bytecode
  // and add to buffer

  // If opcode not hex, resolve address from label

  // Take operands, convert values and store in data, work out
  // offset and store offset as operand

  // Return buffer

  return (char*)0;
}


int
main(int argc, char **argv)
{
  
  char *buffer;
  load_file("hello.asm",&buffer);

  struct token *root;
  root = tokenize(buffer);
  xfree(buffer);

  dump_list(root);

  char *output;
  output = assemble(root);
  
  if(output)
    free(output);

  free_list(root);

  return 0;
}

