#define _POSIX_C_SOURCE 2001112L
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h> 
#include <sys/uio.h>
#include <sys/un.h>

#include <util.h>
#include <mydata.h>
#include <worker.h>
#include <sconfig.h>
#include <threadpool.h>	
#include <icl_hash.h>

#ifndef CONFIG_PATH
#define CONFIG_PATH "misc/default_config.txt"
#endif

//variabili globali di terminazione
volatile int termina = 0;
volatile int hangup = 0;
volatile int clients = 0;

//indirizzi e descrittori inizializzati dal
//server e utilizzati dai worker thread
icl_hash_t * table = NULL;
int req_pipe = 0;

//variabili globali inizalizzate dal thread master e utilizzate dai
//worker per conoscere ed aggiornare lo stato dello storage
long MAX_CAP = 0;
long CUR_CAP = 0;
long MAX_FIL = 0;
long CUR_FIL = 0;
long max_saved_files = 0;
long max_reached_memory = 0;
long num_capacity_miss = 0;
pthread_mutex_t storemtx = PTHREAD_MUTEX_INITIALIZER;
int max_clients = 0;

//usata per il timestamp nel file di log
char timestr[11];
FILE *logfd = NULL;
pthread_mutex_t logmtx = PTHREAD_MUTEX_INITIALIZER;

//usata per passare gli argomenti al signal handler
typedef struct {
    sigset_t     *set;           
    int           signal_pipe;  
} sigHandler_t;

/* riceve in input il set di segnali e l'indirizzo di scrittura di una pipe
** letta dalla select del server; alla ricezione di un segnale lo scrive 
** nella pipe e termina
*/
static void *sigHandler(void *arg) {
	sigset_t *set = ((sigHandler_t*)arg)->set;
   int fd_pipe = ((sigHandler_t*)arg)->signal_pipe;
	for(;;) {
		int sig;
		int r = sigwait(set, &sig);
		if(r != 0) {
	    	errno = r;
	    	perror("FATAL ERROR 'sigwait'");
	    	return NULL;
		}
		switch(sig) {
			case SIGINT:
			case SIGTERM:
			case SIGQUIT:
			case SIGHUP:
				if(writen(fd_pipe,&sig,sizeof(int))==-1){ 
                    perror("write signal handler");
                    return NULL;
                } 
	            return NULL;
			default:  ; 
		}
    }
    return NULL;	   
}
void busy_handler(long fd, int t);

