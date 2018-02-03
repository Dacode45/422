#ifndef RECORD_H
#define RECORD_H

#define REC_SIZE (9)

typedef struct record {
   unsigned int key;
   unsigned int data[REC_SIZE];
} record_t;

#endif
