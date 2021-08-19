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
#define CONFIG_PATH "bin/config.txt"
#endif

volatile int termina = 0;
volatile int hangup = 0;

long MAX_CAP = 0;
long CUR_CAP = 0;
long MAX_FIL = 0;
long CUR_FIL = 0;

long max_saved_files = 0;
long max_reached_memory = 0;
long num_capacity_miss = 0;

int clients = 0;

//char timestr[11]; usata per il timestamp nel file di log

//usata per passare gli argomenti al signal handler
typedef struct {
    sigset_t     *set;           
    int           signal_pipe;  
} sigHandler_t;

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
				//printf("ricevuto segnale %s, esco\n", (sig==SIGINT) ? "SIGINT": ((sig==SIGTERM)?"SIGTERM":"SIGQUIT") );
				if(writen(fd_pipe,&sig,sizeof(int))==-1){ 
                    perror("write signal handler");
                    return NULL;
                }
	            close(fd_pipe);  
	            return NULL;
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


int main(int argc, char* argv[]) {

	/*configuro il server usando la funzione contenuta in sconfig.c per maggiore leggibilita',
	**essendo essenziale all'avvio del server, in caso di errore il programma termina immediatamente
	**ed il messaggio di errore e' gestito dalla funzione stessa */
	config_t * config;
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
    int signal_pipe[2];
    if(pipe(signal_pipe)==-1) { 
    	perror("signal pipe");
    	exit(EXIT_FAILURE);
    }
    //close(signal_pipe[1]);
    
    pthread_t sighandler_thread;
    sigHandler_t handlerArgs = { &mask, signal_pipe[1] };
   	if(pthread_create(&sighandler_thread, NULL, sigHandler, &handlerArgs) != 0) { 
   		fprintf(stderr, "errore creazione signal handler thread\n");
   		exit(EXIT_FAILURE);
   	}
	/*	FINE BLOCCO GESTIONE SEGNALI */

	/* BLOCCO SOCKET+STRUTTURE DATI */
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
	int request_pipe[2];
    if(pipe(request_pipe)==-1) {
    	perror("request_pipe");
    	exit(EXIT_FAILURE);
    }
    //close(request_pipe[1]);

    threadpool_t *pool = NULL;
	pool = createThreadPool(config->num_workers, config->num_workers); //il secondo valore potrebbe anche essere 0, not final
    if(!pool) {
    	fprintf(stderr, "errore nella creazione del thread pool\n");
    	exit(EXIT_FAILURE);
    }

    icl_hash_t *hash = NULL;
    hash = icl_hash_create(config->mem_files, NULL, NULL);
    if (!hash) {
    	fprintf(stderr, "errore nella creazione della tabella hash\n");
    	destroyThreadPool(pool, 0);
    	exit(EXIT_FAILURE);
	}

	replace_queue = init_queue();




	/* FINE BLOCCO SOCKET+STRUTTURE DATI */

	/* BLOCCO SELECT */
	char buf[BUFSIZE];
	int n;

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

    	//printf("TEST: sto per fare la select, cliens connessi: %d\n", clients);
 



		tmpset = set;
		if (select(fdmax+1, &tmpset, NULL, NULL, NULL) == -1) {
			perror("select");
			break;
		}
		for(int i=0; i <= fdmax; i++) { 
			/* GESTIONE SELECT:

				1) NUOVA CONNESSIONE i == listenfd
					-> aggiungo il nuovo descrittore al set
					-> se ho ricevuto SIGHUP chiudo subito la nuova richiesta

				2) SIGNAL PIPE i == signal_pipe[0]

					-> setto variabile di terminazione ed esco dal loop

				3) REQUEST PIPE i == request_pipe[0]

					-> leggo dalla pipe il file descriptor
					-> lo aggiungo al set

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
					
					if(hangup){
						printf("SERVER SHUTTING DOWN... unable to accept new connections\n"); //DA TESTARE
					 	close(connfd);
					 	continue;
					}

					clients++;

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
				// segnale di terminazione
				if (i == signal_pipe[0]) {
					int sig;
                    if(readn(signal_pipe[0],&sig,sizeof(int))==-1){
                        perror("FATAL ERROR reading signal pipe");
                        exit(EXIT_FAILURE);
                    }
                    if(sig==SIGHUP) {
                    	printf("[SERVER] Ricevuto SIGHUP (DA IMPLEMENTARE)\n");
                    	hangup = 1;
          			}else {
                   	 	//printf("[SERVER] ricevuto segnale %s, esco\n", (sig==SIGINT) ? "SIGINT": ((sig==SIGTERM)?"SIGTERM":"SIGQUIT") );
						termina = 1;
					}
		    		break;
				}
				// nuova connessione
				connfd = i;

		    	workerArg_t * wArg = NULL;
		    	wArg = malloc(sizeof(workerArg_t));
		    	if(!wArg){
		    		perror("malloc workerArg");
		    		termina = 1;
		    		break;
		    	}
		    	wArg->clientfd = connfd;
		    	wArg->pipe = request_pipe[1];
		    	wArg->hash_table = hash;

				if(fdmax == connfd) fdmax = oldmax;
				FD_CLR(connfd, &set);

				n = addToThreadPool(pool, workerF, (void*)wArg);
		    	if (n==0) 
		    		continue; 
		    	if (n<0) 
					fprintf(stderr, "FATAL ERROR, adding to the thread pool\n");
		  		else 
					fprintf(stderr, "SERVER TOO BUSY\n");
		    }
			continue;
		}//end for
	}//end while
    /* FINE BLOCCO SELECT */

	//fprintf(stdout, "\n[SERVER CLOSING] memory occupied: %ld\n"
	//	"[SERVER CLOSING] files in memory: %ld\n", CUR_CAP, CUR_FIL);

	printf("[SERVER CLOSING] max files saved: %ld / %ld\n"
			"[SERVER CLOSING] max bytes occupied: %ld / %ld\n"
			"[SERVER CLOSING] number of files replaced: %ld\n"
			"[SERVER CLOSING] list of files currently in storage:\n",
			max_saved_files, MAX_FIL, max_reached_memory, MAX_CAP, num_capacity_miss);
	icl_hash_dump(stdout, hash);
	


    destroyThreadPool(pool, 0); 
    icl_hash_destroy(hash, NULL, free);
    unlink(config->sock_name);
    pthread_join(sighandler_thread, NULL);
    return 0;    
}


/* TO DO LIST:
	
	-LOGGING [PARZIALE]
	-in seguto passare al worker l'indirizzo del log
	(forse il server quando torna il file descriptor mi ritorna insieme anche l'esito dell'operazione?)
	->pacchetto contenente tutte le informazioni da scrivere nel log
*/