#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "record.h"
#include <sys/types.h>
#include <sys/stat.h>

void usage(char * prog_name)
{
  fprintf(stderr, "usage: %s <-i inputfile> <-o outputfile> <-t threshold> (<-g greater than>)\n", prog_name);
  exit(1);
}


int cmpfunc (const void * a, const void * b) {
  record_t* x = (record_t*) a;
  record_t* y = (record_t*) b;
  int i = 0;
  if (x->key == y->key) {
    for (i = 0; i < REC_SIZE; i++) {
      if (x->data[i] != y->data[i]) {
        return (int)(x->data[i] - y->data[i]);
      }
    }
  }
  return (int)(x->key - y->key);
}

int main(int argc, char * argv[]) {
  // each record should be 40 bytes long
  assert(sizeof(record_t) == 40);

  // passed in option
  char* input_file = "";
  char* output_file = "";
  unsigned int threshold = 0;
  int greater_than = 0;

  // grap parameters
  int c;
  opterr = 0;
  while((c = getopt(argc, argv, "i:o:t:g")) != -1)
  {
    switch(c) {
      case 'i':
        input_file = strdup(optarg);
        break;
      case 'o':
        output_file = strdup(optarg);
        break;
      case 't':
        threshold = atoi(optarg);
        break;
      case 'g':
        greater_than = 1;
        break;
      default:
        usage(argv[0]);
    }
  }
  // Load records
  FILE *f = fopen(input_file, "rb");
  if (f == NULL) {
    perror("can't open input file");
    exit(1);
  }
  struct stat st;
  stat(input_file, &st);
  long fsize = st.st_size;
  long num_records = fsize/sizeof(record_t);

  record_t* records = (record_t*) malloc(fsize);
  if (records == NULL) {
    perror("out of memory");
    exit(1);
  }
  fread(records, fsize, 1, f);
  fclose(f);


  // Sort records
  qsort(records, num_records, sizeof(record_t), cmpfunc);

  // Write records
  f = fopen(output_file, "w+");
  if (f == NULL) {
    perror("can't open output file");
    exit(2);
  }
  int i = 0;
  for (i = 0; (i < num_records); i++ ) {
    if (greater_than) {
      if (records[i].key >= threshold) {
        fwrite(records+i,sizeof(record_t), 1, f);
      }
    }else {
      if (records[i].key <= threshold) {
        fwrite(records+i,sizeof(record_t), 1, f);
      }
    }
  }

  fclose(f);
  free(records);
  return 0;
}
