#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#include <limits.h>
#include <regex.h>

#include "crawler.h"

const int MALLOC_ERR = -22;
// Whether there is content parsing.
// Prevents downloader threads from exiting while parsers run.
sem_t contents_parsing;
// similar to the above but parsers wait;
sem_t links_fetching;

char * strcpy_alloc(char* str) {
  char *tmp = (char*)malloc(strlen(str));
  if (tmp == NULL) {
    return NULL;
  }
  strcpy(tmp, str);
  return tmp;
}

typedef struct __crawler_arg {
  queue_t* link_q;
  queue_t* content_q;
  // Use our concurrent queue for when threads finish
  // src of a node is the thread_index
  // content of a node is the return value
  queue_t* finished_q;
  node_t* retv;
  char * (*fetch_fn)(char *link);
  void (*edge_fn)(char *from, char *to);
} crawler_arg;

void *downloader(void *raw_arg) {
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  crawler_arg *arg = (crawler_arg *)raw_arg; // get arguments
  node_t *current_link[0]; // dequeued link
  char *current_content = NULL; // fetched content
  int link_q_size = 0; // size of link_q
  int content_q_size = 0; // size of content q
  int c_parsing = 0;
  int l_fetching = 0;
  int has_err = 0;

  while (
    (!sem_getvalue(&arg->link_q->full, &link_q_size) && link_q_size)
    ||
    (!sem_getvalue(&arg->content_q->full, &content_q_size) && content_q_size)
    ||
    (!sem_getvalue(&contents_parsing, &c_parsing) && c_parsing)
    ||
    (!sem_getvalue(&links_fetching, &l_fetching) && l_fetching)
    ) {
      if ((!content_q_size && !link_q_size) && (c_parsing || l_fetching)) {
        printf("downloader wating, content q size:%i, link q size: %i, c_parsing: %i, l_fetching: %i\n", content_q_size, link_q_size, c_parsing, l_fetching);
        continue;
      }
    printf("In Downloader, content q size:%i, link q size: %i\n", content_q_size, link_q_size);
		has_err = Queue_Dequeue(arg->link_q, current_link);
    sem_post(&links_fetching);
    if (has_err) {
      // Indicate that thread is finished
      Queue_Enqueue(arg->finished_q, arg->retv);
      pthread_exit((void*)(long)has_err);
    }

    // Fetching content
    current_content = arg->fetch_fn((current_link[0])->content);
    if (current_content != NULL) {
      // Must make a copy of the link as it could be freed at any time.
      node_t *tmp = (node_t*)malloc(sizeof(node_t));
      printf("TMP: %p\n", tmp);
      if (tmp == NULL) {
        Queue_Enqueue(arg->finished_q, arg->retv);
        has_err = MALLOC_ERR;
        pthread_exit((void*)(long)has_err);
      }
      // Set the src to be the link to the page
      tmp->src = strcpy_alloc(current_link[0]->content);
      printf("tmp src: %p, link src: %p\n", tmp->src, current_link[0]->content);
      if (tmp->src == NULL) {
        Node_Free(tmp);
        Queue_Enqueue(arg->finished_q, arg->retv);
        has_err = MALLOC_ERR;
        pthread_exit((void*)(long)has_err);
      }
      tmp->content = strcpy_alloc(current_content);
      printf("tmp content: %p, link content: %p\n", tmp->content, current_content);
      if (tmp->content == NULL) {
        Node_Free(tmp);
        Queue_Enqueue(arg->finished_q, arg->retv);
        has_err = MALLOC_ERR;
        pthread_exit((void*)(long)has_err);
      }
      tmp->next = NULL;
      has_err = Queue_Enqueue(arg->content_q, tmp);
      if (has_err) {
        Node_Free(tmp);
        Queue_Enqueue(arg->finished_q, arg->retv);
        pthread_exit((void*)(long)has_err);
      }
    }
    Node_Free(current_link[0]);
    sem_wait(&links_fetching);
  }
  printf("Downloading Exiting %lu\n", (long)arg->retv->src);
  Queue_Enqueue(arg->finished_q, arg->retv);
  printf("here");
  pthread_exit((void*)(long)has_err);
}

