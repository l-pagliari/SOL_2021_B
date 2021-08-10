#define _POSIX_C_SOURCE 2001112L
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>

#include <util.h>
#include <mydata.h>
#include <api.h>

#if !defined(MAX_WAIT_TIME_SEC)
#define MAX_WAIT_TIME_SEC 5
#endif

#if !defined(RETRY_TIME_MS)
#define RETRY_TIME_MS 5
#endif

void usage(void);

int main(int argc, char *argv[]) {
	
	if (argc == 1) {
		fprintf(stderr, "input errato, usare -h per aiuto\n");
		exit(EXIT_FAILURE);
    }
	int r;
    int connection_established = 0;
    char opt;
    char *sockname;
    char *token, *save;
    size_t fsize;
    void *memptr = NULL;
    
    while((opt = getopt(argc, argv, "hf:W:r:")) != -1) {
    	switch(opt) {
			
			case 'h': 
				usage();
				exit(EXIT_SUCCESS);
				break;

			case 'f':
				if(connection_established == 1) {
					fprintf(stderr, "input errato, usare -h per aiuto\n");
					exit(EXIT_FAILURE);
				}
				if((sockname = malloc(strlen(optarg)*sizeof(char))) == NULL ) {
					perror("malloc");
					exit(EXIT_FAILURE);
				}
				strncpy(sockname, optarg, strlen(optarg));
				struct timespec timeout;
				timeout.tv_sec = MAX_WAIT_TIME_SEC;
				int retry = RETRY_TIME_MS;
				if((r = openConnection(sockname, retry, timeout)) == -1) { //API
					perror("openConnection");
					exit(EXIT_FAILURE);
				}
				connection_established = 1;
				break;
			
			case 'W':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita, usare -h per aiuto\n");
					exit(EXIT_FAILURE);
				}
				token = strtok_r(optarg, ",", &save);
				do{
					r = openFile(token, O_CREATE);
					if(r == 0) writeFile(token, NULL);
					token = strtok_r(NULL, ",", &save);
				}while(token!=NULL);
				break;

			case 'r':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				token = strtok_r(optarg, ",", &save);
				do{
					r = openFile(token, 0);
					if(r == 0) {
						readFile(token, &memptr, &fsize);
						//if(saveread) saveFile(read_dir, token, memptr, fsize);
						if(memptr) free(memptr);
					}
					token = strtok_r(NULL, ",", &save);
				}while(token!=NULL);	
				break;
		}
	}
	if(connection_established == 1) {
   		if(closeConnection(sockname) == -1) //API
   			perror("closeConnection"); 	
    }
    return 0;
}

void usage(void) {
	printf("***LISTA DELLE OPZIONI ACCETTATE***\n\n"
		"-f filename: specifica il socket AF_UNIX a cui connettersi;\n\n"
		"-w dirname[,n=0]: invia al server i file nella cartella 'dirname'. Se contiene altre directory "
		"vengono visitate ricorsivamente fino a quando non si leggono n files. Se n non e' specificato o "
		"n=0 non c'e' limite superiore ai file inviati al server;\n\n"
		"-W file1[,file2]: lista di nome di file da scrivere nel server separati da ',';\n\n"
		"-D dirname: directory dove scrivere i file espulsi dal server in seguito di capacity misses per "
		"servire le scritture -w e -W; -D deve essere usata congiuntamente a -w o -W altrimenti genera errore;\n\n"
		"-r file1[,file2]: lista di nomi di file da leggere dal server separati da ',';\n\n"
		"-R [n=0]: tale opzione permette di leggere 'n' file qualsiasi attualmente memorizzati sul server; "
		"se n=0 oppure e' omessa allora vengono letti tutti i file presenti sul server;\n\n"
		"-d dirname: cartella in memoria secondaria dove scrivere tutti i file letti dal server con l'opzione "
		"-r o -R; tale opzione va usata congiuntamente a quest'ultime altrimenti viene generato un errore;\n\n"
		"-t time: tempo in millisecondi che intercorre tra l'invio di due richieste successive al server;\n\n"
		"-l file1[,file2]: lista di nomi di file su cui acquisire la mutua esclusione;\n\n"
		"-u file1[,file2]: lista di nomi di file su cui rilasciare la mutua esclusione;\n\n"
		"-c file1[,file2]: lista di file da rimuovere dal server se presenti;\n\n"
		"-p: abilita le stampe sullo standard output per ogni operazione.\n\n"
		"Gli argomenti possono essere ripetuti piu' volte (ad eccezione di '-f', '-h', '-p').\n");
}

