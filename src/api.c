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

long connfd;
char *socket_name;

//inizializza la richiesta da mandare al server
//NON SO SE VA AGGIUNTA DIRNAME, AGGIORNARE IN SEGUITO
request_t * initRequest(int t, const char * fname, int n){
	request_t * req;
	if((req = malloc(sizeof(request_t))) == NULL) {
		perror("malloc");
		return NULL;
	}
	req->type = t;
	req->cid = getpid();
	req->arg = n;
	if(fname != NULL){
		//devo ricavare il path assoluto del file
		char abs_path[PATH_MAX];
		char *ptr = NULL;
		if((ptr = realpath(fname, abs_path)) == NULL) {
			fprintf(stderr, "%s non esiste\n", fname);
			errno = EINVAL;
			return NULL;
		}
		strncpy(req->filepath, abs_path, PATH_MAX);
	}
	return req;
}

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
    int ret;
    request_t * rts;
    rts = initRequest(OPEN_CONNECTION, NULL, 0);
    if(rts == NULL) return -1;
    SYSCALL_RETURN("write", ret, writen(sockfd, rts, sizeof(request_t)), "inviando dati al server", "");

	connfd = sockfd;
    socket_name = malloc(strlen(sockname)*sizeof(char));
    strncpy(socket_name, sockname, strlen(sockname));
    free(rts);
	return 0;
}

int closeConnection(const char *sockname) {
	if(strncmp(sockname, socket_name, strlen(sockname)) != 0) {
		errno = EINVAL;
		return -1;
	}
	request_t * rts;
	rts = initRequest(CLOSE_CONNECTION, NULL, 0);
	if(rts == NULL) return -1;
	int ret;
	SYSCALL_RETURN("write", ret, writen(connfd, rts, sizeof(request_t)), "inviando dati al server", "");

	close(connfd);
	connfd = -1;
  	if(socket_name) free(socket_name);
  	free(rts);
	return 0;
}

int openFile(const char *pathname, int flags) {

	int ret, retval;
	request_t * rts;
	rts = initRequest(OPEN_FILE, pathname, flags);
	if(rts == NULL) return -1;

	SYSCALL_RETURN("write", ret, writen(connfd, rts, sizeof(request_t)), "inviando dati al server", "");
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati al server", "");
	free(rts);
	return retval;
}

int writeFile(const char *pathname, const char *dirname) {

	int ret, retval;
	request_t * rts;
	rts = initRequest(WRITE_FILE, pathname, 0);
	if(rts == NULL) return -1;
	
	/*	PARTE READ
	*   non sapendo a priori le dimensioni dei file che andranno nello storage ho deciso
	*	di utilizzare stat per sapere esattamente la dimensione del file da leggere per inizializzare 
	*	il buffer e comunicare la size precisa al server
	*/
	FILE *fp;
	char *buffer;
	size_t fsize, num;
	struct stat statbuf;

	SYSCALL_RETURN("stat", ret, stat(pathname, &statbuf), "facendo la stat del file in lettura", "");
	if((fp = fopen(pathname, "r")) == NULL) {
		perror("fopen");
		return -1;
	}
	fsize = statbuf.st_size;
	if((buffer = malloc(fsize)) == NULL) {
		perror("malloc");
		return -1;
	}
	num = fread(buffer, 1, fsize, fp);
	if(num != fsize) {
		fprintf(stderr, "errore nella lettura del file\n");
		return -1;
	}
	fclose(fp);
	//invio la richiesta ed il file al server, leggo l'esito
	SYSCALL_RETURN("write", ret, writen(connfd, rts, sizeof(request_t)), "inviando dati al server", "");
	SYSCALL_RETURN("write", ret, writen(connfd, &fsize, sizeof(size_t)), "inviando dati al server", "");
	SYSCALL_RETURN("write", ret, writen(connfd, buffer, fsize), "inviando dati al server", "");
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");

	free(buffer);
	free(rts);
	return retval;
}

