#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "record.h"

void usage(char *prog_name){
   fprintf(stderr,"usage: %s <-i input file name>\n",prog_name);
   exit(1);
}

int main(int argc, char *argv[]){
   char * input_file = "";
   int c;
   opterr = 0;
   while((c = getopt(argc, argv, "i:")) != -1) {
      switch(c) {
         case 'i' :
            input_file = strdup(optarg);
	    break;
	 default :
	    usage(argv[0]);
      }
   }

   int fd = open(input_file, O_RDONLY);
   if(fd < 0){
      perror("open");
      exit(1);
   }
   record_t r;
   while(1) {
      int rc;
      rc = read(fd,&r,sizeof(record_t));
      if(rc < 0){
          perror("read");
          exit(1);
      }
      if (rc == 0) {   //EOF
	 break;
      }
      // outpur the record- key first
      printf("key: %u  data: ",r.key);
      // now the rest of the data
      int j;
      for(j = 0; j < REC_SIZE; j++){
	 printf("%u ",r.data[j]);
      }
      printf("\n");
   }
   (void) close(fd);
   return 0;
}
