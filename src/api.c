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

long ms_delay = 0;
int delay = 0;

int quiet;

/* inizializzo la richiesta da mandare al server sottoforma di una struct, uguale
   per ogni richiesta */
request_t * initRequest(int t, const char * fname, int n){
	request_t * req;
	if((req = (request_t*)malloc(sizeof(request_t))) == NULL) {
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
		strncpy(req->filepath, abs_path, PATH_MAX+1);
	}
	return req;
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
    		errno = ETIMEDOUT;
    		return -1;
		}
    }
    //mi sono connesso al socket, mando al server il nome del client
    int ret;
    request_t * rts;
    rts = initRequest(OPEN_CONNECTION, NULL, 0);
    if(rts == NULL) return -1;
    SYSCALL_RETURN("write", ret, writen(sockfd, rts, sizeof(request_t)), "inviando dati al server", "");

	connfd = sockfd;
    socket_name = malloc(BUFSIZE*sizeof(char));
    strncpy(socket_name, sockname, BUFSIZE);
    if(!quiet) fprintf(stdout, "[CLIENT] Connesso al socket %s\n", socket_name);
    free(rts);

    if(delay) msleep(ms_delay);;
	return 0;
}

int closeConnection(const char *sockname) {
	if(strncmp(sockname, socket_name, BUFSIZE) != 0) {
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
  	if(!quiet) fprintf(stdout, "[CLIENT] Chiusa la connessione con il socket %s\n", socket_name);
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
	if(delay) msleep(ms_delay);
	return retval;
}

int writeFile(const char *pathname, const char *dirname) {

	int ret, retval;
	request_t * rts;
	rts = initRequest(WRITE_FILE, pathname, 0);
	if(rts == NULL) return -1;
	SYSCALL_RETURN("write", ret, writen(connfd, rts, sizeof(request_t)), "inviando dati al server", "");
	//leggo se il server e' pronto a ricevere il file
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");
	if(retval == -1) return -1;
	
	FILE *fp;
	void *buffer;
	size_t fsize, num;
	struct stat statbuf;

	// utilizzo stat per conoscere in anticipo la size del file che dovro' leggere 
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
	//invio size e contenuto al server
	SYSCALL_RETURN("write", ret, writen(connfd, &fsize, sizeof(size_t)), "inviando dati al server", "");
	SYSCALL_RETURN("write", ret, writen(connfd, buffer, fsize), "inviando dati al server", "");
	//il server mi comunica se c'e' abbastanza spazio in memoria
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");
	//1.retval == 0: c'e' spazio
	//2.retval == 1: il server deve espellere uno o piu' file per fare spazio al nuovo
	//3.retval ==-1: si e' verificato un qualche tipo di errore
	if(retval == -1) {
		free(buffer);
		free(rts);
		return -1;
	}
	//uso un handler per gestire i file che il server mi manda per fare spazio
	while(retval == 1) retval = capacityMissHandler(dirname);
	//leggo l'esito della scrittura
	SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");
	if(!quiet && retval == 0) 
		fprintf(stdout, "[CLIENT] Scritto sul server il file %s (%ld Bytes)\n", pathname, fsize);
	
	free(buffer);
	free(rts);
	if(delay) msleep(ms_delay);
	return retval;
}
		
int capacityMissHandler(const char *dirname) {
	
	int ret, retval = 1;
	size_t expfsize, namelen;
	char *fname = NULL;
	char *buf = NULL;
	char synch = 's';

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

	//while(retval == 1) {
		//leggo il nome del file
		SYSCALL_RETURN("read", ret, readn(connfd, &namelen, sizeof(size_t)), "ricevendo dati dal server", "");
		if(realloc(fname, namelen+1) == NULL){
			perror("realloc");
			return -1;
		}
		SYSCALL_RETURN("read", ret, readn(connfd, fname, namelen), "ricevendo dati dal server", "");
		fname[namelen] = '\0'; //per la stampa del nome, in caso del salvataggio 

		//leggo il contenuto del file
		SYSCALL_RETURN("read", ret, readn(connfd, &expfsize, sizeof(size_t)), "ricevendo dati dal server", "");
		if(realloc(buf, expfsize) == NULL) {
			perror("realloc");
			return -1;
		}
		SYSCALL_RETURN("read", ret, readn(connfd, buf, expfsize), "ricevendo dati dal server", ""); 

		//se specificato devo salvare il file
		if(dirname != NULL) saveFile(dirname, fname, buf, expfsize);

		//comunico la fine della lettura
		SYSCALL_RETURN("write", ret, write(connfd, &synch, 1), "inviando dati al server", "");
		//leggo se c'e' un altro file in arrivo
		SYSCALL_RETURN("read", ret, readn(connfd, &retval, sizeof(int)), "ricevendo dati dal server", "");
	//}
	free(buf);
	free(fname);
	return retval;
}

int writeDirectory(const char *dirname, int max_files, const char *writedir) {
	//controllo input per essere certi che e' stata passata una directory
	struct stat statbuf;
	int r;
	DIR * dir;

	SYSCALL_EXIT(stat, r, stat(dirname, &statbuf), "facendo la stat di %s: errno =%d", dirname, errno);
	if(!S_ISDIR(statbuf.st_mode)) {
		fprintf(stderr, "%s non e' una directory\n", dirname);
		return -1;
	}
	if(chdir(dirname) == -1) {
		fprintf(stderr,"impossibile entrare nella directory %s\n", dirname);
		return -1;
	}
	if((dir = opendir(".")) == NULL) {
		fprintf(stderr, "errore aprendo la directory %s\n", dirname);
		return -1;
	} else {
		struct dirent *file;
		while((errno = 0, file = readdir(dir)) != NULL) {
			struct stat statbuf;
			if(stat(file->d_name, &statbuf) == -1) {
				perror("stat");
				return -1;
			}
			//devo gestire il caso di sotto-directory, ricorsivamente
			if(S_ISDIR(statbuf.st_mode)) {
				if(!isDot(file->d_name)) {
					//faccio find ricorsivamente nella sottocartella
					if(writeDirectory(file->d_name, max_files, writedir) != -2) { 
						if(chdir("..") == -1) {
							fprintf(stderr, "impossibile risalire alla directory parent\n");
							return -1;
						}
					}
				}
			}
			else { //ho un effettivo file da mandare al server
				int r;
				r = openFile(file->d_name, O_CREATE|O_LOCK);
				if(r == 0) writeFile(file->d_name, writedir);
				if(max_files == 1) {
					//printf("[CLIENT] Fine operazione\n");
				 	return 0;
				}
				if(max_files > 1) max_files--;
			}
		}
		//sono alla fine del while e devo controllare errno perche' non posso sapere perche' e' terminato
		if(errno != 0) perror("readdir");
		closedir(dir);
	}
	//printf("[CLIENT] Fine operazione\n");
	if(delay) msleep(ms_delay);
	return 0;
}

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
		/*printf("FILE RICHIESTO (%s) (dim: %dbytes):\n"
				"----------------------------------------\n%s"
				"----------------------------------------\n", pathname, (int) *size, (char*)buf2);*/
		(*buf) = buf2;
	}
	free(rts);
	if(delay) msleep(ms_delay);
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
	if(delay) msleep(ms_delay);
	return count;
}