/*
int readFile(const char* pathname, void** buf, size_t* size) {

	int ret, retval;
	request_t * rts;
	rts = initRequest(READ_FILE, pathname, 0);
	if(rts == NULL) return -1;
	SYSCALL_RETURN("write", ret, writen(connfd, rts, sizeof(request_t)), "inviando dati al server", "");

	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");

	if(retval == 0) {
		
		void* buf2;
		SYSCALL_RETURN("read", ret, readn(connfd, size, sizeof(size_t)), "ricevendo dati dal server", ""); 
		if((buf2 = malloc(*size)) == NULL ) {
			perror("malloc");
			return -1;
		} 
		SYSCALL_RETURN("read", ret, readn(connfd, buf2, *size), "ricevendo dati dal server", "");

		file_t * readfile;
		readfile = buf2;
		printf("[CLIENT] Letto file: %s (dim: %d Bytes)\n", (char*)readfile->contenuto, (int)readfile->file_size);



		printf("FILE RICHIESTO (%s) (dim: %dbytes):\n"
				"----------------------------------------\n%s"
				"----------------------------------------\n", pathname, (int) *size, (char*)buf2); 
		(*buf) = buf2;
	}
	free(rts);
	return retval;
} */

int readFile(const char* pathname, void** buf, size_t* size) {

	int ret, retval;
	request_t * rts;
	rts = initRequest(READ_FILE, pathname, 0);
	if(rts == NULL) return -1;
	SYSCALL_RETURN("write", ret, writen(connfd, rts, sizeof(request_t)), "inviando dati al server", "");

	//PARTE READ
	//retval mi dice se il file esiste o meno nel server
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");

	if(retval == 0) {
		SYSCALL_RETURN("read", ret, readn(connfd, size, sizeof(size_t)), "ricevendo dati dal server", ""); 
		void* buf2 = malloc(*size);
		if(buf2 == NULL) {
			perror("malloc");
			return -1;
		}
		SYSCALL_RETURN("read", ret, readn(connfd, buf2, *size), "ricevendo dati dal server", "");
		//STAMPA A FINI DI TEST
		printf("FILE RICHIESTO (%s) (dim: %dbytes):\n"
				"----------------------------------------\n%s"
				"----------------------------------------\n", pathname, (int) *size, (char*)buf2);
		(*buf) = buf2;
	}
	free(rts);
	return retval;
}

int readNFiles(int N, const char* dirname) {
	int ret;
	request_t * rts;
	rts = initRequest(READ_N_FILES, NULL, N);
	if(rts == NULL) return -1;
	SYSCALL_RETURN("write", ret, writen(connfd, rts, sizeof(request_t)), "inviando dati al server", "");

	//PARTE READ N
	int end, count = 0;
	size_t fsize, namelen;
	char *fname = NULL;
	char *buf = NULL;

	//per qualche ragione le realloc non funzionano se non faccio prima una malloc
	//fixare in seguito se rimane tempo
	fname = malloc(64);
		if(fname == NULL) {
			perror("malloc");
			return -1;
		}
	buf = malloc(64);
		if(buf == NULL) {
			perror("malloc");
			return -1;
		}
	char synch = 's';

	//non so a priori quanti file sono presenti sul server, devo essere notificato
	SYSCALL_RETURN("read", ret, readn(connfd, &end, sizeof(int)), "ricevendo dati dal server", ""); 
	while(!end){
		//leggo il nome del file
		SYSCALL_RETURN("read", ret, readn(connfd, &namelen, sizeof(size_t)), "ricevendo dati dal server", "");
		if(realloc(fname, namelen+1) == NULL){
			perror("realloc");
			return -1;
		}
		SYSCALL_RETURN("read", ret, readn(connfd, fname, namelen), "ricevendo dati dal server", "");
		fname[namelen] = '\0'; //per la stampa del nome, in caso del salvataggio 

		//leggo il contenuto del file
		SYSCALL_RETURN("read", ret, readn(connfd, &fsize, sizeof(size_t)), "ricevendo dati dal server", "");
		if(realloc(buf, fsize) == NULL) {
			perror("realloc");
			return -1;
		}
		SYSCALL_RETURN("read", ret, readn(connfd, buf, fsize), "ricevendo dati dal server", ""); 

		//se specificato devo salvare il file
		if(dirname != NULL) saveFile(dirname, fname, buf, fsize);

		//comunico la fine della lettura
		SYSCALL_RETURN("write", ret, write(connfd, &synch, 1), "inviando dati al server", "");
		//leggo se c'e' un altro file in arrivo
		SYSCALL_RETURN("read", ret, readn(connfd, &end, sizeof(int)), "ricevendo dati dal server", "");
		count++;
	}
	if(count == 0) printf("Nessun file presente sul server\n");
	if(buf) free(buf);
	if(fname) free(fname);
	free(rts);
	return count;
}

