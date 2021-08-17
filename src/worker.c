#define _POSIX_C_SOURCE 2001112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <libgen.h>

#include <util.h>
#include <mydata.h>

char timestr[11]; //usata per il timestamp nel file di log

queue_t * replace_queue;

void rimpiazzamento_fifo(long fd, icl_hash_t *table);

void handler_openfile(long fd, icl_hash_t * table, void * key, int flag, pid_t id) {

	//TEMP: USIAMO SOLO LA WRITE AL MOMENTO QUINDI VOGLIAMO IL CASO O_CREATE|O_LOCK
	int ret, retval = 0;
	file_t * data;
	if(icl_hash_find(table, key) != NULL){
			fprintf(stderr, "%s esiste gia' nel server\n", (char*)key);
			retval = -1;
	}
	//flag utilizzato per la openFile prima di una write
	else if(flag == (O_CREATE|O_LOCK)) {	
		//inizializzo il nuovo file privo di contenuto
		CHECK_EQ_EXIT("malloc", NULL,(data = malloc(sizeof(file_t))), "allocando un nuovo file", "");
		data->contenuto = NULL;
		data->file_size = 0;
		//utilizzo il nome base del file per maggiore leggibilita', in caso di duplicati posso sempre usare la key(abs path)
		char *tmp_str = strdup(key);
		char *bname = basename(tmp_str);
		size_t len = strlen(bname);
		CHECK_EQ_EXIT("malloc", NULL,(data->file_name = malloc(len+1)), "allocando il nome file", "");
		strncpy(data->file_name, bname, len);
		data->file_name[len] = '\0';
		//parte lock
		data->lock_flag = 1;
		data->locked_by = id;
		//inizializzo le variabili di mutex che serviranno per modificare la lock in seguito
		pthread_mutex_init(&(data->lock), NULL);
		pthread_cond_init(&(data->cond), NULL);
		fprintf(stdout, "%s[LOG] Locked file %s\n", tStamp(timestr), (char*)key);

		
		//devo ricordarmi da qualche parte che il file identificato da key e' stato lockato da id
		//printf("[TEST] inserisco nella lista il file che ho lockato (id=%d)\n", id);
		//list_insert(id, (char*)key);


		/*INSERIRE KEY E ID NELLA CLEANUP LIST */
		cleanuplist_ins(id, key);


		//inserisco nella tabella il nuovo file
		if(icl_hash_insert(table, key, data) == NULL) {
			fprintf(stderr, "errore inserimento in memoria di %s\n", (char*)key);
			retval = -1;
		}
	}
	else printf("flags non ancora supportati\n");

	SYSCALL_EXIT("write", ret, writen(fd, &retval, sizeof(int)), "inviando dati al client", "");
}

void handler_writefile(long fd, icl_hash_t * table, void * key) {

	int ret, retval;
	size_t fsize;
	void * buf;
	file_t *data;
	SYSCALL_EXIT("read", ret, readn(fd, &fsize, sizeof(size_t)), "leggendo dati dal client", "");
	CHECK_EQ_EXIT("malloc", NULL,(buf = malloc(fsize)), "allocando la stringa buf", "");
	SYSCALL_EXIT("read", ret, readn(fd, buf, fsize), "leggendo dati dal client", "");

	//CONTROLLO MEMORIA
	if(fsize > MAX_CAP) {
		fprintf(stderr, "[SERVER] File troppo grande, impossibile salvarlo\n");
		icl_hash_delete(table, key, NULL, free);
		retval = -1;
		SYSCALL_EXIT("write", ret, writen(fd, &retval, sizeof(int)), "inviando dati al client", "");
		return;
	}
	
	CUR_CAP = CUR_CAP + fsize;
	CUR_FIL++;
	while(CUR_CAP > MAX_CAP || CUR_FIL > MAX_FIL) {

		//printf("TESTSERVER: devo fare spazio per il nuovo file, lancio l'algoritmo di rimpiazzamento\n");


		//comunico al client che il server deve espellere uno o piu' file per fare spazio
		//retval = 1;
		//SYSCALL_EXIT("write", ret, writen(fd, &retval, sizeof(int)), "inviando dati al client", "");
		rimpiazzamento_fifo(fd, table);
		//printf("TESTSERVER: sono tornato dal rimpiazzamento\n");

	}
	retval = 0;
	SYSCALL_EXIT("write", ret, writen(fd, &retval, sizeof(int)), "inviando dati al client", "");

	//printf("TESTSERVER: sto per scrivere %s\n", (char*)key);

	//a questo punto ho sicuramente abbastanza spazio, salvo il file in memoria
	data = icl_hash_find(table, key);
	if(data != NULL) {
		CHECK_EQ_EXIT("malloc", NULL,(data->contenuto = malloc(fsize)), "allocando la stringa data", "");	
		memcpy(data->contenuto, buf, fsize);
		data->file_size = fsize;



		q_put(replace_queue, key);

		fprintf(stdout, "%s[LOG] Writed file %s (%d Bytes)\n", tStamp(timestr), (char*)key, (int)fsize);
		
	}
	else fprintf(stderr, "[SERVER] errore nella scrittura del file\n"); //mai successo ma controllo per sicurezza
}

