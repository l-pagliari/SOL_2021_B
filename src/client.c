//#define _POSIX_C_SOURCE 2001112L
#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

#include <util.h>
#include <mydata.h>
#include <api.h>

#if !defined(MAX_WAIT_TIME_SEC)
#define MAX_WAIT_TIME_SEC 5
#endif

#if !defined(RETRY_TIME_MS)
#define RETRY_TIME_MS 500
#endif

int quiet = 1;	

void usage(void);
int makeDir(const char* dirname, char *path);

int main(int argc, char *argv[]) {
	
	if (argc == 1) {
		fprintf(stderr, "input errato, usare -h per aiuto\n");
		exit(EXIT_FAILURE);
    }
	int r;
    int connection_established = 0;

    char opt;
    char *sockname = NULL;
    char *token, *save, *token2;
    
    size_t fsize;
    void *memptr = NULL;
	//usati da strtol in R
    char *endptr;
    long val;
    
    char *append_buf;
    size_t append_len;


	char *read_dir = NULL;
    char *write_dir = NULL;
    char rdpath[PATH_MAX];
    char wrpath[PATH_MAX];	

    while((opt = getopt(argc, argv, ":hf:w:W:r:R:d:D:u:l:c:t:pa:")) != -1) {
    	switch(opt) {
			
			case 'h': 
				usage();
				exit(EXIT_SUCCESS);
				break;

			case 'p':
    			if(!quiet) {
    				fprintf(stderr, "input errato, usare -h per aiuto\n");
               		exit(EXIT_FAILURE);
    			}
    			quiet = 0;
    			break;	

			case 'f':
				if(connection_established == 1) {
					fprintf(stderr, "input errato, usare -h per aiuto\n");
					exit(EXIT_FAILURE);
				}
				if((sockname = malloc(BUFSIZE*sizeof(char))) == NULL ) {
					perror("malloc");
					exit(EXIT_FAILURE);
				}
				strncpy(sockname, optarg, BUFSIZE);

				struct timespec timeout;
				timeout.tv_sec = MAX_WAIT_TIME_SEC;
				int retry = RETRY_TIME_MS;
				
				if((r = openConnection(sockname, retry, timeout)) == -1) { //API
					perror("openConnection");
					exit(EXIT_FAILURE);
				}
				if(!quiet) printf("[CLIENT] Aperta connessione con il server \n");
				connection_established = 1;
				break;
			
			case 'W':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita, usare -h per aiuto\n");
					exit(EXIT_FAILURE);
				}
				token = strtok_r(optarg, ",", &save);
				do{
					r = openFile(token, O_CREATE|O_LOCK);
					if(r == 0) {
						r = writeFile(token, write_dir);
						if(r == -1) printf("[CLIENT] Errore nella scrittura del file\n");
						//else if(!quiet) printf("[CLIENT] Scritto il file %s sul server\n", token);
						r = closeFile(token);
						if(r == -1) printf("[CLIENT] Errore nella chiusura del file\n");
					}
					token = strtok_r(NULL, ",", &save);
				}while(token!=NULL);
				break;

			case 'r':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita, usare -h per aiuto\n");
					exit(EXIT_FAILURE);
				}
				token = strtok_r(optarg, ",", &save);
				do{
					r = openFile(token, 0);
					if(r == 0)
						r = readFile(token, &memptr, &fsize);
					if(r == 0) {
						if(!quiet) printf("[CLIENT] Letto il file %s (%ld Bytes)\n", token, fsize);
						if(read_dir) r = saveFile(read_dir, token, memptr, fsize);
						/*if(r == 0) {
							if(!quiet) printf("[CLIENT] Salvato il file %s nella directory %s\n", token, read_dir);
						}*/
						r = closeFile(token);
						if(r == -1) printf("[CLIENT] Errore nella chiusura del file\n");
					}
					if(memptr) free(memptr);
					token = strtok_r(NULL, ",", &save);
				}while(token!=NULL);	
				break;

			//directory dove salvare localmente i file letti dal server
			case 'd':
				/*salvo in rdpath il path assoluto della directory passata in input
				  se non esiste la creo */
				r = makeDir(optarg, rdpath);
				if(r == -1) {
					fprintf(stderr, "input errato, usare -h per aiuto\n");
					exit(EXIT_FAILURE);
				}
				read_dir = rdpath;
				if(!quiet) fprintf(stdout, "[CLIENT] File letti verranno salvati nella cartella %s\n", read_dir);
				break;

			//directory dove salvare localmente i file espulsi dal server per capacity miss a seguto di una write
			case 'D':
				/*salvo in wrpath il path assoluto della directory passata in input
				  se non esiste la creo */
				r = makeDir(optarg, wrpath);
				if(r == -1) {
					fprintf(stderr, "input errato, usare -h per aiuto\n");
					exit(EXIT_FAILURE);
				}
				write_dir = wrpath;
				if(!quiet) fprintf(stdout, "[CLIENT] File espulsi verranno salvati nella cartella %s\n", write_dir);
				break;

			case 'w':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				//input della forma -w dirname,[n=0]
				token = strtok_r(optarg, ",", &save);
				token2 = strtok_r(NULL, ",", &save);
				if(token2 == NULL) writeDirectory(token, 0, write_dir);
				else if(strlen(token2) > 2) {
					//estraggo il valore numerico usando strtol
					errno = 0;
					val = strtol(optarg+2, &endptr, 10);
					if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0)) {
               			perror("strtol");
               			exit(EXIT_FAILURE);
           			}
					if (endptr == optarg+2 || val < 0) {
               			fprintf(stderr, "input errato, usare -h per aiuto\n");
               			exit(EXIT_FAILURE);
           			}
					writeDirectory(token, (int)val, write_dir);
				}
				else {
					fprintf(stderr, "input errato, usare -h per aiuto\n");
               		exit(EXIT_FAILURE);
               	}
				break;

			














			case 'R':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				//input nella forma n=7, faccio un controllo sull'input utilizzando strtol
				errno = 0;
				val = strtol(optarg+2, &endptr, 10);
				if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0)) {
               		perror("strtol");
               		exit(EXIT_FAILURE);
           		}
				if (endptr == optarg+2 || val < 0) {
               		fprintf(stderr, "input errato, usare -h per aiuto\n");
               		exit(EXIT_FAILURE);
           		}
				//read_dir != NULL indica che vogliamo memorizzare i file letti
				//il cast per l'uso realistico non dovrebbe causare problemi, in caso si puo' usare atoi con un altro errchecking
				r = readNFiles((int)val, read_dir);
				if(!quiet && r > 0) {
					if(read_dir == NULL) printf("[CLIENT] Letti %d file diversi dal server\n", r);
					else printf("[CLIENT] Letti %d file diversi dal server e salvati nella directory %s\n", r, read_dir);
				}
				break;

			









			
			case 'u':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				token = strtok_r(optarg, ",", &save);
				do{
					r = unlockFile(token);
					if(!quiet && r==0) printf("[CLIENT] Lockato il file %s\n", token);
					token = strtok_r(NULL, ",", &save);
				}while(token!=NULL);
				break;

			case 'l':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				token = strtok_r(optarg, ",", &save);
				do{
					r = lockFile(token);
					if(!quiet && r==0) printf("[CLIENT] Unlockato il file %s\n", token);
					token = strtok_r(NULL, ",", &save);
				}while(token!=NULL);
				break;

			case 'c':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				token = strtok_r(optarg, ",", &save);
				do{
					r = removeFile(token);
					if(!quiet && r==0) printf("[CLIENT] Eliminato il file %s\n", token);
					token = strtok_r(NULL, ",", &save);
				}while(token!=NULL);
				break;

			case 't':
				//l'argomento e' il numero di millisecondi
				errno = 0;
				val = strtol(optarg, &endptr, 10);
				if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0)) {
               		perror("strtol");
               		exit(EXIT_FAILURE);
           		}
				if (endptr == optarg || val < 0) {
               		fprintf(stderr, "input errato, usare -h per aiuto\n");
               		exit(EXIT_FAILURE);
           		}
           		setDelay(val);
           		if(!quiet) printf("[CLIENT] Abilitato il ritardo tra le richieste di %ldmsec\n", val);
    			break;

    		//principalmente per il testing di append
    		//imput della forma -a filename,stringa
    		case 'a':
    			if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				token = strtok_r(optarg, ",", &save);
				token2 = strtok_r(NULL, ",", &save);
				if(token2 == NULL) {
					fprintf(stderr, "input errato, usare -h per aiuto\n");
               		exit(EXIT_FAILURE);
				}
				append_len = strlen(token2)+1;
				append_buf = malloc(append_len);
				if(append_buf == NULL) {
					perror("malloc");
					exit(EXIT_FAILURE);
				}
				strncpy(append_buf, token2, append_len);
				//prima dell'append faccio una openfile con O_LOCK
				r = openFile(token, O_LOCK);
				if(r != -1){
					r = appendToFile(token, append_buf, append_len, write_dir);
					if(r == -1) fprintf(stderr, "errore append\n");
				}
				break;


			//getopt error handling manuale per gestire gli argomenti opzionali
			case '?': 
				fprintf(stderr, "comando non riconosciuto: %c\n", (char)optopt);
				break; 
			case ':':
				//l'argomento di R e' opzionale, gestisco personalmente l'errore
				if(optopt == 'R') {
					if(connection_established == 0) {
						fprintf(stderr, "connesione non stabilita\n");
						exit(EXIT_FAILURE);
					}
					r = readNFiles(0, read_dir);
					if(!quiet && r > 0) {
						if(read_dir == NULL) printf("[CLIENT] Letti %d file diversi dal server\n", r);
						else printf("[CLIENT] Letti %d file diversi dal server e salvati nella directory %s\n", r, read_dir);
					}
				}
				else {
					fprintf(stderr,"argomento assente per %c\n", (char)optopt);
					exit(EXIT_FAILURE);
				}
				break;
		}		
	}
	if(connection_established == 1) {
   		if(closeConnection(sockname) == -1) //API
   			perror("closeConnection"); 
   		else if(!quiet) printf("[CLIENT] Chiusa la connessione con il server\n");
						
    }
    if(sockname) free(sockname);
    return 0;
}