void *parser(void *raw_arg) {
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  crawler_arg *arg = (crawler_arg *)raw_arg;
  node_t *first_link = NULL;
  node_t *current_content[0];
  int link_q_size = 0;
  int content_q_size = 0;
  int c_parsing = 0;
  int l_fetching = 0;

  int has_err = 0;

	while (
    (!sem_getvalue(&arg->link_q->full, &link_q_size) && link_q_size)
    ||
    (!sem_getvalue(&arg->content_q->full, &content_q_size) && content_q_size)
    ||
    (!sem_getvalue(&contents_parsing, &c_parsing) && c_parsing)
    ||
    (!sem_getvalue(&links_fetching, &l_fetching) && l_fetching)
  ) {
    if ((!content_q_size && !link_q_size) && (c_parsing || l_fetching)) {
      printf("parser wating, content q size:%i, link q size: %i, c_parsing: %i, l_fetching: %i\n", content_q_size, link_q_size, c_parsing, l_fetching);
      continue;
    }
    printf("In Parser, content q size:%i, link q size: %i\n", content_q_size, link_q_size);
    has_err = Queue_Dequeue(arg->content_q, current_content);
    sem_post(&contents_parsing);
    if (has_err) {
      // Indicate that thread is finished
      Queue_Enqueue(arg->finished_q, arg->retv);
      pthread_exit((void*)(long)has_err);
    }
    // Parse Content
    first_link = ParseLinks(current_content[0]);
    // Store Link
    printf("freeing content\n");
    Node_Free(current_content[0]);
    printf("freed content\n");
    while(first_link != NULL) {
      // Send to edge function
      char *from = strcpy_alloc(first_link->src);
      if (from == NULL) {
        // Indicate that thread is finished
        Queue_Enqueue(arg->finished_q, arg->retv);
        has_err = MALLOC_ERR;
        pthread_exit((void*)(long)has_err);

      }
      char *to = strcpy_alloc(first_link->content);
      if (to == NULL) {
        Queue_Enqueue(arg->finished_q, arg->retv);
        has_err = MALLOC_ERR;
        pthread_exit((void*)(long)has_err);
      }
      arg->edge_fn(from, to);
      node_t *tmp = first_link;
      first_link = first_link->next;
      printf("adding link %p\n", tmp);
      has_err = Queue_Enqueue(arg->link_q,tmp);
      printf("added link\n");
      if (has_err) {
        Node_Free(tmp);
        Queue_Enqueue(arg->finished_q, arg->retv);
        pthread_exit((void*)(long)has_err);
      }
    }
    sem_wait(&contents_parsing);
    printf("here");
  }
  printf("Parser Exiting %lu, %i\n", (long)arg->retv->src, has_err);
  Queue_Enqueue(arg->finished_q, arg->retv);
  pthread_exit((void*)(long)has_err);
}

regex_t regex;