void handler_readfile(long fd, icl_hash_t * table, void * key) {

	int ret, retval = 0;
	file_t *data;
	if((data = icl_hash_find(table, key)) == NULL ) {
		printf("il file non e' presente nel server\n");
		retval = -1;
	}
	SYSCALL_EXIT("write", ret, writen(fd, &retval, sizeof(int)), "inviando dati al client", "");
	if(retval == 0) {
		SYSCALL_EXIT("write", ret, writen(fd, &(data->file_size), sizeof(size_t)), "inviando dati al client", "");
		SYSCALL_EXIT("write", ret, writen(fd, data->contenuto, data->file_size), "inviando dati al client", "");
		fprintf(stdout, "%s[LOG] Read file %s (%d Bytes)\n", tStamp(timestr), (char*)key, (int)data->file_size);
	}
} 

void handler_read_n_files(long fd, icl_hash_t * table, int N) { 
	void **file_array = NULL;
	int unused, num, i;
	int end = 0;

	if(table->nentries != 0) {	//caso limite
		//due casi possibili:
		//1. N==0 o N>=entries --> devo leggere tutti i file contenuti in memoria
		//2. N<entries 		   --> devo leggere N file a caso dalla memoria
		if(N == 0 || (N > table->nentries)) 
			 num = table->nentries;
		else num = N;
		//inizializzo l'array di file vuoto
		if((file_array = malloc(N*sizeof(void*))) == NULL){
			perror("malloc");
			exit(EXIT_FAILURE);
		}
		//inzializzo a null i pointers
		for(i=0; i<num; i++){
			file_array[i] = NULL;
		}
		//riempio l'array di num file
		if(get_n_entries(table, num, file_array) == -1) {
			fprintf(stderr, "errore nella funzione get_n_entries\n");
			exit(EXIT_FAILURE);
		}
		file_t * newfile;
		size_t name_len;
		char synch;
		//invio i file al client
		for(i=0; i<num; i++){
			newfile = file_array[i];
			name_len = strlen(newfile->file_name);
			//invio che c'e' un nuovo file in arrivo
			SYSCALL_EXIT("write", unused, writen(fd, &end, sizeof(int)), "inviando dati al client", "");
			//invio il nome del file
			SYSCALL_EXIT("write", unused, writen(fd, &name_len, sizeof(size_t)), "inviando dati al client", "");
			SYSCALL_EXIT("write", unused, writen(fd, newfile->file_name, name_len), "inviando dati al client", "");
			//invio il contenuto
			SYSCALL_EXIT("write", unused, writen(fd, &newfile->file_size, sizeof(size_t)), "inviando dati al client", "");
			SYSCALL_EXIT("write", unused, writen(fd, newfile->contenuto, newfile->file_size), "inviando dati al client", "");

			fprintf(stdout, "%s[LOG] Read file %s (%d Bytes)\n", tStamp(timestr), newfile->file_name, (int)newfile->file_size);

			//leggo un byte per sapere che il client ha ricevuto il file
			SYSCALL_EXIT("read", unused, read(fd, &synch, 1), "leggendo dati dal client", "");
		}
	}	
	end = 1;
	SYSCALL_EXIT("write", unused, writen(fd, &end, sizeof(int)), "inviando dati al client", "");
	if(file_array) free(file_array);
}

