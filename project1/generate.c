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
   fprintf(stderr,"usage: %s <-s random seed> <-n number of records> <-o output file name>\n",prog_name);
   exit(1);
}


int main(int argc, char * argv[])
{
   // each record should be 40 bytes long
   assert(sizeof(record_t) == 40);

   // passed in options
   int random_seed = 0;
   int num_records = 0;
   char * output_file = "";

   // grab parameters from args list
   int c;
   opterr = 0;  // if non-zero, getopt() prints an error to stderr
   while((c = getopt(argc,argv,"n:s:o:")) != -1)  // -1 if no matching options remain
   {
      switch(c){
	case 'n' :
	   num_records = atoi(optarg);
	   break;
	case 's' :
	   random_seed = atoi(optarg);
           break;
	case 'o' :
	   output_file = strdup(optarg);
	   break;
	default :
	   usage(argv[0]);
      }
   }

   srand(random_seed);
   int fd = open(output_file, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR); //write only, create if doesn't exist, truncate otherwise, set permissions: read,write,and execute
   if(fd < 0){
      perror("open");  //output to stderr
      exit(1);
   }

   // create a record
   record_t r;
   int i;
   for(i = 0; i < num_records;i++){
      r.key = rand() % (unsigned int) 0xFFFFFFFF;
      int j;
      for(j = 0; j < REC_SIZE; j++){
 	r.data[j] = rand() % (int) 1e3;  // 1 * 10^3
      }

      // write syscall to write to output_file
      int rc = write(fd,&r,sizeof(record_t));
      if(rc != sizeof(record_t)) {
        perror("write");
        exit(1);
      }
   }

   // (void) ignores error code returned by close
   (void) close(fd);

   return 0;
}