void usage(void) {
	printf("***LISTA DELLE OPZIONI ACCETTATE***\n\n"
		"-f filename: \nspecifica il socket AF_UNIX a cui connettersi;\n\n"
		"-w dirname[,n=0]: \ninvia al server i file nella cartella 'dirname'. Se contiene altre directory "
		"vengono visitate ricorsivamente fino a quando non si leggono n files. Se n non e' specificato o "
		"n=0 non c'e' limite superiore ai file inviati al server;\n\n"
		"-W file1[,file2]: \nlista di nome di file da scrivere nel server separati da ',';\n\n"
		"-D dirname: directory dove scrivere i file espulsi dal server in seguito di capacity misses per "
		"servire le scritture -w e -W; -D deve essere usata congiuntamente a -w o -W altrimenti genera errore;\n\n"
		"-r file1[,file2]: \nlista di nomi di file da leggere dal server separati da ',';\n\n"
		"-R [n=0]: \ntale opzione permette di leggere 'n' file qualsiasi attualmente memorizzati sul server; "
		"se n=0 oppure e' omessa allora vengono letti tutti i file presenti sul server;\n\n"
		"-d dirname: \ncartella in memoria secondaria dove scrivere tutti i file letti dal server con l'opzione "
		"-r o -R; tale opzione va usata congiuntamente a quest'ultime altrimenti viene generato un errore;\n\n"
		"-t time: \ntempo in millisecondi che intercorre tra l'invio di due richieste successive al server;\n\n"
		"-l file1[,file2]: \nlista di nomi di file su cui acquisire la mutua esclusione;\n\n"
		"-u file1[,file2]: \nlista di nomi di file su cui rilasciare la mutua esclusione;\n\n"
		"-c file1[,file2]: \nlista di file da rimuovere dal server se presenti;\n\n"
		"-p: \nabilita le stampe sullo standard output per ogni operazione.\n\n"
		"Gli argomenti possono essere ripetuti piu' volte (ad eccezione di '-f', '-h', '-p').\n");
}

int makeDir(const char* dirname, char *path){
	//provo a creare la directory, se gia' esiste il file controllo che sia una directory
	struct stat statbuf;
	int unused, r;
	r = mkdir(dirname, 0777);
	if(r == -1 && errno != EEXIST) {
		return -1;
	}
	if(r == -1) {
		SYSCALL_RETURN(stat, unused, stat(dirname, &statbuf), "facendo la stat di %s\n", dirname);
		if(!S_ISDIR(statbuf.st_mode)) {
			return -1;
		}
	}
	//scrivo in path il path assoluto della directory
	if(realpath(dirname, path) == NULL){
		fprintf(stderr, "Errore scrittura del path assoluto della directory %s\n", dirname);
		return -1;
	}
	return 0;
}