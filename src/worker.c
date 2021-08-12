#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <libgen.h>
//#include <errno.h>
//#include <ctype.h>

#include <util.h>
#include <mydata.h>

char timestr[11]; //usata per il timestamp nel file di log

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
		CHECK_EQ_EXIT("malloc", NULL,(data->file_name = malloc(strlen(bname))), "allocando il nome file", "");
		strncpy(data->file_name, bname, strlen(bname));
		//parte lock
		data->lock_flag = 1;
		data->locked_by = id;
		//inizializzo le variabili di mutex che serviranno per modificare la lock in seguito
		pthread_mutex_init(&(data->lock), NULL);
		pthread_cond_init(&(data->cond), NULL);
		fprintf(stdout, "%s[LOG] Locked file %s\n", tStamp(timestr), (char*)key);
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

	int ret, retval = 0;
	size_t fsize;
	void * buf;
	SYSCALL_EXIT("read", ret, readn(fd, &fsize, sizeof(size_t)), "leggendo dati dal client", "");
	CHECK_EQ_EXIT("malloc", NULL,(buf = malloc(fsize)), "allocando la stringa buf", "");
	SYSCALL_EXIT("read", ret, readn(fd, buf, fsize), "leggendo dati dal client", "");
	file_t *data;
	data = icl_hash_find(table, key);
	if(data != NULL) {
		CHECK_EQ_EXIT("malloc", NULL,(data->contenuto = malloc(fsize)), "allocando la stringa data", "");	
		memcpy(data->contenuto, buf, fsize);
		data->file_size = fsize;

		fprintf(stdout, "%s[LOG] Writed file %s (%d Bytes)\n", tStamp(timestr), (char*)key, (int)fsize);
		CUR_CAP = CUR_CAP + fsize;
		CUR_FIL++;
	}
	else {
		fprintf(stderr, "errore nella scrittura del file nel server\n");
		retval = -1;
	}
	SYSCALL_EXIT("write", ret, writen(fd, &retval, sizeof(int)), "inviando dati al client", "");
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

void workerF(void *arg) {
	assert(arg);

    long connfd = ((workerArg_t*)arg)->clientfd;
	int req_pipe = ((workerArg_t*)arg)->pipe; 
	icl_hash_t * htab = ((workerArg_t*)arg)->hash_table; 

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
    }
}