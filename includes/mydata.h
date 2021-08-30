/* NB: contiene sia strutture dati condivise tra server e client che 
   valori esclusivi tra i due; per semplicita' ho deciso comunque di tenerle
   in un unico header */
#if !defined(MY_DATA_H)
#define MY_DATA_H

#include <icl_hash.h>
#include <queue.h>

/* flag usati per l'apertura di file nello storage */
#define O_CREATE 1
#define O_LOCK 2

/* lista di variabili globali inizializzate dal manager
   thread e utilizzate dai worker, dove in caso di modifica
   viene utilizzata la mutua esclusione */

/* variabili di terminazione */
extern volatile int termina;
extern volatile int hangup;
extern volatile int clients;
/* variabili di stato dello storage */
extern long MAX_CAP;
extern long CUR_CAP;
extern long MAX_FIL;
extern long CUR_FIL;
extern long max_saved_files;
extern long max_reached_memory;
extern long num_capacity_miss;
extern long num_read;
extern long num_write; 
extern unsigned long all_read;
extern unsigned long all_write;
extern pthread_mutex_t storemtx;
/* indirizzo coda di rimpiazzamento file FIFO/LRU */
extern queue_t * replace_queue;
/* indirizzo tabella hash contente i file del server */
extern icl_hash_t * table;
/* indirizzo pipe utilizzata dai workers per comunicare la fine lavoro */
extern int req_pipe;
/* descrittore del file di log e mutex associata per la scrittura */
extern FILE *logfd;
extern pthread_mutex_t logmtx;
/* variabile abilitazione scritture lato client */ 
extern int quiet;
extern int delay;
extern long ms_delay;

/* stuct usata all'avvio del server per salvare i valori di configurazione */
typedef struct {
	char*	sock_name;       /* indirizzo del socket */
	char*	log_name;        /* indirizzo del file di log */
	long 	num_workers;     /* numero di thread worker da creare */
	long	mem_files;       /* numero massimo di file salvabili in memoria */ 
	long 	mem_bytes;       /* numero massimo di bytes salvabili in memoria */
} config_t;

/* struct contente una richiesta del client */
typedef struct {
    int     type;               /* tipo di richiesta*/
    int     arg;                /* facoltativo argomento numerico, flag */
    pid_t   cid;                /* pid del client usato come id */
    char    filepath[PATH_MAX]; /* path assoluto di un file oggetto della richiesta */
} request_t;

/* struct che rappresenta un file salvato in memoria */
typedef struct {
    void*           contenuto;  /* generico contenuto del file */
    char*           file_name;  /* nome base del file */
    size_t          file_size;  /* grandezza del file (in bytes) */  
    int             open_flag;  /* flag che indica se il file e' aperto */
    int             lock_flag;  /* flag che indica se il file e' locked */
    int             locked_by;  /* id del client che lockato il file */
    pthread_cond_t  cond;       /* notifica i client in attesa del file */
} file_t;

/* enumerazione per maggiore legibilita' delle operazioni 
   richieste dal client al server */
enum {
    OPEN_CONNECTION     = 0,
    CLOSE_CONNECTION    = 1,
    OPEN_FILE           = 2,
    WRITE_FILE          = 3,
    READ_FILE           = 4,
    READ_N_FILES        = 5,
    REMOVE_FILE         = 6,
    LOCK_FILE           = 7,
    UNLOCK_FILE         = 8,
    CLOSE_FILE          = 9
};

/* enumerazione per maggiore legibilita' dei codici di errore
   del server inviati al client */
enum {
    err_args_invalid    = -13,
    err_path_invalid    = -12,
    err_worker_busy     = -11,
    err_server_busy     = -10,
    err_memory_alloc    = -9,
    err_storage_fault   = -8,
    err_file_exist      = -7,
    err_file_notexist   = -6,
    err_file_locked     = -5,
    err_file_notlocked  = -4,
    err_file_notopen    = -3,
    err_file_toobig     = -2        
};

/* brevissima funzione di utility per la free su un file che
   non ha trovato spazio altrove */
static inline void freeFile(void * f){
    if(((file_t*)f)->contenuto) 
        free(((file_t*)f)->contenuto);
    free(((file_t*)f)->file_name);
    pthread_cond_destroy(&(((file_t*)f)->cond));
    free(f);
}

/* breve header che non ha trovato spazio altrove di una semplice
   lista usata dai worker alla chiusura della connessione con un client
   per sbloccare i file rimasti locked */
typedef struct node {
   int id;
   char ht_key[PATH_MAX];
   struct node *next;
} listnode_t;

pthread_mutex_t clist_mtx;

void cleanuplist_ins(int id, char * data);
int cleanuplist_del(char * data);
char * cleanuplist_getakey(int id);
void cleanuplist_free(void);
int cleanlist_isEmpty(void);

#endif /* MY_DATA_H */