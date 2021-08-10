//#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <libgen.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/stat.h>

#include <api.h>
#include <mydata.h>
#include <util.h>

#define MAX_FILE_LEN 1024*1024*5 //5MB

long connfd;
char *socket_name;

int openConnection(const char *sockname, int msec, const struct timespec abstime) {

	struct sockaddr_un serv_addr;
    int sockfd;
    SYSCALL_RETURN("socket", sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
    memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;    
    strncpy(serv_addr.sun_path,sockname, strlen(sockname)+1);

    int maxtime = 0;
    struct timespec sleeptime;
    sleeptime.tv_sec = msec / 1000;
    sleeptime.tv_nsec = msec * 1000000;

    while(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
    	nanosleep(&sleeptime, NULL);
    	maxtime = maxtime + msec;
    	printf("tento nuovamente a connettermi...\n");
    	if(maxtime >= abstime.tv_sec * 1000) {
    		errno = ETIMEDOUT;
    		return -1;
		}
    }
    connfd = sockfd;
    socket_name = malloc(strlen(sockname)*sizeof(char));
    strncpy(socket_name, sockname, strlen(sockname));
	return 0;
}

int closeConnection(const char *sockname) {
	if(strncmp(sockname, socket_name, strlen(sockname)) != 0) {
		errno = EINVAL;
		return -1;
	}
	int ret;
	request_t * request;
	if((request = malloc(sizeof(request_t))) == NULL) {
		perror("malloc");
		return -1;
	}
	request->type = CLOSE_CONNECTION;
	SYSCALL_RETURN("write", ret, writen(connfd, request, sizeof(request_t)), "inviando dati al server", "");
	close(connfd);
	connfd = -1;
  	if(socket_name) free(socket_name);
	return 0;
}

int openFile(const char *pathname, int flags) {
	
	char abs_path[PATH_MAX];
	char *ptr;
	if((ptr = realpath(pathname, abs_path)) == NULL) {
		fprintf(stderr, "%s non esiste\n", pathname);
		errno = EINVAL;
		return -1;
	}
	int ret, retval;
	request_t * request;
	if((request = malloc(sizeof(request_t))) == NULL) {
		perror("malloc");
		return -1;
	}
	request->type = OPEN_FILE;
	strncpy(request->path, abs_path, PATH_MAX);
	request->flag = flags;
	SYSCALL_RETURN("write", ret, writen(connfd, request, sizeof(request_t)), "inviando dati al server", "");
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati al server", "");
	return retval;
}

int writeFile(const char *pathname, const char *dirname) {

	char abs_path[PATH_MAX];
	char *ptr;
	if((ptr = realpath(pathname, abs_path)) == NULL) {
		fprintf(stderr, "%s non esiste\n", pathname);
		errno = EINVAL;
		return -1;
	}
	int ret, retval;
	request_t * request;
	if((request = malloc(sizeof(request_t))) == NULL) {
		perror("malloc");
		return -1;
	}
	request->type = WRITE_FILE;
	strncpy(request->path, abs_path, PATH_MAX);
	SYSCALL_RETURN("write", ret, writen(connfd, request, sizeof(request_t)), "inviando dati al server", "");

	//PARTE WRITE

	FILE *fp;
	if((fp = fopen(abs_path, "r")) == NULL) {
		perror("fopen");
		return -1;
	}
	char *buffer; 
	buffer = malloc(MAX_FILE_LEN);
	if(buffer) {
		size_t f_size = fread(buffer, sizeof(char), MAX_FILE_LEN, fp); 
		//lo riduco perche' solitamente e' minore
		void *tmp = realloc(buffer, f_size+1); 
		if(tmp){
			buffer = tmp;
			buffer[f_size+1] = '\0'; //usiamo il contenuto dei file come stringhe nei test
			SYSCALL_RETURN("write", ret, writen(connfd, &f_size, sizeof(size_t)), "inviando dati al server", "");
			SYSCALL_RETURN("write", ret, writen(connfd, buffer, f_size), "inviando dati al server", "");
		}
		free(buffer);
	}
	fclose(fp);
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");
	return retval;
}

//una volta che usiamo questa funzione sappiamo che il file esiste nel server
int readFile(const char* pathname, void** buf, size_t* size) {

	char abs_path[PATH_MAX];
	char *ptr;
	if((ptr = realpath(pathname, abs_path)) == NULL) {
		fprintf(stderr, "%s non esiste\n", pathname);
		errno = EINVAL;
		return -1;
	}
	int ret, retval;
	request_t * request;
	if((request = malloc(sizeof(request_t))) == NULL) {
		perror("malloc");
		return -1;
	}
	request->type = READ_FILE;
	strncpy(request->path, abs_path, PATH_MAX);
	SYSCALL_RETURN("write", ret, writen(connfd, request, sizeof(request_t)), "inviando dati al server", "");

	//PARTE READ
	
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");
	if(retval == 0) {
		SYSCALL_RETURN("read", ret, readn(connfd, size, sizeof(size_t)), "ricevendo dati dal server", ""); 
		void* buf2 = malloc(*size);
		if(buf2 == NULL) {
			perror("malloc");
			return -1;
		}
		SYSCALL_RETURN("readn", ret, readn(connfd, buf2, *size), "ricevendo dati dal server", "");
		printf("FILE RICHIESTO (%s) (dim: %dbytes):\n"
				"----------------------------------------\n%s"
				"----------------------------------------\n", pathname, (int) *size, (char*)buf2); 
		(*buf) = buf2;
	}
	return retval;
}