/*
	int r;
    int connection_established = 0;
    char *sockname;
    char opt;
    size_t fsize;
    void *memptr = NULL;
    char *token, *token2, *save;
    char *read_dir = NULL;
    int saveread = 0;
	while((opt = getopt(argc, argv, ":hf:W:r:w:R:c:d:l:u:")) != -1) {
		switch(opt) {
			case 'h': 
				usage();
				exit(EXIT_SUCCESS);
				break;
			case 'f':
				if(connection_established == 1) {
					fprintf(stderr, "errore input\n");
					exit(EXIT_FAILURE);
				}
				if((sockname = malloc(strlen(optarg)*sizeof(char))) == NULL ) {
					perror("malloc");
					exit(EXIT_FAILURE);
				}
				strncpy(sockname, optarg, strlen(optarg));
				struct timespec timeout;
				timeout.tv_sec = 2;
				int retry = 400;
				if((r = openConnection(sockname, retry, timeout)) == -1) { //API
					perror("openConnection");
					exit(EXIT_FAILURE);
				}
				connection_established = 1;
				break;
			//TEST 1: scrivo un nuovo file nel server
			case 'W':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				token = strtok_r(optarg, ",", &save);
				do{
					r = openFile(token, O_CREATE);
					if(r == 0) writeFile(token, NULL);
					token = strtok_r(NULL, ",", &save);
				}while(token!=NULL);
				break;
			//TEST 2: leggo un file dal server
			//TEST 4: leggo un numero variabile di file dal server
			//TEST 7: se e' stata specificata la directory di scrittura salvo il file in locale
			case 'r':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				token = strtok_r(optarg, ",", &save);
				do{
					r = openFile(token, 0);
					if(r == 0) {
						readFile(token, &memptr, &fsize);
						if(saveread) saveFile(read_dir, token, memptr, fsize);
						if(memptr) free(memptr);
					}
					token = strtok_r(NULL, ",", &save);
				}while(token!=NULL);	
				break;
			//TEST 3: scrivo nmax file da una directory
			case 'w':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				//input della forma -w dirname,[n=0]
				token = strtok_r(optarg, ",", &save);
				token2 = strtok_r(NULL, ",", &save);
				if(token2 == NULL) writeDirectory(token, 0);
				else if(strlen(token2) > 2) {
					r = atoi(token2+2);
					writeDirectory(token, r);
				}
				else fprintf(stderr, "input errato\n");
				break;

			//TEST 5: leggo n(opzionale) file a caso dal server oppure tutti
			case 'R':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				if(strlen(optarg) > 2) {
					r = atoi(optarg+2);
					readNFiles(r, read_dir);
				}
				else fprintf(stderr, "errore input\n");
				break;	

			//TEST 6: cancello uno o piu file dal server
			case 'c':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				token = strtok_r(optarg, ",", &save);
				do{
					r = removeFile(token);
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
					token = strtok_r(NULL, ",", &save);
				}while(token!=NULL);
				break;

			case 'u':
				if(connection_established == 0) {
					fprintf(stderr, "connesione non stabilita\n");
					exit(EXIT_FAILURE);
				}
				token = strtok_r(optarg, ",", &save);
				do{
					r = unlockFile(token);
					token = strtok_r(NULL, ",", &save);
				}while(token!=NULL);
				break;

			//TEST 7: le read vengono salvate nella directory specificata	
			case 'd':
				if(saveread == 0){
					read_dir = optarg;
					saveread = 1;
				}
				break;

			//getopt error handling manuale
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
					readNFiles(0, NULL);
				}
				else fprintf(stderr,"argomento assente per %c\n", (char)optopt);
				break;
		}
	}
*/