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
#include <signal.h>
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

long ms_delay = 0;
int delay = 0;

int quiet;

/* inizializzo la richiesta da mandare al server sottoforma di una struct, uguale
   per ogni richiesta */
request_t * initRequest(int t, const char * fname, int n){
	request_t * req = NULL;
	if((req = malloc(sizeof(request_t))) == NULL) {
		perror("malloc");
		return NULL;
	}
	req->type = t;
	/*utilizzo il pid come identificativo del client, ci sono altri modi se non 
	  sufficiente ma per gli scopi del progetto lo ritengo un buon compromesso */
	req->cid = getpid();
	req->arg = n;
	if(fname != NULL){
		/* ricavo il path assoluto del file che viene usato come chiave unica per identificare
		   il file nel server */
		char abs_path[PATH_MAX];
		char *ptr = NULL;
		if((ptr = realpath(fname, abs_path)) == NULL) {
			fprintf(stderr, "%s non esiste\n", fname);
			errno = EINVAL;
			return NULL;
		}
		strncpy(req->filepath, abs_path, PATH_MAX);
	} else memset(req->filepath, '\0', PATH_MAX); //valgrind
	return req;
}

void print_err(char * str, int err);

/* inizializzo la richiesta da mandare al server sottoforma di una struct, uguale
   per ogni richiesta */

int sendRequest(int type, const char * filename, int flag, long fd){
	int ret, r;
	request_t * req = NULL;
	if((req = malloc(sizeof(request_t))) == NULL) {
		//perror("malloc");
		return err_memory_alloc;
	}
	req->type = type;
	/*utilizzo il pid come identificativo del client, ci sono altri modi se non 
	  sufficiente ma per gli scopi del progetto lo ritengo un buon compromesso */
	req->cid = getpid();
	req->arg = flag;
	if(filename != NULL){
		/* ricavo il path assoluto del file che viene usato come chiave unica per identificare
		   il file nel server */
		char abs_path[PATH_MAX];
		char *ptr = NULL;
		if((ptr = realpath(filename, abs_path)) == NULL) {
			//fprintf(stderr, "%s non esiste\n", fname);
			return err_path_invalid;
		}
		strncpy(req->filepath, abs_path, PATH_MAX);
	} else memset(req->filepath, '\0', PATH_MAX); //valgrind
	//invio la richiesta al server 
	SYSCALL_EXIT("write", ret, writen(fd, req, sizeof(request_t)), "inviando dati al server", "");
	//controllo server busy
   SYSCALL_EXIT("read", ret, readn(fd, &r, sizeof(int)), "ricevendo dati al server", "");
	free(req);  
	return r;
}



/* prova a connettersi al server attraverso il socket sockname, se fallisce 
   riprova ad intervalli regolari msec fino al passaggio del tempo assoluto abstime */
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
   //tento a riconnettermi se la connect fallisce
  	while(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
    	nanosleep(&sleeptime, NULL);
    	maxtime = maxtime + msec;
    	printf("tento nuovamente a connettermi...\n");
    	if(maxtime >= abstime.tv_sec * 1000) {
    		fprintf(stderr, "open connection: impossibile connettersi\n");
    		errno = ETIMEDOUT;
    		return -1;
		}
   }
   int r;
   r = sendRequest(OPEN_CONNECTION, NULL, 0, sockfd);
   if(r<0) {
   	print_err("open connection", r);
   	return -1;
   }
	connfd = sockfd;
   socket_name = malloc(BUFSIZE*sizeof(char));
   strncpy(socket_name, sockname, BUFSIZE);
	if(delay) msleep(ms_delay);;
	return 0;
}

int closeConnection(const char *sockname) {
	if(strncmp(sockname, socket_name, BUFSIZE) != 0) {
		//errno = EINVAL;
		return err_args_invalid;
	}
	int r;
   r = sendRequest(CLOSE_CONNECTION, NULL, 0, connfd);

   close(connfd);
	connfd = -1;
  	if(socket_name) free(socket_name);
   if(r<0) {
   	print_err("close connection", r);
   	return -1;
   }
	return 0;
}