void handler_unlockfile(long fd, icl_hash_t * table, void * key, pid_t id) {

	int unused, retval = 0;
	file_t *data;
	
	data = icl_hash_find(table, key);
	if(data != NULL) {
		//L’operazione ha successo solo se l’owner della lock è il processo che ha richiesto l’operazione
		//altrimenti l’operazione termina con errore.
		if(data->locked_by == id) {
			LOCK(&data->lock);
			data->lock_flag = 0;
			data->locked_by = -1;
			SIGNAL(&data->cond);	//se c'e' un altro worker in attesa del file lo avverto
			UNLOCK(&data->lock);

			fprintf(stdout, "%s[LOG] Unlocked file %s\n", tStamp(timestr), (char*)key);
			//devo togliere il file dalla lista di cleanup alla chiusura
			//printf("[TEST] elimino dalla lista il file appena sbloccato\n");
			//list_delete(id, (char*)key);

			/* TOGLIERE KEY E ID DALLA CLEANUP LIST */
			cleanuplist_del(key);



		}
		else {
			retval = -1;
			fprintf(stderr, "[SERVER] errore unlock: file lockato da un altro client\n");
		}
	} 
	else {
		retval = -1;
		fprintf(stderr, "[SERVER] errore unlock: file non trovato\n");
	}
	SYSCALL_EXIT("write", unused, write(fd, &retval, sizeof(int)), "inviando dati al client", "");
}

void handler_lockfile(long fd, icl_hash_t * table, void * key, pid_t id) {
	int unused, retval = 0;
	int check = 1; //gestione caso limite
	file_t *data;

	data = icl_hash_find(table, key);
	if(data != NULL) {
		//caso 1: il file era stato aperto/creato con il flag O_LOCK e larichiesta proviene dallo stesso processo
		if(data->lock_flag == 1 && data->locked_by == id ) {
			printf("[SERVER] il file e' gia' in stato di lock\n");
			SYSCALL_EXIT("write", unused, write(fd, &retval, sizeof(int)), "inviando dati al client", "");
			return;
		}
		//caso 2: il file non ha flag lock settato
		if(data->lock_flag != 1 ){
			LOCK(&data->lock);
			if(data->lock_flag != 1) {	//caso limite in cui tra l'if e il lock veniamo deschedulati 
				data->lock_flag = 1;	//e il file viene lockato da un altro worker
				data->locked_by = id;
			} else check = 0;
			UNLOCK(&data->lock);
			if(check){
				fprintf(stdout, "%s[LOG] Locked file %s\n", tStamp(timestr), (char*)key);


				//devo ricordarmi da qualche parte che il file identificato da key e' stato lockato da id
				//list_insert(id, (char*)key);

				/* AGGIUNGERE KEY ED ID ALLA CLEANUP LIST */
				cleanuplist_ins(id, key);


				SYSCALL_EXIT("write", unused, write(fd, &retval, sizeof(int)), "inviando dati al client", "");
				return;	
			}			
		}
		//caso 3: il file e' lockato, da specifica l'operazione non termina finche' il flag non viene resettato
		//attendo un segnale che verra' inviato dal processo che effettua l'unlock
		printf("[SERVER] file occupato, attendo la liberazione...\n");
		LOCK(&data->lock);
		while(data->lock_flag == 1)
			WAIT(&data->cond, &data->lock);
		data->lock_flag = 1;		
		data->locked_by = id;
		UNLOCK(&data->lock);
		fprintf(stdout, "%s[LOG] Locked file %s\n", tStamp(timestr), (char*)key);	
		//aggiungo il file alla lista di cleanup all'uscita
		//list_insert(id, (char*)key);	
		cleanuplist_ins(id, key);
	}
	else { 
		retval = -1; 
		fprintf(stderr, "[SERVER] errore lock: file non trovato\n");
	}
	SYSCALL_EXIT("write", unused, write(fd, &retval, sizeof(int)), "inviando dati al client", "");
}

