#if !defined(API_H)
#define API_H

#include <time.h>

/*	@brief: apre una connessione AF_UNIX al socket sockname. In caso di fallimento
**	riprova ad intervalli regolari 'msec' fino ad un tempo massimo 'abstime'
** 	@returns: 0 in caso di successo, -1 fallimento 
*/
int openConnection(const char *sockname, int msec, const struct timespec abstime);

/*	@brief: chiude la connessione AF_UNIX associata al socket sockname
** 	@returns: 0 in caso di successo, -1 fallimento 
*/
int closeConnection(const char *sockname);

/*	@brief: richiesta di apertura o di creazione file; la semantica dipende dai flag passati
**	come argomento che possono essere O_CREATE per creare un nuovo file oppure O_LOCK per 
**	aprire il file in modalita locked; possono essere passati entrambi in OR
** 	@returns: 0 in caso di successo, -1 fallimento 
*/
int openFile(const char *pathname, int flags);

/*	@brief: legge tutto il contenuto del file dal server ritornando un puntatore ad un'area
**	allocata sullo heap nel parametro 'buf', mentre 'size' conterra' la dimensione del buffer
** 	@returns: 0 in caso di successo, -1 fallimento 
*/
int readFile(const char* pathname, void** buf, size_t* size);

/*	@brief: richiede al server la lettura di 'N' files qualsiasi da memorizzare nella directory
**	'dirname' lato client; se il server ha meno file disponibili, o se N<=0, li invia tutti
** 	@returns: numero file letti in caso di successo, -1 fallimento 
*/
int readNFiles(int N, const char* dirname);

/*	@brief: scrive tutto il file puntato da 'pathname' nel file server; la precedente operazione 
**	deve essere stata openFile(pathname, O_CREATE|O_LOCK); se dirname e' specificato gli eventuali
**	file espulsi dal server per fare spazio al nuovo vengono salvati in locale, altrimenti eliminati
** 	@returns: 0 in caso di successo, -1 fallimento 
*/
int writeFile(const char *pathname, const char *dirname);

/*	@brief: richiesta di scrivere in append al file 'pathname' i 'size' bytes contenuti
**	nel buffer 'buf'; se dirname e' specificato gli eventuali file espulsi dal server per 
**	fare spazio al nuovo vengono salvati in locale, altrimenti eliminati
** 	@returns: 0 in caso di successo, -1 fallimento 
*/
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

/*	@brief: setta il flag O_LOCK al file in caso di successo; se il file e' gia' locked 
**	l'operazione non viene completata fino a quando il flag lock precedente viene resettato
** 	@returns: 0 in caso di successo, -1 fallimento 
*/
int lockFile(const char* pathname);

/*	@brief: resetta il flag O_LOCK sul file 'pathname'; l'operazione ha successo solo se
**	l'owner della lock e' il client che ha richiesto l'operazione
** 	@returns: 0 in caso di successo, -1 fallimento 
*/
int unlockFile(const char* pathname);

/*	@brief: richiesta di chiusura del file 'pathname'; eventuali operazioni successive
**	falliscono
** 	@returns: 0 in caso di successo, -1 fallimento 
*/
int closeFile(const char* pathname);

/*	@brief: rimuove il file cancellandolo dal file storage server; l'operazione fallisce
**	se il file non e' in stato di locked da parte del client che ha richiesto l'operazione
** 	@returns: 0 in caso di successo, -1 fallimento 
*/
int removeFile(const char* pathname);

/*	@brief: salva nella directory 'dirname' i 'size' bytes contenuti nel buffer 'buf'; il
**	nome del nuovo file viene ricavato da 'pathname'
** 	@returns: 0 in caso di successo, -1 fallimento 
*/
int saveFile(const char* dirname, const char* pathname, void* buf, size_t size);

/*	@brief: effettua richieste di scrittura sul file server di ogni file trovato ricorsivamente
**	nella directory 'dirname', fino ad un massimo di 'max_files'; se il numero e' 0 o non specificato
**	tutti i file vengono inviati
** 	@returns: 0 in caso di successo, -1 fallimento 
*/
int writeDirectory(const char *dirname, int max_files, const char *writedir);

/*	@brief: imposta un ritardo di 'msec' tra le richieste al server
*/
void setDelay(long msec);

/*	@brief: utility per stampa degli errori; stampa la stringa 'str' e converte
**	il codice d'errore del server 'err' in una stringa
*/
void print_err(char * str, int err);

#endif /* API_H */