int main(int argc, char* argv[]) {

	/*configuro il server usando la funzione contenuta in sconfig.c per maggiore leggibilita',
	**essendo essenziale all'avvio del server, in caso di errore il programma termina immediatamente
	**ed il messaggio di errore e' gestito dalla funzione stessa */
	config_t * config;
	if(argc == 2)
		config = read_config(argv[1]);
	else
		config = read_config(CONFIG_PATH);

	/* BLOCCO GESTIONE SEGNALI	*/
	sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT); 
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGTERM); //non strettamente richiesto dalla specifica
    sigaddset(&mask, SIGHUP); 
    
    if(pthread_sigmask(SIG_BLOCK, &mask,NULL) != 0) { 
    	fprintf(stderr, "errore sigmask\n");
    	exit(EXIT_FAILURE); 
    }
	struct sigaction s;
    memset(&s,0,sizeof(s));    
    s.sa_handler=SIG_IGN;
    if((sigaction(SIGPIPE,&s,NULL)) == -1 ) { 
    	perror("sigaction"); 
    	exit(EXIT_FAILURE); 
    } 
    //creo la pipe per la comunicazione sighandler-server
    int signal_pipe[2];
    if(pipe(signal_pipe)==-1) { 
    	perror("signal pipe");
    	exit(EXIT_FAILURE);
    }
    //genero il thread signal handler e gli passo come argomenti maschera segnali e pipe
    pthread_t sighandler_thread;
    sigHandler_t handlerArgs = { &mask, signal_pipe[1] };
   	if(pthread_create(&sighandler_thread, NULL, sigHandler, &handlerArgs) != 0) { 
   		fprintf(stderr, "errore creazione signal handler thread\n");
   		exit(EXIT_FAILURE);
   	}
	/*	FINE BLOCCO GESTIONE SEGNALI */

	/* BLOCCO SOCKET E STRUTTURE DATI */
	int listenfd;
    if((listenfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) { 
    	perror("socket");
    	exit(EXIT_FAILURE); 
    }
	struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;    
    strncpy(serv_addr.sun_path, config->sock_name, strlen(config->sock_name)+1);
	if(bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr)) == -1) { 
    	perror("bind"); 
    	exit(EXIT_FAILURE);
    }
    if(listen(listenfd, MAXBACKLOG) == -1) {
    	perror("listen");
    	exit(EXIT_FAILURE);
    }
    /* creo la pipe per la comunicazione manager-worker */
	int request_pipe[2];
    if(pipe(request_pipe)==-1) {
    	perror("request_pipe");
    	unlink(config->sock_name);
    	free_config(config);
    	exit(EXIT_FAILURE);
    }
    req_pipe = request_pipe[1];
	/*creo il threadpool per gestire le richieste utilizzo il modello 1 thread 1 richiesta, 
      nessun task pendente se tutti i thread sono occupati */
    threadpool_t *pool = NULL;
	pool = createThreadPool(config->num_workers, 20); 
    if(!pool) {
    	fprintf(stderr, "errore nella creazione del thread pool\n");
    	unlink(config->sock_name);
		free_config(config);
    	exit(EXIT_FAILURE);
    }
    /* creo la tabella hash utilizzata come data structure per lo storage server, il numero
       massimo di file(entry) che puo' ospitare e' fisso quindi non occorre fare resize in seguito */
    table = icl_hash_create(config->mem_files, NULL, NULL);
    if (!table) {
    	fprintf(stderr, "errore nella creazione della tabella hash\n");
    	destroyThreadPool(pool, 1);
    	unlink(config->sock_name);
    	free_config(config);
    	exit(EXIT_FAILURE);
	}
	/* inizializzo una coda che viene usata dai thread worker per la gestione del capacity miss;
	   mantiene l'ordine dei file inseriti necessario per la politica di rimpiazzamento */
	replace_queue = init_queue();
	if(!replace_queue) {
    	fprintf(stderr, "errore nella creazione della replace queue\n");
    	destroyThreadPool(pool, 0); 
    	icl_hash_destroy(table, &free, &freeFile);
    	unlink(config->sock_name);
    	free_config(config);
    	exit(EXIT_FAILURE);
    }
   logfd = fopen(config->log_name, "w+");
   if(!logfd) {
   	fprintf(stderr, "errore nell'apertura del file di log\n");
    	destroyThreadPool(pool, 0); 
    	icl_hash_destroy(table, &free, &freeFile);
    	unlink(config->sock_name);
    	free_config(config);
    	exit(EXIT_FAILURE);
   }
	/* FINE BLOCCO SOCKET+STRUTTURE DATI */

	/* BLOCCO SELECT */
	char buf[BUFSIZE];
	int n;
	//int errn;
	fd_set set, tmpset;
   FD_ZERO(&set);
   FD_ZERO(&tmpset);
	FD_SET(listenfd, &set);        
   FD_SET(signal_pipe[0], &set); 
   FD_SET(request_pipe[0], &set); 
    
  	int fdmax, oldmax;
   fdmax = (listenfd > signal_pipe[0]) ? listenfd : signal_pipe[0];
   fdmax = (fdmax > request_pipe[0]) ? fdmax : request_pipe[0];
	oldmax = fdmax;
	while(!termina) {		
		tmpset = set;
		if (select(fdmax+1, &tmpset, NULL, NULL, NULL) == -1) {
			perror("select");
			break;
		}
		for(int i=0; i <= fdmax; i++) { 
			/* GESTIONE SELECT:

				1) NUOVA CONNESSIONE(i == listenfd)
					-> aggiungo il nuovo descrittore al set
					-> incremento il counter dei client connessi
					
				2) FINE TASK WORKER(i == request_pipe[0])
					-> una richiesta del client e' stata gestita
					-> leggo dalla pipe il file descriptor 
					-> lo aggiungo nuovamente al set

				3) RICEZIONE SEGNALE(i == signal_pipe[0])
					-> leggo il segnale ricevuto e decido il tipo di chiusura
					-> setto variabile di terminazione

				4) NUOVA RICHIESTA DA UN CLIENT CONNESSO else
					-> setto gli argomenti da passare
					-> rimuovo il descrittore di connessione dal set
					-> passo gli argomenti e la thread function ad addToThreadPool */

			if (FD_ISSET(i, &tmpset)) {
				long connfd;
				// e' una nuova richiesta di connessione
				if (i == listenfd) {  
		  			if((connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) == -1) {
		  				perror("accept");
		  				termina = 1;
		  				break;
		  			}
		  			/* se ho ricevuto il segnale SIGHUP rifiuto tutte le connessioni attendo notifica
		  			   di terminazione dal worker thread che chiude la connessione con l'ultimo server */
					if(hangup){
						printf("SERVER SHUTTING DOWN... unable to accept new connections\n");
					 	close(connfd);
					 	continue;
					}

					if(clients == config->num_workers) {
						//printf("SERVER BUSY... try again\n");
					 	busy_handler(connfd, err_server_busy); //chiude la connessione in maniera pulita
					 	continue;
					}
					
					clients++;
					if(clients>max_clients) max_clients = clients;
					FD_SET(connfd, &set);  
		  			if(connfd > fdmax) {
		  				oldmax = fdmax; 
		  				fdmax = connfd; 
		  			} 
		  			continue;
				}
				// la richiesta di un client precedente e' stata servita
				if (i == request_pipe[0]) {
					if((n = read(request_pipe[0], buf, BUFSIZE)) == -1) { 
						perror("read request pipe");
						termina = 1;
						break;
					}
					connfd = *((long*) buf);
					FD_SET(connfd, &set);  
		  			if(connfd > fdmax) {
		  				oldmax = fdmax;
		  				fdmax = connfd;
		  			} 
		  			continue;
		  		}
				// il signal handler ha letto un segnale, lo leggo
				if (i == signal_pipe[0]) {
					int sig;
                    if(readn(signal_pipe[0],&sig,sizeof(int))==-1){
                        perror("FATAL ERROR reading signal pipe");
                        exit(EXIT_FAILURE);
                    }
                    /* due tipi di chiusura del server:
                       hangup	-> 	non accetto piu' nuove connessioni, quando tutti i
                       			 	client al momento connessi hanno finito, termino;
                       termina 	->	forzo la chiusura di tutti worker attivi e termino 
                       				il prima possibile */
                    if(sig==SIGHUP) {
                    	LOCK(&logmtx);
                    	fprintf(logfd, "%s[LOG] Recieved SIGHUP, closing upcoming connections\n",tStamp(timestr));
                    	UNLOCK(&logmtx);
                    	hangup = 1;
                    	if(clients == 0) termina = 1;
          			}else 
						termina = 1;
		    		break;
				}
				// nuova richiesta da un client connesso
				connfd = i;
				if(fdmax == connfd) fdmax = oldmax;
				FD_CLR(connfd, &set);
				//notifico il pool che c'e' un nuovo task
				n = addToThreadPool(pool, workerF, &connfd);
		    	if (n==0) 
		    		continue; 
		    	if (n<0) 
					fprintf(stderr, "FATAL ERROR, adding to the thread pool\n");
		  		else {
					//fprintf(stderr, "SERVER TOO BUSY\n");
					busy_handler(connfd, err_worker_busy);
					//per il momento la chiudo poi vedo se posso fare di meglio
				}
		    }
			continue;
		}//end for
	}//end while
    /* FINE BLOCCO SELECT */

    /* BLOCCO STAMPA CHIUSURA SERVER E CLEANUP */
	printf("\n[SERVER CLOSING] max clients connected:\t%d\n"
				"[SERVER CLOSING] max files saved:\t%ld\n"
				"[SERVER CLOSING] max MB occupied:\t%ld\n"
				"[SERVER CLOSING] capacity misses:\t%ld\n"
			   "[SERVER CLOSING] files in storage:\t%ld / %ld\n"
				"[SERVER CLOSING] storage capacity:\t%ld / %ld\n"
				"[SERVER CLOSING] list of files currently in storage:\n",
			max_clients, max_saved_files, max_reached_memory/1024/1024, num_capacity_miss, 
			CUR_FIL ,MAX_FIL, CUR_CAP/1024/1024, MAX_CAP/1024/1024);
	icl_hash_dump(stdout, table);
	
	LOCK(&logmtx);
	fprintf(logfd, "%s[LOG] MAX CLIENTS: %d\n",tStamp(timestr), max_clients);
	fprintf(logfd, "%s[LOG] MAX CAPACITY: %ld\n",tStamp(timestr), max_reached_memory);
	fprintf(logfd, "%s[LOG] MAX FILES: %ld\n",tStamp(timestr), max_saved_files);
	UNLOCK(&logmtx);

	destroyThreadPool(pool, 0); 
   icl_hash_destroy(table, &free, &freeFile);
   unlink(config->sock_name);
   free_config(config);
   freeQueue(replace_queue);
   cleanuplist_free();
   fclose(logfd);
   pthread_join(sighandler_thread, NULL);
   return 0;    
}					


void busy_handler(long fd, int t) {

	request_t * req;
	int errn = t;
	req = malloc(sizeof(request_t));
	if(req == NULL) {
		fprintf(stderr, "errore malloc request\n");
		return;
	}
	if(readn(fd, req, sizeof(request_t)) == -1) {
		fprintf(stderr, "errore lettura richiesta client\n");
		return;
	}				
	if(writen(fd, &errn, sizeof(int)) == -1) {
		perror("write");
		exit(EXIT_FAILURE);
	}
	close(fd);
	free(req);
}