int openFile(const char *pathname, int flags) {

	int ret, retval, r;
   r = sendRequest(OPEN_FILE, pathname, flags, connfd);
   if(r<0) {
   	print_err("open file", r);
   	return -1;
   }
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati al server", "");
	if(retval<0) {
   	print_err("open file", retval);
   	if(delay) msleep(ms_delay);
   	return -1;
   }
	if(delay) msleep(ms_delay);
	return 0;
}

int writeFile(const char *pathname, const char *dirname) {

	int ret, retval, r;
	r = sendRequest(WRITE_FILE, pathname, 0, connfd);
   if(r<0) {
   	print_err("open file", r);
   	return -1;
   }
	//leggo se il server e' pronto a ricevere il file
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");
	if(retval<0) {
   	print_err("write file", retval);
   	return -1;
   }
	FILE *fp;
	void *buffer;
	size_t fsize, num;
	struct stat statbuf;
	// utilizzo stat per conoscere in anticipo la size del file che dovro' leggere 
	SYSCALL_RETURN("stat", ret, stat(pathname, &statbuf), "facendo la stat del file in lettura", "");
	if((fp = fopen(pathname, "r")) == NULL) {
		perror("write file: fopen");
		return -1;
	}
	fsize = statbuf.st_size;
	if((buffer = malloc(fsize)) == NULL) {
		perror("write file: malloc");
		return -1;
	}
	num = fread(buffer, 1, fsize, fp);
	if(num != fsize) {
		perror("write file: fwrite");
		return -1;
	}
	fclose(fp);
	//invio size e contenuto al server
	SYSCALL_RETURN("write", ret, writen(connfd, &fsize, sizeof(size_t)), "inviando dati al server", "");
	SYSCALL_RETURN("write", ret, writen(connfd, buffer, fsize), "inviando dati al server", "");
	//il server mi comunica se c'e' abbastanza spazio in memoria
	retval = 0;
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");
	//1.retval == 0: c'e' spazio
	//2.retval == 1: il server deve espellere uno o piu' file per fare spazio al nuovo
	//3.retval < 0: si e' verificato un qualche tipo di errore
	if(retval < 0) {
		print_err("write file", retval);
		free(buffer);
		return -1;
	}
	//uso un handler per gestire i file che il server mi manda per fare spazio
	if(retval == 1 && !quiet) fprintf(stdout, "[CLIENT] Storage pieno, uno o piu' file verra' espulso dal server\n");
	while(retval == 1) retval = capacityMissHandler(dirname);
	//leggo l'esito della scrittura
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");
	if(retval < 0) {
		print_err("write file", retval);
		free(buffer);
		return -1;
	}
	if(!quiet) 
		fprintf(stdout, "[CLIENT] Scritto sul server il file %s (%ld MB)\n", pathname, fsize/1024/1024);
	
	free(buffer);
	if(delay) msleep(ms_delay);
	return 0;
}
		
int capacityMissHandler(const char *dirname) {
	
	int ret, retval = 1;
	size_t expfsize, namelen;
	char fname[PATH_MAX];
	char *buf = NULL;
	char synch = 's';

	//leggo il nome del file
	SYSCALL_RETURN("read", ret, readn(connfd, &namelen, sizeof(size_t)), "ricevendo dati dal server", "");
	SYSCALL_RETURN("read", ret, readn(connfd, fname, namelen), "ricevendo dati dal server", "");
	fname[namelen] = '\0'; //per la stampa del nome, in caso del salvataggio 
	//leggo il contenuto del file
	SYSCALL_RETURN("read", ret, readn(connfd, &expfsize, sizeof(size_t)), "ricevendo dati dal server", "");
	buf = malloc(expfsize*sizeof(char)); 
	if(buf == NULL) {
		perror("malloc");
		return -1;
	}
	SYSCALL_RETURN("read", ret, readn(connfd, buf, expfsize), "ricevendo dati dal server", ""); 

	if(!quiet) fprintf(stdout, "[CLIENT] Ricevuto il file %s (%ld MB) a seguito di un capacity miss\n", fname, expfsize/1024/1024);

	//se specificato devo salvare il file
	if(dirname != NULL) {
		saveFile(dirname, fname, buf, expfsize);
		//if(!quiet) fprintf(stdout, "[CLIENT] Salvato il file %s nella directory %s\n", fname, dirname);
	}
	//comunico la fine della lettura
	SYSCALL_RETURN("write", ret, write(connfd, &synch, 1), "inviando dati al server", "");
	//leggo se c'e' un altro file in arrivo
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");
	
	free(buf);
	return retval;
}

