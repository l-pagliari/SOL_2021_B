#define _POSIX_C_SOURCE 2001112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>

#include <util.h>
#include <mydata.h>

char timestr[11];

long max_saved_files;
long max_reached_memory;
long num_capacity_miss;

queue_t * replace_queue;

volatile int termina;
volatile int hangup;
volatile int clients;

icl_hash_t * table;
int req_pipe;

int handler_openfile(long fd, request_t * req);

int handler_writefile(long fd, request_t * req); 

void workerF(void *arg) {

	long connfd = *(long*)arg;
	request_t * req;
	int r;
	//char * keyptr;
 
		req = malloc(sizeof(request_t));
		if(req == NULL) {
			fprintf(stderr, "errore malloc request\n");
			return;
		}
		if(readn(connfd, req, sizeof(request_t)) == -1) {
			fprintf(stderr, "errore lettura richiesta client\n");
			return;
		}

		switch(req->type){
			
			case OPEN_CONNECTION:
				fprintf(stdout, "%s[LOG] Opened connection (client %d)\n", tStamp(timestr), (int)req->cid);		
				if(write(req_pipe, &connfd, sizeof(long)) == -1) 
					fprintf(stderr, "errore scrivendo nella request pipe");
    			break;
    		
			case CLOSE_CONNECTION:
				/*while((keyptr = cleanuplist_getakey(req->cid)) != NULL) {
					unlock_atexit(htab, keyptr);
				}*/
				clients--;
    			close(connfd);
    			fprintf(stdout, "%s[LOG] Closed connection (client %d)\n", tStamp(timestr), (int)req->cid);
    			//se ho ricevuto hangup e questo era l'ultimo client attivo, lo notifico al server
    			if(hangup == 1 && clients == 0) {
    				printf("HANGUP: tutti i task sono terminati, chiudo\n");
    				termina = 1;
    				close(req_pipe);
    			}
				break;

			case OPEN_FILE:
				r = handler_openfile(connfd, req);
				if(write(connfd, &r, sizeof(int)) == -1){
					fprintf(stderr, "errore inviando dati al client\n");
				}
				if(write(req_pipe, &connfd, sizeof(long))==-1)
    				fprintf(stderr, "errore scrivendo nella request pipe\n");
				break;
		
    		case WRITE_FILE:
    			r = handler_writefile(connfd, req);
    			if(write(connfd, &r, sizeof(int)) == -1){
					fprintf(stderr, "errore inviando dati al client\n");
				}
    			if(write(req_pipe, &connfd, sizeof(long))==-1) {
    				fprintf(stderr, "errore scrivendo nella request pipe\n");
    			}
				break;

    	/*
    	case READ_FILE:
    		handler_readfile(connfd, htab, req->filepath);
    		SYSCALL_EXIT("write", notused, write(req_pipe, &connfd, sizeof(long)), "scrivendo nella request pipe", "");
    		break;

    	case READ_N_FILES:
    		handler_read_n_files(connfd, htab, req->arg);
    		SYSCALL_EXIT("write", notused, write(req_pipe, &connfd, sizeof(long)), "scrivendo nella request pipe", "");
    		break;

    	case UNLOCK_FILE:
    		handler_unlockfile(connfd, htab, req->filepath, req->cid);
    		SYSCALL_EXIT("write", notused, write(req_pipe, &connfd, sizeof(long)), "scrivendo nella request pipe", "");
    		break;

    	case LOCK_FILE:
    		handler_lockfile(connfd, htab, req->filepath, req->cid);
    		SYSCALL_EXIT("write", notused, write(req_pipe, &connfd, sizeof(long)), "scrivendo nella request pipe", "");
    		break;

    	case REMOVE_FILE:
    		handler_removefile(connfd, htab, req->filepath, req->cid);
    		SYSCALL_EXIT("write", notused, write(req_pipe, &connfd, sizeof(long)), "scrivendo nella request pipe", "");
    		break;

    	case APPEND_FILE:
    		handler_append(connfd, htab, req->filepath, req->cid);
    		SYSCALL_EXIT("write", notused, write(req_pipe, &connfd, sizeof(long)), "scrivendo nella request pipe", "");
    		break; */
    		default: ;
    	}
    	free(req);
}