int unlockFile(const char* pathname) {
	int ret, retval;
	request_t * rts;
	rts = initRequest(UNLOCK_FILE, pathname, 0);
	if(rts == NULL) return -1;
	SYSCALL_RETURN("write", ret, writen(connfd, rts, sizeof(request_t)), "inviando dati al server", "");
	SYSCALL_RETURN("read", ret, read(connfd, &retval, sizeof(int)), "inviando dati al server", "");
	return retval;
}

int lockFile(const char* pathname) {
	int ret, retval;
	request_t * rts;
	rts = initRequest(LOCK_FILE, pathname, 0);
	if(rts == NULL) return -1;
	SYSCALL_RETURN("write", ret, writen(connfd, rts, sizeof(request_t)), "inviando dati al server", "");
	SYSCALL_RETURN("read", ret, read(connfd, &retval, sizeof(int)), "inviando dati al server", "");
	return retval;
}

int removeFile(const char* pathname) {
	int ret, retval;
	request_t * rts;
	rts = initRequest(REMOVE_FILE, pathname, 0);
	if(rts == NULL) return -1;
	SYSCALL_RETURN("write", ret, writen(connfd, rts, sizeof(request_t)), "inviando dati al server", "");
	SYSCALL_RETURN("read", ret, read(connfd, &retval, sizeof(int)), "inviando dati al server", "");
	return retval;
}

//API aggiuntiva, salva localmente nella cartella dirname un file con il contenuto puntato dal buffer
int saveFile(const char* dirname, const char* pathname, void* buf, size_t size) {
	 
	//provo a creare la directory, se gia' esiste il file controllo che sia una directory
	struct stat statbuf;
	int unused, r;
	r = mkdir(dirname, 0777);
	if(r == -1 && errno != EEXIST) {
		perror("mkdir");
		return -1;
	}
	if(r == -1) {
		SYSCALL_EXIT(stat, unused, stat(dirname, &statbuf), "facendo la stat di %s\n", dirname);
		if(!S_ISDIR(statbuf.st_mode)) {
			fprintf(stderr, "%s non e' una directory\n", dirname);
			return -1;
		}
	}
	//ricavo il nome da salvare da pathname(formato sconosciuto a priori)
	char *filepath;
	size_t len;
	if(strchr(pathname, '/') != NULL) {
		char *tmp_str = strdup(pathname);
		char *bname = basename(tmp_str);
		len = strlen(dirname)+strlen(bname)+2; //carattere '/' e terminatore
		filepath = malloc(len);
		if(filepath == NULL){
			perror("malloc");
			return -1;
		}
		sprintf(filepath, "%s/%s", dirname, bname);
	}
	else {
		len = strlen(dirname)+strlen(pathname)+2;
		filepath = malloc(len);
		if(filepath == NULL){
			perror("malloc");
			return -1;
		}
		sprintf(filepath, "%s/%s", dirname, pathname);
	}

	//creo un nuovo file all'interno della directory
	FILE *fp;
	if((fp = fopen(filepath, "w+")) == NULL) {
		perror("fopen");
		return -1;
	}
	if(fwrite(buf, 1, size, fp) != size ) {
		perror("fwrite");
		return -1;
	}
	printf("[CLIENT] Saved file %s\n", filepath);
	free(filepath);
	fclose(fp);
	return 0;
}