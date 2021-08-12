#if !defined(MY_DATA_H)
#define MY_DATA_H

#include <signal.h>
#include <icl_hash.h>

#define O_CREATE 1
#define O_LOCK 2

extern long MAX_CAP;
extern long CUR_CAP;
extern long MAX_FIL;
extern long CUR_FIL;

//usata all'avvio del server per salvare i valori di configurazione
typedef struct {
	char*	sock_name;
	char*	log_name;
	long 	num_workers;
	long	mem_files;
	long 	mem_bytes;
} config_t;

//usata per passare gli argomenti al signal handler
typedef struct {
    sigset_t     *set;           
    int           signal_pipe;  
} sigHandler_t;

//usata per passare i dati utili dal manager thread al worker thread
typedef struct {
    long        clientfd;
    int         pipe;
    icl_hash_t* hash_table;
} workerArg_t;


typedef struct {
    int     type;
    int     arg;
    pid_t   cid;
    char    filepath[PATH_MAX];
    //char    dirpath[PATH_MAX]; non sono sicuro serva
} request_t;

typedef struct {
    void*           contenuto;
    char*           file_name;
    size_t          file_size;
    int             lock_flag;
    pid_t           locked_by;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
} file_t;

//enumerazioni usate per maggiore legibilita' del codice
//nello scambio di informazioni tra client e server
enum {
    OPEN_CONNECTION     = 0,
    CLOSE_CONNECTION    = 1,
    OPEN_FILE           = 2,
    WRITE_FILE          = 3,
    READ_FILE           = 4
    //READ_N              = 4,
    //REMOVE_FILE         = 5,
    //LOCK_FILE           = 6,
    //UNLOCK_FILE         = 7
};

#endif /* MY_DATA_H */