void handler_removefile(long fd, icl_hash_t * table, void * key, pid_t id) {
	//L’operazione fallisce se il file non è in stato locked, o è in
	//stato locked da parte di un processo client diverso da chi effettua la removeFile
	int unused, retval = 0;
	file_t *data;

	data = icl_hash_find(table, key);
	if(data != NULL) {
		if(data->lock_flag == 1 && data->locked_by == id) {
			//prima di eliminare il file devo salvare size e nome
			size_t fsize = data->file_size;
			char * tmp = malloc(strlen((char*)key));
			if(tmp == NULL){
				perror("malloc");
				exit(EXIT_FAILURE);
			}
			strcpy(tmp, (char*)key);

			//lo tolgo dalla lista del cleanup
			//list_delete(id);



			/* TOGLIERE KEY ED ID DALLA CLEANUP LIST */
			cleanuplist_del(key);




			q_remove(replace_queue, key);

			//posso rimuovere il file
			LOCK(&data->lock);
			retval = icl_hash_delete(table, key, NULL, free);
			UNLOCK(&data->lock);
			if(retval==0){

				//aggiorno le informazioni sulla memoria occupata
				CUR_CAP = CUR_CAP - fsize;
				CUR_FIL = CUR_FIL - 1;
				fprintf(stdout, "%s[LOG] Removed file %s (%d Bytes)\n", tStamp(timestr), tmp, (int)fsize);
			} 
			free(tmp);
		}
		else if(data->lock_flag != 1) {
			retval = -1;
			fprintf(stderr, "[SERVER] errore remove: file non lockato\n");
		}
		else {
			retval = -1;
			fprintf(stderr, "[SERVER] errore remove: file lockato da un altro client\n");
		}
	}else {
		retval = -1;
		fprintf(stderr, "[SERVER] errore remove: file non trovato\n");
	}
	SYSCALL_EXIT("write", unused, write(fd, &retval, sizeof(int)), "inviando dati al client", "");
}

void unlock_atexit(icl_hash_t * table, char * key) {

	file_t *data;
	data = icl_hash_find(table, key);
	//dovrebbe essere presente ma controllo per sicurezza
	if(data != NULL) {
		LOCK(&data->lock);
		data->lock_flag = 0;
		data->locked_by = -1;
		SIGNAL(&data->cond);	//se c'e' un altro worker in attesa del file lo avverto
		UNLOCK(&data->lock);
		fprintf(stdout, "%s[LOG] Unlocked file (at exit) %s\n", tStamp(timestr), (char*)key);
	} 
	else fprintf(stderr, "[SERVER] errore unlock_atexit\n");
	cleanuplist_del(key);
}