/*ho deciso che se faccio open su un file gia' aperto va bene lo stesso,
  mentre se ho il flag O_LOCK ed il file e' gia' lockato ritorno errore */
int handler_openfile(long fd, request_t * req) {

	int id = req->cid;
	int flag = req->arg;
	//char *key = req->filepath;
	file_t *data;

	char *key = (char*)malloc(PATH_MAX*sizeof(char));
	if(key == NULL) {
		fprintf(stderr, "malloc key\n");
		return -1;
	}
	strncpy(key, req->filepath, PATH_MAX);

	if(flag == O_CREATE || flag == (O_CREATE|O_LOCK)) {	
		//controllo che il file non esista gia'
		if(icl_hash_find(table, key) != NULL) {
			fprintf(stderr, "[SERVER] Impossibile creare il file, esiste gia' in memoria\n");
			return -1;
		}
		//inizializzo il nuovo file privo di contenuto
		data = malloc(sizeof(file_t));
		if(data == NULL) {
			perror("malloc file_t");
			return -1;
		}
		data->contenuto = NULL;
		data->file_size = 0;
		data->open_flag = 1;
		//salvo il nome base del file 
		char *tmp_str = strdup(key);
		char *bname = basename(tmp_str);
		size_t len = strlen(bname);
		data->file_name = (char*)malloc(BUFSIZE*sizeof(char));
		strncpy(data->file_name, bname, BUFSIZE);
		data->file_name[len] = '\0';
		free(tmp_str);
		//cond usata per notificare che il file e' tornato disponibile
		pthread_cond_init(&(data->cond), NULL);
		//se occorre setto true il flag di lock del file
		if(flag == (O_CREATE|O_LOCK)) {
			data->lock_flag = 1;
			data->locked_by = id;
		}
		else {
			data->lock_flag = 0;
			data->locked_by = -1;
		}
		//inserisco nella tabella il nuovo file
		LOCK_RETURN(&(table->lock), -1);
		if(icl_hash_insert(table, key, data) == NULL) {
			fprintf(stderr, "errore inserimento in memoria di %s\n", key);
			UNLOCK_RETURN(&(table->lock), -1);
			return -1;
		}
		UNLOCK_RETURN(&(table->lock), -1);
		//scrivo l'esito delle operazioni sul log
		if(data->lock_flag == 1) {
			fprintf(stdout, "%s[LOG] Open-Locked file %s\n", tStamp(timestr), key);
			//voglio ricordarmi che il file identificato da key e' stato lockato da id
			cleanuplist_ins(id, key);
		}
		else {
			fprintf(stdout, "%s[LOG] Opened file %s\n", tStamp(timestr), key);
		}

	}
	else if(flag == O_LOCK) {
		//controllo che il file esista e sia sbloccato
		data = icl_hash_find(table, key);
		if(data == NULL) {
			fprintf(stderr, "[SERVER] Errore: file non presente in memoria\n");
			return -1;
		}
		if(data->lock_flag == 1) {
			fprintf(stderr, "[SERVER] Errore: file gia' lockato\n");
			return -1;
		}
		//modifico il flag del file
		int limitcase = 0;	//caso in cui il file sia stato lockato nel frattempo
		LOCK_RETURN(&(table->lock), -1);
		if(data->lock_flag != 1) {
			data->open_flag = 1;	
			data->lock_flag = 1;	
			data->locked_by = id;
		} else limitcase = 1;
		UNLOCK_RETURN(&(table->lock), -1);
		if(limitcase == 1) {
			fprintf(stderr, "[SERVER] Errore: file gia' lockato\n");
			return -1;
		}
		fprintf(stdout, "%s[LOG] Open-Locked file %s\n", tStamp(timestr), key);
		//devo ricordarmi da qualche parte che il file identificato da key e' stato lockato da id
		cleanuplist_ins(id, key);
	}
	/* nella mia implementazione non ho trovato un uso particolare di open file senza flag, ho
	   comunque implementato l'opzione per completezza */
	else { 
		data = icl_hash_find(table, key);
		if(data == NULL) {
			fprintf(stderr, "[SERVER] Errore: file non presente in memoria\n");
			return -1;
		}
		LOCK_RETURN(&(table->lock), -1);
		data->open_flag = 1;
		UNLOCK_RETURN(&(table->lock), -1);
		fprintf(stdout, "%s[LOG] Opened file %s\n", tStamp(timestr), key);
	}
	//TEMP: cosa succede alla key della hash se faccio una free?
	//free(req);
	return 0;			
}

