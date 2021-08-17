#define _POSIX_C_SOURCE 2001112L 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include <queue.h>
#include <util.h>

queue_t* init_queue(){

	queue_t * q;
	q = malloc(sizeof(queue_t));
	if(q == NULL) {
		perror("malloc");
		return NULL;
	}
	q->head = NULL;
	q->tail = NULL;
	pthread_mutex_init(&q->qlock, NULL);
	return q;
}

char* q_put(queue_t *q, char *key) {

	node_t * new;
	new = malloc(sizeof(node_t));
	if(new == NULL) {
		perror("malloc");
		return NULL;
	}
	strncpy(new->tkey, key, strlen(key));
	new->next = NULL;

	LOCK(&q->qlock);
	if(isEmpty(q)) 
		q->head = new;
	else
		q->tail->next = new;
	q->tail = new;
	UNLOCK(&q->qlock);

	//printf("TEST: ho inserito nella coda: %s\n", new->tkey);

	return new->tkey;
}

void q_pull(queue_t *q, char *key) {

	node_t *tmp;
	tmp = q->head;

	/*str = malloc(strlen(tmp->tkey));
	if(str == NULL) {
		perror("malloc");
		return NULL;
	}*/
	strncpy(key, tmp->tkey, PATH_MAX);

	LOCK(&q->qlock);
	q->head = q->head->next;
	if(q->head == NULL)
		q->tail = NULL;
	UNLOCK(&q->qlock);

	free(tmp);
	
}

//scorre la coda ed elimina il nodo richiesto se lo trova
void q_remove(queue_t *q, char *key) {
	//caso limite
	if(isEmpty(q)) return;

	node_t *curr;
	node_t *prev;
	node_t *tmp;
	//caso il nodo da eliminare e' il primo
	if(strcmp(q->head->tkey, key) == 0) {
		tmp = q->head;
		LOCK(&q->qlock);
		q->head = q->head->next;
			if(q->head == NULL)
		q->tail = NULL;
		UNLOCK(&q->qlock);
		free(tmp);
		return;
	}
	prev = q->head;
	curr = q->head->next;
	//scorro la coda per trovare il valore richiesto
	while(curr != NULL && strcmp(curr->tkey, key) != 0){
		prev = curr;
		curr = curr->next;
	}
	//se l'ho trovato, lo elimino mantenendo l'ordine
	if(curr != NULL) {
		tmp = curr;
		prev->next = curr->next;
		if(curr->next == NULL){
			LOCK(&q->qlock);
			q->tail = prev;
			UNLOCK(&q->qlock);
		}
		free(tmp);
	}
}

int isEmpty(queue_t *q){
	return q->head == NULL;
}