//libera num bytes dalla tabella politica fifo inviando le entries eliminate al file descriptor
void rimpiazzamento_fifo(long fd, icl_hash_t *table) {

		char  key[PATH_MAX];
		file_t *data;
		size_t name_len, size;
		int unused, r, retval = 1;
		char synch;

		SYSCALL_EXIT("write", unused, writen(fd, &retval, sizeof(int)), "inviando dati al client", "");
	

		//do {
			q_pull(replace_queue, key);
			//printf("2 key da q_pull : %s\n", key);
			data = icl_hash_find(table, key);
			//printf("3\n");

			name_len = strlen(data->file_name);
			size = data->file_size;
			//printf("4\n");
		

			//invio il nome del file
			SYSCALL_EXIT("write", unused, writen(fd, &name_len, sizeof(size_t)), "inviando dati al client", "");
			SYSCALL_EXIT("write", unused, writen(fd, data->file_name, name_len), "inviando dati al client", "");
			//invio il contenuto
			SYSCALL_EXIT("write", unused, writen(fd, &data->file_size, sizeof(size_t)), "inviando dati al client", "");
			SYSCALL_EXIT("write", unused, writen(fd, data->contenuto, data->file_size), "inviando dati al client", "");

			//leggo un byte per sapere che il client ha ricevuto il file
			SYSCALL_EXIT("read", unused, read(fd, &synch, 1), "leggendo dati dal client", "");

			q_remove(replace_queue, key);



			/* TOGLIERE KEY E ID DALLA CLEANUP LIST */
			cleanuplist_del(key);
			//list_delete(id);

			//rimuovo effettivamente il file dalla memoria
			LOCK(&data->lock);
			r = icl_hash_delete(table, key, NULL, free);
			UNLOCK(&data->lock);

			if(r == 0) 
				fprintf(stdout, "%s[LOG] CAPACITY MISS: Removed file %s (%ld Bytes)\n", tStamp(timestr), key, size);
			else
				fprintf(stderr, "[SERVER] Errore nella rimozione file per capacity miss\n");

			//aggiorno i valori e decido se occorre inviarne un altro
			CUR_CAP = CUR_CAP - size;
			CUR_FIL--;
			//if(CUR_CAP <= MAX_CAP && CUR_FIL <= MAX_FIL) retval = 0;

			//lo comunico al client
			//SYSCALL_EXIT("write", unused, writen(fd, &retval, sizeof(int)), "inviando dati al client", "");

			//printf("TESTSERVER: ho scritto al client retval= %d\n", retval);
		//} while(retval);
}

void workerF(void *arg) {
	assert(arg);

    long connfd = ((workerArg_t*)arg)->clientfd;
	int req_pipe = ((workerArg_t*)arg)->pipe; 
	icl_hash_t * htab = ((workerArg_t*)arg)->hash_table; 
	char * keyptr;

	request_t * req;
	int notused;
	CHECK_EQ_EXIT("malloc", NULL,(req = malloc(sizeof(request_t))), "allocando request_t", "");
	SYSCALL_EXIT("read", notused, readn(connfd, req, sizeof(request_t)), "leggendo richiesta dal client", "");

	switch(req->type){

		case OPEN_CONNECTION:
			fprintf(stdout, "%s[LOG] Opened connection (client %d)\n", tStamp(timestr), (int)req->cid);		
			SYSCALL_EXIT("write", notused, write(req_pipe, &connfd, sizeof(long)), "scrivendo nella request pipe", "");
    		break;
    		
		case CLOSE_CONNECTION:
			//se ho file lockati dal client che sto chiudendo, li libero
			//while((keyptr = list_find_key((int)req->cid)) != NULL)
    			//unlock_atexit(htab, keyptr, req->cid);

			/* ELIMINO ENTRY DALLA LISTA FINTANTO CHE L'ID CHE STA CHIUDENDO PRODUCE HIT */
			while((keyptr = cleanuplist_getakey(req->cid)) != NULL) {
				unlock_atexit(htab, keyptr);
			}

    		close(connfd);
    		fprintf(stdout, "%s[LOG] Closed connection (client %d)\n", tStamp(timestr), (int)req->cid);
			break;

		case OPEN_FILE:
    		handler_openfile(connfd, htab, req->filepath, req->arg, req->cid);
    		SYSCALL_EXIT("write", notused, write(req_pipe, &connfd, sizeof(long)), "scrivendo nella request pipe", "");
    		break;
		
    	case WRITE_FILE:
    		handler_writefile(connfd, htab, req->filepath);
    		SYSCALL_EXIT("write", notused, write(req_pipe, &connfd, sizeof(long)), "scrivendo nella request pipe", "");
    		break;
    	//free(req) non fa funzionare la read per qualche ragione
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
    }
}