int handler_writefile(long fd, request_t * req) {

	file_t *data;
	int ret, retval = 0;
	size_t fsize;
	void *buf;
	char *key = req->filepath;


	//controllo che il file sia presente in memoria, aperto e locked
	data = icl_hash_find(table, key);
	if(data == NULL){
		fprintf(stderr, "[SERVER] Errore: file perso\n");
		return -1;
	}
	if(data->open_flag != 1) {
		fprintf(stderr, "[SERVER] Errore: file non aperto\n");
		return -1;
	}
	if(data->lock_flag != 1 || data->locked_by != req->cid ){
		fprintf(stderr, "[SERVER] Errore: file non lockato\n");
		return -1;
	}
	//comunico al client che puo' inviare il file
	SYSCALL_RETURN("write", ret, writen(fd, &retval, sizeof(int)), "inviando dati al client", "");

	//leggo prima la dimensione del file e poi il file
	SYSCALL_RETURN("read", ret, readn(fd, &fsize, sizeof(size_t)), "leggendo dati dal client", "");
	buf = malloc(fsize);
	if(buf == NULL) {
		perror("malloc buffer");
		return -1;
	}
	SYSCALL_RETURN("read", ret, readn(fd, buf, fsize), "leggendo dati dal client", "");

	//CONTROLLO MEMORIA
	if(fsize > MAX_CAP) {
		fprintf(stderr, "[SERVER] Errore: file troppo grande\n");
		/* Non e' specificata la gestione di questo specifico caso: il client tenta di scrivere un file
		   piu' grande della capacita' massima del server. Siccome la specifica chiede che le scritture avvengano
		   solo in append, in seguito di questo errore rimane nel server un file "guscio" privo di contenuto; 
		   posso eliminarlo subito o lasciarlo nel server da cui verra' tolto dall'algoritmo di rimpiazzamento in seguito
		   oppure manualmente dall'utente con un remove. 
		   Non creando problemi di nessun tipo ho deciso di lasciarlo nel server e dare la scelta all'utente
		
		icl_hash_delete(table, key, NULL, free); */
		return -1;
	}
	//aggiorno i valori globali di storage
	LOCK_RETURN(&storemtx, -1);
	CUR_CAP = CUR_CAP + fsize;
	CUR_FIL++;
	if(CUR_FIL > max_saved_files) max_saved_files = CUR_FIL;
	if(CUR_CAP > max_reached_memory) max_reached_memory = CUR_CAP;
	UNLOCK_RETURN(&storemtx, -1);
	while(CUR_CAP > MAX_CAP || CUR_FIL > MAX_FIL) {

		//TEMP: DA RIGUARDARE
		printf("TEMP: CAPACITY MISS! Algoritmo TBI\n");
		//rimpiazzamento_fifo(fd, table);  
		num_capacity_miss++;
	}
	//comunico al client che c'e' abbastanza spazio per scrivere il file
	SYSCALL_RETURN("write", ret, writen(fd, &retval, sizeof(int)), "inviando dati al client", "");

	//posso ora scrivere il contenuto del buffer nel file
	data->contenuto = malloc(fsize);
	if(data->contenuto == NULL) {
		perror("malloc contenuto file");
		return -1;
	}
	memcpy(data->contenuto, buf, fsize);
	data->file_size = fsize;
	//metto il nuovo file nella coda di rimpiazzamento
	q_put(replace_queue, key);
		
	fprintf(stdout, "%s[LOG] Writed file %s (%d Bytes)\n", tStamp(timestr), (char*)key, (int)fsize);

	free(buf);
	return 0;
}







/* TO DO:
-cleanup list controllare lock interne all'insert
*/