#if !defined QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <linux/limits.h>

/* 	semplice coda usata per tenere traccia dell'ordine dei file salvati sul server
	necessario per l'algoritmo di rimpiazzamento a seguito di capacity miss */

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

void freeQueue(queue_t *q);

int isEmpty(queue_t *q);

/*  funzione atipica per una coda, elimina un elemento interno preservando
**	l'ordine; e' necessaria nel caso si elimini un file nel server per altre cause 
*/
void q_remove(queue_t *q, char *key);

/*	funzione che mette in fondo alla coda un elemento; viene usata per trasformare la
**	politica FIFO intrinseca della struttura queue in una politica LRU; ogni volta che
**	un file viene "usato" riceve un bump nella coda risultando come se fosse stato appena inserito
*/
void q_bump(queue_t *q, char *key);

#endif /* QUEUE_H */