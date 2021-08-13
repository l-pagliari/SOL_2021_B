#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <util.h>
#include <mydata.h>


struct node *head = NULL;
struct node *current = NULL;

void list_insert(int id, char * data) {
   struct node *link = (struct node*) malloc(sizeof(struct node));
   if(link == NULL) {
      perror("malloc");
      exit(EXIT_FAILURE);
   }
	link->id = id;
   strncpy(link->ht_key, data, strlen(data));

   link->next = head;
   head = link;
}

int list_isEmpty() {
   return head == NULL;
}

char* list_find_key(int id) {
   struct node* current = head;

   if(head == NULL) return NULL;
   while(current->id != id) {
      if(current->next == NULL)  return NULL;
      else current = current->next;
   }
   if(current == NULL) return NULL;      
   return current->ht_key;
}

int list_delete(int id) {
   struct node* current = head;
   struct node* previous = NULL;
   
   if(head == NULL) {
      return -1;
   }
   while(current->id != id) {
      if(current->next == NULL) {
         return -1;
      } else {
         previous = current;
         current = current->next;
      }
   }
   if(current == head) {
      head = head->next;
   } else {
      previous->next = current->next;
   }    
   free(current);
   return 1;
}