int writeDirectory(const char *dirname, int max_files, const char *writedir) {
	//controllo input per essere certi che e' stata passata una directory
	struct stat statbuf;
	int r;
	DIR * dir;

	SYSCALL_EXIT(stat, r, stat(dirname, &statbuf), "facendo la stat di %s: errno =%d", dirname, errno);
	if(!S_ISDIR(statbuf.st_mode)) {
		fprintf(stderr, "write dir: directory non valida\n");
		return -1;
	}
	if(chdir(dirname) == -1) {
		fprintf(stderr,"write dir: impossibile accedere a %s\n", dirname);
		return -1;
	}
	if((dir = opendir(".")) == NULL) {
		fprintf(stderr, "write dir: errore apertura %s\n", dirname);
		return -1;
	} else {
		struct dirent *file;
		while((errno = 0, file = readdir(dir)) != NULL) {
			struct stat statbuf;
			if(stat(file->d_name, &statbuf) == -1) {
				perror("write dir: stat");
				return -1;
			}
			//devo gestire il caso di sotto-directory, ricorsivamente
			if(S_ISDIR(statbuf.st_mode)) {
				if(!isDot(file->d_name)) {
					//faccio find ricorsivamente nella sottocartella
					if(writeDirectory(file->d_name, max_files, writedir) != -2) { 
						if(chdir("..") == -1) {
							fprintf(stderr, "write dir: impossibile risalire alla directory parent\n");
							return -1;
						}
					}
				}
			}
			else { //ho un effettivo file da mandare al server
				int r;
				r = openFile(file->d_name, O_CREATE|O_LOCK);
				if(r == 0) {
				 	writeFile(file->d_name, writedir);
					closeFile(file->d_name);
				}
				if(max_files == 1) {
					if(!quiet) fprintf(stdout, "[CLIENT] Letto il numero di file chiesto dalla directory\n");
					return 0;
				}
				if(max_files > 1) max_files--;
			}
		}
		//sono alla fine del while e devo controllare errno perche' non posso sapere perche' e' terminato
		if(errno != 0) perror("write dir: readdir");
		closedir(dir);
	}
	if(!quiet) fprintf(stdout, "[CLIENT] Fine operazione di scrittura directory %s\n", dirname);
	if(delay) msleep(ms_delay);
	return 0;
}

int readFile(const char* pathname, void** buf, size_t* size) {

	int ret, retval;
	retval = sendRequest(READ_FILE, pathname, 0, connfd);
   if(retval<0) {
   	print_err("read file", retval);
   	return -1;
   }
	//retval mi dice se il file esiste o meno nel server
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");
	if(retval<0) {
   	print_err("read file", retval);
   	return -1;
   }
	SYSCALL_RETURN("read", ret, readn(connfd, size, sizeof(size_t)), "ricevendo dati dal server", ""); 
	void* buf2 = malloc(*size);
	if(buf2 == NULL) {
		perror("read file: malloc");
		return -1;
	}
	SYSCALL_RETURN("read", ret, readn(connfd, buf2, *size), "ricevendo dati dal server", "");
	(*buf) = buf2;
	
	if(delay) msleep(ms_delay);
	return 0;
}