int crawl(
  char *start_url,
  int download_workers,
  int parse_workers,
  int queue_size,
  char * (*fetch_fn)(char *link),
  void (*edge_fn)(char *from, char *to)
){
  if(regcomp(&regex, "link:([a-z|0-9|\\/|:|\\.]+)", REG_EXTENDED | REG_ICASE)){
    perror("Could not complie regex\n");
    return(1);
  }
  int has_err = 0;
  // Node's free the link, copy original to make sure library does not free
  // arguments.
  char *s_url = (char *)malloc(strlen(start_url));
  strcpy(s_url, start_url);

  queue_t link_q;
  queue_t content_q;
  queue_t finished_q;
  sem_init(&contents_parsing, 0, 0);
  sem_init(&links_fetching, 0, 0);
  has_err = Queue_Init(&link_q, queue_size, 1);
  if (has_err) {
    perror("Couldn't make link queue");
    return has_err;
  }

  has_err = Queue_Init(&content_q, queue_size, 1);
  if (has_err){
    perror("Couldn't make content queue");
    return has_err;
  }

  has_err = Queue_Init(&finished_q, download_workers + parse_workers, 1);
  if (has_err) {
    perror("Couldn't make finished_q");
    return has_err;
  }
	printf("linkq %p\n", &link_q);
	printf("contentq %p\n", &content_q);
	printf("finishedq %p\n", &finished_q);

  // Add the first link to the link_q
  node_t *first_node = (node_t*)malloc(sizeof(node_t));
  if (first_node == NULL) {
    perror("Couldn't make first link");
    return -1;
  }
  first_node->src = strcpy_alloc(s_url);
  if (first_node->src == NULL) {
    perror("Couldn't set src of first link");
    return -1;
  }
  first_node->content = strcpy_alloc(s_url);
  if (first_node->content == NULL) {
    perror("Couldn't set content of first link");
    return -1;
  }
  has_err = Queue_Enqueue(&link_q, first_node);
  if (has_err) {
    perror("Couldn't enqueue first link");
    return -1;
  }

  // Make Threads, parsers first.
	size_t should_cancel = 0;
  if (parse_workers <= 0) {
    parse_workers = 1;
  }
  if (download_workers <= 0) {
    download_workers = 1;
  }
	// 0 not started. 1 indicates not finished. 2 indicates finished;
  int *finished = (int*)malloc((parse_workers+download_workers)*sizeof(int));
	if (finished == NULL) {
		perror("Couldn't allocate finished array");
		return -1;
	}
	crawler_arg* crawler_args = (crawler_arg*)malloc((parse_workers+download_workers)*sizeof(crawler_arg*));
	if (crawler_args == NULL) {
		perror("Couldn't allocate crawler argument array");
		return -1;
	}
	pthread_t* parsers = (pthread_t *)malloc(parse_workers * sizeof(pthread_t*));
	if (parsers == NULL) {
		perror("Couldn't allocate parsers array");
		return -1;
	}
	pthread_t* downloaders = (pthread_t *)malloc(download_workers * sizeof(pthread_t*));
	if (downloaders == NULL) {
		perror("Couldn't allocate downloaders array");
		return -1;
	}

	// spawn threads
  // i < parse_workers indicates parse worker i >= parse_workers indicates download_worker
  int i = 0;
  for (; i < parse_workers; i++ ) {
    crawler_arg* args = &crawler_args[i];
    node_t* retv = (node_t*)malloc(sizeof(node_t));
    if (retv == NULL) {
      perror("failed to create parser thread arg->retv");
      should_cancel = 1;
      break;
    }
    // use the source of retv to store the index of the thread.
    retv->src = (char *)(long)i;
    args->link_q = &link_q;
    args->content_q = &content_q;
    args->finished_q = &finished_q;
    args->retv = retv;
    args->fetch_fn = fetch_fn;
    args->edge_fn = edge_fn;
    has_err = pthread_create(&parsers[i], NULL, parser, args);
    if (has_err) {
      perror("failed to create parser thread");
      should_cancel = 1;
      break;
    }
    finished[i] = 1;
  }
  if (!should_cancel) {
    for (i = parse_workers; i < (parse_workers + download_workers); i++ ) {
      crawler_arg* args = &crawler_args[i];
      node_t* retv = (node_t*)malloc(sizeof(node_t));
      if (retv == NULL) {
        perror("failed to create parser thread arg->retv");
        should_cancel = 1;
        break;
      }
      // use the source of retv to store the index of the thread.
      retv->src = (char *)(long)i;
      args->link_q = &link_q;
      args->content_q = &content_q;
      args->finished_q = &finished_q;
      args->retv = retv;
      args->fetch_fn = fetch_fn;
      args->edge_fn = edge_fn;

      has_err = pthread_create(&downloaders[i-parse_workers], NULL, downloader, args);
      if (has_err) {
        perror("failed to create downloader thread");
        should_cancel = 1;
        break;
      }
      finished[i] = 1;
    }
  }
  // clean up
  if (!should_cancel) {
    for (i = 0; i < (parse_workers + download_workers); i++) {
      node_t* reth[1];
      has_err = Queue_Dequeue(&finished_q, reth);
      printf("finished %lu\n", (long)reth[0]->src);
      if (has_err) {
        perror("failed to create downloader thread");
        should_cancel = 1;
        break;
      }
			int thr_id = (int)(long)reth[0]->src;
      void *status;
      if (i < parse_workers) {
        has_err = pthread_join(parsers[thr_id], &status);
        finished[thr_id] = 2;
        if (has_err) {
          perror("parser thread joined improperly");
          should_cancel = 1;
          break;
        }
        has_err = (int)(long)status;
        if (has_err) {
          perror("parser thread exited with error");
          should_cancel = 1;
          break;
        }
      } else {
        has_err = pthread_join(downloaders[thr_id-parse_workers], &status);
        finished[thr_id] = 2;
        if (has_err) {
          perror("downloader thread joined improperly");
          should_cancel = 1;
          break;
        }
        has_err = *(int*)status;
        if (has_err) {
          perror("downloader thread exited with error");
          should_cancel = 1;
          break;
        }
      }


    }
  }

  if (should_cancel) {
    for (i = 0; i < (parse_workers + download_workers); i++) {
      // cancel all started threads
      if (finished[i] == 1) {
        if (i < parse_workers) {
          pthread_cancel(parsers[i]);
        } else {
          pthread_cancel(parsers[i-parse_workers]);
        }
      }
    }
  }
  if (has_err) {
    return -1;
  } else {
    return 0;
  }
}

