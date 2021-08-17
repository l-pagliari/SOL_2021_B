#if !defined QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <linux/limits.h>

typedef struct queue_node {
	char tkey[PATH_MAX];
	struct queue_node *next;
} node_t;

typedef struct queue {
	node_t *head;
	node_t *tail;
	pthread_mutex_t qlock;
} queue_t;

queue_t * init_queue();

char* q_put(queue_t *q, char *key);

void q_pull(queue_t *q, char *key);

void q_remove(queue_t *q, char *key);

int isEmpty(queue_t *q);

#endif /* QUEUE_H */