int readNFiles(int N, const char* dirname) {
	int ret, retval;
	retval = sendRequest(READ_N_FILES, NULL, N, connfd);
   if(retval<0) {
   	print_err("read file", retval);
   	return -1;
   }
	int end, count = 0;
	size_t fsize, namelen;
	char fname[PATH_MAX];
	char *buf = NULL;

	char synch = 's';
	//non so a priori quanti file sono presenti sul server, devo essere notificato
	SYSCALL_RETURN("read", ret, readn(connfd, &end, sizeof(int)), "ricevendo dati dal server", ""); 

	while(!end){
		//leggo il nome del file
		SYSCALL_RETURN("read", ret, readn(connfd, &namelen, sizeof(size_t)), "ricevendo dati dal server", "");
		SYSCALL_RETURN("read", ret, readn(connfd, fname, namelen), "ricevendo dati dal server", "");
		fname[namelen] = '\0'; //per la stampa del nome, in caso del salvataggio 
		//leggo il contenuto del file
		SYSCALL_RETURN("read", ret, readn(connfd, &fsize, sizeof(size_t)), "ricevendo dati dal server", "");
		buf = malloc(fsize);
		if(buf == NULL) {
			perror("read N: malloc");
			return -1;
		}
		SYSCALL_RETURN("read", ret, readn(connfd, buf, fsize), "ricevendo dati dal server", ""); 

		if(!quiet) printf("[CLIENT] Letto il file %s (%ld MB)\n", fname, fsize/1024/1024);
		//se specificato devo salvare il file
		if(dirname != NULL) saveFile(dirname, fname, buf, fsize);

		//comunico la fine della lettura
		SYSCALL_RETURN("write", ret, write(connfd, &synch, 1), "inviando dati al server", "");
		//leggo se c'e' un altro file in arrivo
		SYSCALL_RETURN("read", ret, readn(connfd, &end, sizeof(int)), "ricevendo dati dal server", "");
		count++;
		free(buf);
	}
	if(end < 0) {
		print_err("read N", end);
		if(delay) msleep(ms_delay);
		return -1;
	}
	else if(count == 0 && !quiet) fprintf(stdout, "[CLIENT] Nessun file presente sul server\n");

	if(delay) msleep(ms_delay);
	return count;
}

int unlockFile(const char* pathname) {
	int ret, retval;
	retval = sendRequest(UNLOCK_FILE, pathname, 0, connfd);
   if(retval<0){
   	print_err("unlock file", retval);
   	return -1;
   } 
	SYSCALL_RETURN("read", ret, read(connfd, &retval, sizeof(int)), "inviando dati al server", "");
	if(retval<0){
   	print_err("unlock file", retval);
   	if(delay) msleep(ms_delay);
   	return -1;
   } 
	if(delay) msleep(ms_delay);
	return 0;
}

int lockFile(const char* pathname) {
	int ret, retval;
	retval = sendRequest(LOCK_FILE, pathname, 0, connfd);
   if(retval<0){
   	print_err("lock file", retval);
   	return -1;
   } 
	SYSCALL_RETURN("read", ret, read(connfd, &retval, sizeof(int)), "inviando dati al server", "");
	if(retval<0){
   	print_err("lock file", retval);
   	if(delay) msleep(ms_delay);
   	return -1;
   } 
	if(delay) msleep(ms_delay);
	return 0;
}

int removeFile(const char* pathname) {
	int ret, retval;
	retval = sendRequest(REMOVE_FILE, pathname, 0, connfd);
   if(retval<0){
   	print_err("remove file", retval);
   	return -1;
   } 
	SYSCALL_RETURN("read", ret, read(connfd, &retval, sizeof(int)), "inviando dati al server", "");
	if(retval<0){
   	print_err("remove file", retval);
   	if(delay) msleep(ms_delay);
   	return -1;
   } 
	if(delay) msleep(ms_delay);
	return 0;
}

int closeFile(const char* pathname){
	int ret, retval;
	retval = sendRequest(CLOSE_FILE, pathname, 0, connfd);
   if(retval<0){
   	print_err("close file", retval);
   	return -1;
   } 
	SYSCALL_RETURN("read", ret, read(connfd, &retval, sizeof(int)), "inviando dati al server", "");
	if(retval<0){
   	print_err("close file", retval);
   	if(delay) msleep(ms_delay);
   	return -1;
   } 
	if(delay) msleep(ms_delay);
	return 0;
}

