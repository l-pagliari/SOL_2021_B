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

void handler_openfile(long fd, icl_hash_t * table, void * key, int flag) {

	int ret, retval = 0;
	file_t * data;
	if(flag == O_CREATE || flag == (O_CREATE | O_LOCK)) {
		CHECK_EQ_EXIT("malloc", NULL,(data = malloc(sizeof(file_t))), "allocando un nuovo file", "");
		data->contenuto = NULL;
		data->file_size = 0;
		pthread_mutex_init(&(data->lock), NULL);
		pthread_cond_init(&(data->cond), NULL);
		data->lock_flag = 0;
		data->locked_by = -1;

		char *tmp_str = strdup(key);
		char *bname = basename(tmp_str);
		CHECK_EQ_EXIT("malloc", NULL,(data->file_name = malloc(strlen(bname))), "allocando il nome file", "");
		strncpy(data->file_name, bname, strlen(bname));

		if(icl_hash_insert(table, key, data) == NULL) {
			fprintf(stderr, "%s esiste gia' nel server\n", (char*)key);
			retval = -1;
		}
	}




	else {
		data = icl_hash_find(table, key);
		if(data == NULL) {
			fprintf(stderr, "ERRORE: file richiesto non trovato\n");
			retval = -1;
		}
	}
	SYSCALL_EXIT("write", ret, writen(fd, &retval, sizeof(int)), "inviando dati al client", "");
}

void handler_writefile(long fd, icl_hash_t * table, void * key) {

	int ret, retval = 0;
	size_t sz;
	void * buf;
	SYSCALL_EXIT("read", ret, readn(fd, &sz, sizeof(size_t)), "leggendo dati dal client", "");
	CHECK_EQ_EXIT("malloc", NULL,(buf = malloc(sz*sizeof(char))), "allocando la stringa buf", "");
	SYSCALL_EXIT("read", ret, readn(fd, buf, sz*sizeof(char)), "leggendo dati dal client", "");
	file_t *data;
	data = icl_hash_find(table, key);
	if(data != NULL) {
		CHECK_EQ_EXIT("malloc", NULL,(data->contenuto = malloc(sz)), "allocando la stringa data", "");	
		memcpy(data->contenuto, buf, sz);
		data->file_size = sz;
	}
	if(retval == 0) fprintf(stdout, "%s[LOG] Writed file %s (%d Bytes)\n", tStamp(timestr), (char*)key, (int)sz);

	CUR_CAP = CUR_CAP + sz;
	CUR_FIL++;

	SYSCALL_EXIT("write", ret, writen(fd, &retval, sizeof(int)), "inviando dati al client", "");
}

void handler_readfile(long fd, icl_hash_t * table, void * key) {

	int ret, retval = 0;
	file_t *data;
	if((data = icl_hash_find(table, key)) == NULL ) retval = -1;
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

    	case CLOSE_CONNECTION:
    		close(connfd);
    		fprintf(stdout, "%s[LOG] Closed connection (client ?)\n", tStamp(timestr));
    		break;

    	case OPEN_FILE:
    		handler_openfile(connfd, htab, req->path, req->flag);
    		SYSCALL_EXIT("write", notused, write(req_pipe, &connfd, sizeof(long)), "scrivendo nella request pipe", "");
    		break;
		
    	case WRITE_FILE:
    		handler_writefile(connfd, htab, req->path);
    		SYSCALL_EXIT("write", notused, write(req_pipe, &connfd, sizeof(long)), "scrivendo nella request pipe", "");
    		break;
    
    	case READ_FILE:
    		handler_readfile(connfd, htab, req->path);
    		SYSCALL_EXIT("write", notused, write(req_pipe, &connfd, sizeof(long)), "scrivendo nella request pipe", "");
    		break;
    }
}