node_t* ParseLinks(node_t* page) {
  printf("Parse link, src: %s, content: %s", page->src, page->content);
  char* text = page->content;
  if (text == NULL) {
    return NULL;
  }
  const char * p = text; // Pointer to current start of string
  const int groups = 2;
  regmatch_t m[groups];
  node_t* start_node = NULL;
  node_t* next_node = NULL;
  while (1) {
    int nomatch = regexec(&regex, p, groups, m, 0);
    if (nomatch) {
      return start_node;
    }
    if (start_node == NULL) {
      start_node = next_node = (node_t *)malloc(sizeof(node_t));
    } else {
      node_t* prev = next_node;
      next_node = (node_t *)malloc(sizeof(node_t));
      prev-> next = next_node;
    }
    int start = m[1].rm_so + (p - text);
    int finish = m[1].rm_eo + (p-text);

    next_node->src = strcpy_alloc(page->src);
    next_node->content = (char *)malloc((finish-start)*sizeof(char) +1);
    memcpy(next_node->content, text+start, finish-start);
    next_node->content[finish-start] = '\0';
    next_node->next = NULL;
    p += m[0].rm_eo;
   }
  return start_node;
}

void Node_Free(node_t *node){
  printf("FREEING: %p\n", node);
  if(node != NULL) {
    if (node->src != NULL) {
      free(node->src);
    }
    if (node->content != NULL) {
      free(node->content);
    }
    free(node);
  }
}
const int QUEUE_DEFAULT_MAX_SIZE = 1000;
// Queue_Init initializes a queue.
int Queue_Init(queue_t *q, size_t size, size_t panic_on_error) {
  int has_err = 0;
  q->bounded = size;
  if (!q->bounded) {
    q->bounded = QUEUE_DEFAULT_MAX_SIZE;
  }
  q->panic_on_error = panic_on_error;
  q->head = q->tail = NULL;
  has_err = pthread_mutex_init(&q->lock, NULL);
  if (has_err) {
    q->err_msg = "queue initialization err: failed to initialize head mutex";
    if (q->panic_on_error) {
      perror(q->err_msg);
      exit(q->panic_on_error);
    }
    return has_err;
  }

  has_err = sem_init(&q->empty, 0, q->bounded);
  if (has_err) {
    q->err_msg = "queue initialization err: failed to initialize empty semaphore";
    if (q->panic_on_error) {
      perror(q->err_msg);
      exit(q->panic_on_error);
    }
    return has_err;
  }
  has_err = sem_init(&q->full, 0, 0);
  if (has_err) {
    q->err_msg = "queue initialization err: failed to initialize full semaphore";
    if (q->panic_on_error) {
      perror(q->err_msg);
      exit(q->panic_on_error);
    }
    return has_err;
  }
  return 0;
}

