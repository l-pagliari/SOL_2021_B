#define _POSIX_C_SOURCE 2001112L 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include <util.h>
#include <mydata.h>


struct node *head = NULL;
//forse serve una mutex per quando modifico la head
pthread_mutex_t clist_mtx = PTHREAD_MUTEX_INITIALIZER;

void cleanuplist_ins(int id, char * data) {
   listnode_t * new = malloc(sizeof(listnode_t));
   if(new == NULL) {
      perror("malloc");
      exit(EXIT_FAILURE);
   }
	new->id = id;
   strncpy(new->ht_key, data, PATH_MAX);
   
   LOCK(&clist_mtx);
   new->next = head;
   head = new;
   UNLOCK(&clist_mtx);
}

//puo' esistere una sola entry per key
int cleanuplist_del(char *data) {
   
   if(head == NULL) return -1;
   listnode_t * prev;
   listnode_t * curr;
   listnode_t * temp;
   //caso in cui il nodo da eliminare e' il primo
   if(strcmp(data, head->ht_key) == 0) {
      LOCK(&clist_mtx);
      temp = head;
      head = head->next;
      UNLOCK(&clist_mtx);
      free(temp);
      return 0;
   }
   else {
      LOCK(&clist_mtx);
      prev = head;
      curr = head->next;
      //scorro la lista 
      while(curr != NULL && (strcmp(data, curr->ht_key) != 0)) {
         prev = curr;
         curr = curr->next;
      }
      //se mi sono fermato prima della fine lo elimino
      if(curr != NULL) {
         temp = curr;
         prev->next = curr->next;
         free(temp);
         UNLOCK(&clist_mtx);
         return 0;
      }
      UNLOCK(&clist_mtx);
   }
   return -1;
}

char * cleanuplist_getakey(int id) {

   listnode_t * curr;
   curr = head;
   while(curr != NULL && curr->id != id) 
         curr = curr->next;
      
   if(curr != NULL) 
         return curr->ht_key;
  
   return NULL;
}

void cleanuplist_free(void) {
   LOCK(&clist_mtx);
   while(head != NULL) {
      struct node *tmp = head;
      tmp = head;
      head = head->next;
      free(tmp);
   }
   UNLOCK(&clist_mtx);
   pthread_mutex_destroy(&clist_mtx);
}

int cleanuplist_isEmpty() {
   return head == NULL;
}