int unlockFile(const char* pathname) {
	int ret, retval;
	request_t * rts;
	rts = initRequest(UNLOCK_FILE, pathname, 0);
	if(rts == NULL) return -1;
	SYSCALL_RETURN("write", ret, writen(connfd, rts, sizeof(request_t)), "inviando dati al server", "");
	SYSCALL_RETURN("read", ret, read(connfd, &retval, sizeof(int)), "inviando dati al server", "");
	if(delay) msleep(ms_delay);
	return retval;
}

int lockFile(const char* pathname) {
	int ret, retval;
	request_t * rts;
	rts = initRequest(LOCK_FILE, pathname, 0);
	if(rts == NULL) return -1;
	SYSCALL_RETURN("write", ret, writen(connfd, rts, sizeof(request_t)), "inviando dati al server", "");
	SYSCALL_RETURN("read", ret, read(connfd, &retval, sizeof(int)), "inviando dati al server", "");
	if(delay) msleep(ms_delay);
	return retval;
}

int removeFile(const char* pathname) {
	int ret, retval;
	request_t * rts;
	rts = initRequest(REMOVE_FILE, pathname, 0);
	if(rts == NULL) return -1;
	SYSCALL_RETURN("write", ret, writen(connfd, rts, sizeof(request_t)), "inviando dati al server", "");
	SYSCALL_RETURN("read", ret, read(connfd, &retval, sizeof(int)), "inviando dati al server", "");
	if(delay) msleep(ms_delay);
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
	//printf("[CLIENT] Saved file %s\n", filepath);
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

//richiesta di scrivere in append al file pathname i size byte contenuti dal buffer
//l'operazione e' garantita atomica dal server
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
}