// Queue_Enqueue adds a node to the queue. You are responsible for freeing
// the node when dequeued.
int Queue_Enqueue(queue_t *q, node_t* node) {
  int has_err = 0;
  node->next = NULL;
  has_err = sem_wait(&q->empty);
  printf("Start Enqueueing %p, %p\n", q, node);
  Queue_Print(q);
  if (has_err) {
    q->err_msg = "queue enqueue err: failed to wait on empty semaphore";
    if (q->panic_on_error) {
      perror(q->err_msg);
      exit(q->panic_on_error);
    }
    return has_err;
  }
  has_err = pthread_mutex_lock(&q->lock);
  if (has_err) {
    q->err_msg = "queue enqueue err: failed to lock tail mutex";
    if (q->panic_on_error) {
      perror(q->err_msg);
      exit(q->panic_on_error);
    }
    return has_err;
  }
  // handle no elements
  if (q->tail != NULL) {
    q->tail->next = node;
  } else {
    q->head = q->tail = node;
  }
  has_err = pthread_mutex_unlock(&q->lock);
  if (has_err) {
    q->err_msg = "queue enqueue err: failed to unlock tail mutex";
    if (q->panic_on_error) {
      perror(q->err_msg);
      exit(q->panic_on_error);
    }
    return has_err;
  }
  // remeber, empty is max size of queue - number of elements in the queue
  // full is just the number of elements
  // so we post here when ever an element is enqueued

	has_err = sem_post(&q->full);
  if (has_err) {
    q->err_msg = "queue enqueue err: failed to post full semaphore";
    if (q->panic_on_error) {
      perror(q->err_msg);
      exit(q->panic_on_error);
    }
  }
  printf("End Enqueueing %p\n", q);

  return has_err;
}

// Queue_Dequeeu remeoves a node from the queue.
int Queue_Dequeue(queue_t *q, node_t **node) {
  int has_err = 0;
  // Wait for there to be an element.
  has_err = sem_wait(&q->full);
	printf("Start Dequeueing %p\n", q);
  Queue_Print(q);

  if (has_err) {
    q->err_msg = "queue dequeue err: failed to wait on full semaphore";
    if (q->panic_on_error) {
      perror(q->err_msg);
      exit(q->panic_on_error);
    }
    return has_err;
  }
  has_err = pthread_mutex_lock(&q->lock);
  if (has_err) {
    q->err_msg = "queue dequeue err: failed to unlock head mutex";
    if (q->panic_on_error) {
      perror(q->err_msg);
      exit(q->panic_on_error);
    }
    return has_err;
  }
	// Handle first element
	if (q->head == NULL) {
		q->head = q->tail;
	}
  if (q->head == NULL) {
    q->err_msg = "queue dequeeu err: dequeued when empty";
    if (q->panic_on_error) {
      perror(q->err_msg);
      exit(q->panic_on_error);
    }
    return has_err;
  }
  node_t *tmp = q->head;
  q->head = tmp->next;
  if (q->head == NULL) {
    q->head = q->tail = NULL;
  }
  if (tmp == NULL) {
    has_err = pthread_mutex_unlock(&q->lock);
    if (has_err) {
      q->err_msg = "queue dequeue err: failed to unlock tail mutex";
      if (q->panic_on_error) {
        perror(q->err_msg);
        exit(q->panic_on_error);
      }
      return has_err;
    }
    else {
      q->err_msg = "queue dequeue err: Queue dequed when empty!";
      if (q->panic_on_error) {
        perror(q->err_msg);
        exit(q->panic_on_error);
      }
      return -1;
    }
  }
  node[0] = tmp;
  has_err = pthread_mutex_unlock(&q->lock);
  if (has_err) {
    q->err_msg = "queue dequeue err: failed to unlock head mutex";
    if (q->panic_on_error) {
      perror(q->err_msg);
      exit(q->panic_on_error);
    }
    return has_err;
  }
  has_err = sem_post(&q->empty);
  if (has_err) {
    q->err_msg = "queue dequeue err: failed to post empty semaphore";
    if (q->panic_on_error) {
      perror(q->err_msg);
      exit(q->panic_on_error);
    }
  }
  printf("End Dequeueing %p %p\n", q, tmp);

  return has_err;
}

void Queue_Print(queue_t *q) {
  node_t* tmp = q->head;
  int q_empty = 0;
  sem_getvalue(&q->empty, &q_empty);
  printf("Printing queue %p, empty: %i\n", q, q_empty);
  while (tmp != NULL) {
    printf ("NODE: %p\n", tmp);
    tmp = tmp->next;
  }
  return;
}
