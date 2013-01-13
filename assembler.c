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

char*
assemble(struct token *root)
{
  
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

