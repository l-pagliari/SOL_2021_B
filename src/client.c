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
#include <signal.h>

#include <util.h>
#include <mydata.h>
#include <api.h>

/* variabili utilizzate per decidere quanto tempo
   aspettare tra un tentativo di connesione e un altro;
   ed un tempo massimo di attesa */
#if !defined(MAX_WAIT_TIME_SEC)
#define MAX_WAIT_TIME_SEC 4
#endif
#if !defined(RETRY_TIME_MS)
#define RETRY_TIME_MS 500
#endif
/* flag per la stampa su stdout delle operazioni */
int quiet = 1;	

/* prototipi */
static void usage(void);
static int makeDir(const char* dirname, char *path);

int main(int argc, char *argv[]) {
	
	if (argc == 1) {
		fprintf(stderr, "input errato, usare -h per aiuto\n");
		exit(EXIT_FAILURE);
    }
    /* dichiarazione delle variabili usate per interagire con l'API */
	int r;
    int connection_established = 0;
	char opt;
    char *sockname = NULL;
    char *token, *save, *token2;
    size_t fsize;
    void *memptr = NULL;
    long val;
    char *read_dir = NULL;
    char *write_dir = NULL;
    char rdpath[PATH_MAX];
    char wrpath[PATH_MAX];	
    
    /* comunico con un socket, maschero SIGPIPE per evitare interruzioni
       troppo brusche a seguito di chiusura connessione lato server */
    struct sigaction s;
    memset(&s,0,sizeof(s));    
    s.sa_handler=SIG_IGN;
    if((sigaction(SIGPIPE,&s,NULL)) == -1 ) { 
    	perror("sigaction"); 
    	exit(EXIT_FAILURE); 
    } 

    /* utilizzo getopt per il parsing della command line, le richieste vengono 
       eseguite nell'ordine in cui arrivano. Un alternativa e' usare sempre getopt
       per i comandi ma inserire le richieste in una coda per poi al termine del parsing
       estrare le richieste con un ordine di priorita'. Soluzione progettata ma non 
       implementata per mancanza di tempo */
	while((opt = getopt(argc, argv, ":hf:w:W:r:R:d:D:u:l:c:t:pa:")) != -1) {
    	switch(opt) {
			/* stampa modalita' d'uso */
			case 'h': 
				usage();
				exit(EXIT_SUCCESS);
				break;
			/* abilito le stampe su stout delle operazioni con un flag globale */	
			case 'p':
    			if(!quiet) {
    				fprintf(stderr, "input errato, usare -h per aiuto\n");
               		break;
    			}
    			quiet = 0;
    			break;	
    		/* provo a connettermi al socket, in caso di fallimento riprovo 
    		   ad intervalli regolari fino ad un tempo massimo */
			case 'f':
				if(connection_established == 1) {
					fprintf(stderr, "input errato, usare -h per aiuto\n");
					break;
				}
				if((sockname = malloc(BUFSIZE*sizeof(char))) == NULL ) {
					perror("malloc");
					exit(EXIT_FAILURE);
				}
				strncpy(sockname, optarg, BUFSIZE);

				struct timespec timeout;
				timeout.tv_sec = MAX_WAIT_TIME_SEC;
				int retry = RETRY_TIME_MS;
				
				r = openConnection(sockname, retry, timeout); 
				if(r == -1) 
					exit(EXIT_FAILURE);
				if(!quiet) printf("[CLIENT] Aperta connessione con il server \n");
				connection_established = 1;
				break;
			/* richiesta di scrivere un file specifico sul server; lo apro, lo scrivo 
			   in append e poi lo chiudo */
			case 'W':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita, usare -h per aiuto\n");
					exit(EXIT_FAILURE);
				}
				token = strtok_r(optarg, ",", &save);
				do{
					r = openFile(token, O_CREATE|O_LOCK);
					if(r == 0) {
						writeFile(token, write_dir);
						closeFile(token);
					}
					token = strtok_r(NULL, ",", &save);
				}while(token!=NULL);
				break;
			/* richiesta di leggere un file, se specificato lo salvo una volta letto */
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
						if(!quiet) fprintf(stdout, "[CLIENT] Letto il file %s (%ld MB)\n", token, fsize/1024/1024);
						if(read_dir) saveFile(read_dir, token, memptr, fsize);
						if(memptr) free(memptr);
						closeFile(token);
					}
					token = strtok_r(NULL, ",", &save);
				}while(token!=NULL);	
				break;
			/* abilito il salvataggio locale dei file letti dal server nella directory input */
			case 'd':
				/*salvo in rdpath il path assoluto della directory passata in input
				  se non esiste la creo */
				r = makeDir(optarg, rdpath);
				if(r == -1) {
					fprintf(stderr, "input errato, usare -h per aiuto\n");
					break;
				}
				read_dir = rdpath;
				if(!quiet) fprintf(stdout, "[CLIENT] File letti verranno salvati nella cartella %s\n", read_dir);
				break;
			/* abilito il salvataggio locale dei file espulsi dal server per capacity miss */
			case 'D':
				/*salvo in wrpath il path assoluto della directory passata in input
				  se non esiste la creo */
				r = makeDir(optarg, wrpath);
				if(r == -1) {
					fprintf(stderr, "input errato, usare -h per aiuto\n");
					break;
				}
				write_dir = wrpath;
				if(!quiet) fprintf(stdout, "[CLIENT] File espulsi verranno salvati nella cartella %s\n", write_dir);
				break;
			/* scrivo fino a n file contenuti ricorsivamente nella directory input */
			case 'w':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				//input della forma -w dirname,[n=0]
				token = strtok_r(optarg, ",", &save);
				token2 = strtok_r(NULL, ",", &save);
				if(token2 == NULL) writeDirectory(token, 0, write_dir);
				else if((strlen(token2) > 2) && (isNumber(token2+2, &val) == 0) && (val>=0)) {
					writeDirectory(token, val, write_dir);
				}
				else {
					fprintf(stderr, "input errato, usare -h per aiuto\n");
               		break;
               	}
				break;
			/* leggo fino a n file a caso contenuti nel server e li salvo in read_dir */
			case 'R':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				//input nella forma n=7
				if(read_dir != NULL && isNumber(optarg+2, &val) == 0) {
					readNFiles(val, read_dir);
				}
				else {
					fprintf(stderr, "input errato, usare -h per aiuto\n");
					break;
               	}	
				break;
			/* richiesta di lock su file */
			case 'l':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				token = strtok_r(optarg, ",", &save);
				do{
					r = lockFile(token);
					if(!quiet && r==0) printf("[CLIENT] Lockato il file %s\n", token);
					token = strtok_r(NULL, ",", &save);
				}while(token!=NULL);
				break;
			/* richiesta di unlock su file */
			case 'u':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				token = strtok_r(optarg, ",", &save);
				do{
					r = unlockFile(token);
					if(!quiet && r==0) printf("[CLIENT] Unlockato il file %s\n", token);
					token = strtok_r(NULL, ",", &save);
				}while(token!=NULL);
				break;
			/* richiesta di eliminare un file presente sul server */
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
			/* abilita il ritardo tra le richieste di un numero di millisecondi passato in input */
			case 't':
				//l'argomento e' il numero di millisecondi
				if(isNumber(optarg, &val) == 0 && val >= 0) {
					setDelay(val);
					if(!quiet) printf("[CLIENT] Abilitato il ritardo tra le richieste di %ldmsec\n", val);
				}
				else fprintf(stderr, "input errato, usare -h per aiuto\n");
				break;
			/* getopt error handling manuale per gestire gli argomenti opzionali */
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
					if( read_dir == NULL ) {
						fprintf(stderr, "input errato, usare -h per aiuto\n");
						break;
					}
					r = readNFiles(0, read_dir);
				}
				else {
					fprintf(stderr,"argomento assente per %c\n", (char)optopt);
					break;
				}
				break;
		}		
	}
	if(connection_established == 1) {
		//se ci sono errori vengono stampati dall'api, ma la connessione viene chiusa in ogni caso
   		closeConnection(sockname);
   		if(!quiet) printf("[CLIENT] Chiusa la connessione con il server\n");
	}
    if(sockname) free(sockname);
    return 0;
}

