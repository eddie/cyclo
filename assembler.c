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

void 
dump_buffer(char *path, char *buffer, int length)
{
  if(!path) {
    die("dump_buffer: no path specified");
  }

  if(!buffer) {
    die("dump_buffer: no buffer");
  }

  if(length <=0) {
    die("dump_buffer: zero length buffer");
  }

  FILE *file;
  file = fopen(path,"wb");

  if(!file) {
    die("dump_buffer: couldn't open %s for writing",path);
  }

  fwrite(buffer,length,1,file);
  fclose(file);
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

      // TODO: Only append value on directive,instruction or operand
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
      base += 3;  // 3 Bytes per instruction
    }
    root = root->next;
  }
  
  return base;
}

int
is_intermediate(char *mnemonic)
{
  if( (strcasecmp("ADD",mnemonic) == 0) ||
      (strcasecmp("ADC",mnemonic) == 0) ||
      (strcasecmp("SUB",mnemonic) == 0) ||
      (strcasecmp("SBC",mnemonic) == 0) ||
      (strcasecmp("AND",mnemonic) == 0) ||
      (strcasecmp("OR", mnemonic) == 0) ||
      (strcasecmp("XOR",mnemonic) == 0)) {
    
    return 0;
  }
  
  return 1;        
}

int16_t 
calculate_data_size(struct token *root)
{
  int16_t base;
  base = 0x00;

  while(root){

    if( root->type == TINST ){

      // We only want to increase data size 
      // for non jump instructions (except JPI)
      // and HLT

      if(!is_intermediate(root->s_val)){
        base += 1;
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

int16_t
lookup_label_address(struct token *root, char *label)
{
  if(!root) {
    die("lookup_label_address: no tokens");
  }

  while(root) {

    if(root->type == TLABEL){
      if( strcasecmp(label,root->s_val) == 0 ){
        return (int16_t) root->i_val;
      }
    }

    root = root->next;
  }

  die("lookup_label_address: label not found");
}

char*
assemble(struct token *tokens)
{
  if(!tokens) {
    die("assemble: no tokens");
  }

  int16_t data_offset = calculate_code_size(tokens);
  int16_t data_length = calculate_data_size(tokens);

  int16_t pc = 0x0;
  int16_t dc = data_offset;

  int8_t *memory = xmalloc(data_offset * sizeof(int8_t) + data_length); 

  int8_t byte_code = 0x0;
  int16_t operand = 0x0;

  struct token *root = tokens;
  struct token *tmp;

  while(root) {

    if(root->type == TINST) {

      byte_code = mnemonic_to_bytecode(root->s_val);
      memory[pc++] = byte_code;

      // Have an operand for instruction
      if(root->next) {

        tmp = root->next;

        if(tmp->type == TOP) {

          operand = (int16_t) htoi(tmp->s_val);

          if(is_intermediate(root->s_val)){

            memory[pc++] = 0x00;
            memory[pc++] = (int8_t)operand;

          }else{

            memory[pc++] = (int8_t)(dc >> 8) & 0xFF;       // Store high of mem address
            memory[pc++] = (int8_t)dc;            // Store low of mem address
            memory[dc++] = (int8_t)operand;       // Store 8bit value in memory

          }

        }else if(tmp->type == TADDR) {

          memory[pc]   = 0x00;
          memory[pc+1] = 0x00;

          tmp->i_val = (int16_t) pc;
          pc+=2; // 16bits

        }else{

          memory[pc++] = 0x00;
          memory[pc++] = 0x00;

        }

      }

    }else if(root->type == TLABEL) {

      // Store address in i_value of token HACK
      root->i_val = (int16_t) pc;
    }

    root = root->next;
  }

  // Now we need to update label references
  root = tokens;
  int16_t addr;

  while(root) {

    if(root->type == TADDR){

      addr = lookup_label_address(tokens,root->s_val);

      memory[(int8_t)root->i_val]   = (int8_t) (addr >> 8) & 0xFF; // High
      memory[(int8_t)root->i_val+1] = (int8_t) addr;               // Low

    }
    root = root->next;
  }

  printf("Data Offset: %d Length:%d\n",data_offset,data_length);

  dump_buffer("main.out",memory,data_offset * sizeof(int8_t) + data_length);

  free(memory);

  return (char*)0;
}


int
main(int argc, char **argv)
{
  if(argc < 2){
    die("no assembly file specified");
  }

  printf("Assembling %s\n",argv[1]);

  char *buffer;
  load_file(argv[1],&buffer);

  struct token *root;
  root = tokenize(buffer);
  xfree(buffer);

  assemble(root);
 
  free_list(root);

  return 0;
}

