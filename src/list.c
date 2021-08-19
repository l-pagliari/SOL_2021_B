#define _POSIX_C_SOURCE 2001112L 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <util.h>
#include <mydata.h>


struct node *head = NULL;
//forse serve una mutex per quando modifico la head

void cleanuplist_ins(int id, char * data) {
   listnode_t * new = malloc(sizeof(listnode_t));
   if(new == NULL) {
      perror("malloc");
      exit(EXIT_FAILURE);
   }
	new->id = id;
   strncpy(new->ht_key, data, strlen(data));

   new->next = head;
   head = new;
}

//puo' esistere una sola entry per key
int cleanuplist_del(char *data) {
   
   if(head == NULL) return -1;
   listnode_t * prev;
   listnode_t * curr;
   listnode_t * temp;
   //caso in cui il nodo da eliminare e' il primo
   if(strcmp(data, head->ht_key) == 0) {
      temp = head;
      head = head->next;
      free(temp);
      return 0;
   }
   else {
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
         return 0;
      }
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
   while(head != NULL) {
      struct node *tmp = head;
      tmp = head;
      head = head->next;
      free(tmp);
   }
}

int cleanuplist_isEmpty() {
   return head == NULL;
}