/* stampa su stdout un riassunto delle operazioni possibili */
static void usage(void) {
	printf("***LISTA DELLE OPZIONI ACCETTATE***\n\n"
		"-f filename: \nspecifica il socket AF_UNIX a cui connettersi;\n\n"
		"-w dirname[,n=0]: \ninvia al server i file nella cartella 'dirname'. Se contiene altre directory "
		"vengono visitate ricorsivamente fino a quando non si leggono n files. Se n non e' specificato o "
		"n=0 non c'e' limite superiore ai file inviati al server;\n\n"
		"-W file1[,file2]: \nlista di nome di file da scrivere nel server separati da ',';\n\n"
		"-D dirname: directory dove scrivere i file espulsi dal server in seguito di capacity misses per "
		"servire le scritture -w e -W; -D deve essere usata congiuntamente a -w o -W altrimenti genera errore;\n\n"
		"-r file1[,file2]: \nlista di nomi di file da leggere dal server separati da ',';\n\n"
		"-R [n=0]: \ntale opzione permette di leggere 'n' file qualsiasi attualmente memorizzati sul server "
		"e salvarli nella directory specificata con -d; "
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
/* funzione di utility usata per assicurarsi che le directory passate in input esistono
   e siano utilizzabili; se non esiste viene creata e restituisce in path il path assoluto
   della directory passata in input */
static int makeDir(const char* dirname, char *path){
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