//API aggiuntiva, salva localmente nella cartella dirname un file con il contenuto puntato dal buffer
int saveFile(const char* dirname, const char* pathname, void* buf, size_t size) {
	 //ricavo il nome da salvare da pathname, formato sconosciuto a priori
	char *filepath;
	size_t len;
	if(strchr(pathname, '/') != NULL) {
		char *tmp_str = strdup(pathname);
		char *bname = basename(tmp_str);
		len = strlen(dirname)+strlen(bname)+2; //carattere '/' e terminatore
		filepath = malloc(len*sizeof(char));
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
	if(fwrite(buf, sizeof(char), size, fp) != size ) {
		perror("fwrite");
		return -1;
	}
	if(!quiet) fprintf(stdout, "[CLIENT] Salvato %s\n", filepath);
	free(filepath);
	fclose(fp);
	if(delay) msleep(ms_delay);
	return 0;
}

int setDelay(long msec) {
	ms_delay = msec;
   delay = 1;
   return 0;
}

/*
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {

	int ret, retval;
	request_t * rts;
	rts = initRequest(APPEND_FILE, pathname, 0);
	if(rts == NULL) return -1;
	SYSCALL_RETURN("write", ret, writen(connfd, rts, sizeof(request_t)), "inviando dati al server", "");

	//invio la size ed il buffer
	SYSCALL_RETURN("write", ret, writen(connfd, &size, sizeof(size_t)), "inviando dati al server", "");
	SYSCALL_RETURN("write", ret, writen(connfd, buf, size), "inviando dati al server", "");
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");

	while(retval == 1) retval = capacityMissHandler(dirname);

	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");
	return retval;
}*/


/* TO DO:
	-append
	-initRequest ingloba l'invio e il check per busy -> nella initRequest c'e' ancora l'error handling vecchio.
*/

void print_err(char * str, int err) {
	switch(err){
		case err_server_busy:
			fprintf(stderr, "[ERR]%s: server occupato, riprovare...\n", str);
			/*forzatura per momenti atipici di sovraccarico (stress test), soluzioni
			  meno brutali come un try again con timeout ancora non implementate */
			exit(EXIT_FAILURE);
			break;
		case err_worker_busy:
			fprintf(stderr, "[ERR]%s: worker occupati, operazione annullata\n", str);
			/*forzatura per momenti atipici di sovraccarico (stress test), soluzioni
			  meno brutali come un try again con timeout ancora non implementate 
			exit(EXIT_FAILURE); */
			break;
		case err_memory_alloc: 
			fprintf(stderr, "[ERR] %s: memory allocation\n", str);
			break;
		case err_storage_fault:
			fprintf(stderr, "[ERR] %s: errore gestione interna storage\n", str);
			break;
		case err_file_exist:
			fprintf(stderr, "[ERR] %s: file gia' presente in memoria\n", str);
			break;
		case err_file_notexist:
			fprintf(stderr, "[ERR] %s: file non esiste in memoria\n", str);
			break;
		case err_file_locked:
			fprintf(stderr, "[ERR] %s: file lockato da un altro utente\n", str);
			break;
		case err_file_notlocked:
			fprintf(stderr, "[ERR] %s: file non lockato\n", str);
			break;
		case err_file_notopen:
			fprintf(stderr, "[ERR] %s: file non aperto\n", str);
			break;
		case err_file_toobig:
			fprintf(stderr, "[ERR] %s: file troppo grande\n", str);
			break;
		case err_path_invalid:
			fprintf(stderr, "[ERR] %s: indirizzo file non valido\n", str);
			break;
		case err_args_invalid:
			fprintf(stderr, "[ERR] %s: valori input invalidi\n", str);
			break;

		default: 
			fprintf(stderr, "[ERR] %s: errore server (solitamente macro)\n", str);
			break;
	}
}