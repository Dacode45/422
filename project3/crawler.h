#ifndef __CRAWLER_H
#define __CRAWLER_H

#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct __node_t {
  char* src;
  char* content;
  struct __node_t *next;
} node_t;

void Node_Free(node_t *node);

// First node in queue has no link
typedef struct __queue_t {
  node_t *head;
  node_t *tail;
  pthread_mutex_t lock;
  sem_t empty; // max size of the queue - number of elements in queue
  sem_t full; // number of elements in queue
  size_t bounded; // Indicates if the queue has a max size and what the max size is.
  // Should this queue cause the program to exit when an error occurs.
  // also the exit value.
  char * queue_name;
  char const * err_msg;
  size_t panic_on_error;
} queue_t;

int Queue_Init(queue_t *q, size_t size, size_t panic_on_error);
int Queue_Enqueue(queue_t *q, node_t* node);
int Queue_Dequeue(queue_t *q, node_t **node);
void Queue_Print(queue_t *q);

int crawl(
  char *start_url,
  int download_workers,
  int parse_workers,
  int queue_size,
  char * (*fetch_fn)(char *link),
  void (*edge_fn)(char *from, char *to)
);

node_t* ParseLinks(node_t* content);

#endif
