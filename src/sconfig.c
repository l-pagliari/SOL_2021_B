#define _POSIX_C_SOURCE 2001112L 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <util.h>
#include <mydata.h>

/*CONFIGURAZIONE SERVER: 
	non viene mai modificata dopo l'avvio e non e' gestita dall'utente che usa il client quindi
	ho scelto un formato rigido di configurazione nella forma:
	
	#commento
	[descrizione] "<stringa indirizzo socket>"
	[descrizione] "<stringa indirizzo file log>" 
	[descrizione] "<numero workers>"
	[descrizione] "<numero massimo di file salvabili>"
	[descrizione] "<capacita' della memoria server in bytes>"

	righe non vuote, non iniziate da '#', generano errore ed il programma termina subito.
*/

//IMPORTANTE: all'aumentare dei valori di configurazione, incrementare questo valore
#ifndef NUM_CONF_VALS
#define NUM_CONF_VALS 5
#endif

//effettuo il parsing del file di configurazione
//ci aspettiamo i valori ordinati e racchiusi tra ""
config_t * read_config(char * conf_path) {
	int n, i = 0;
	char *token, *saveptr, *buffer;
	char ** bufptr;
	config_t * conf;
	FILE *fp;

	CHECK_EQ_EXIT("malloc", NULL, (buffer = malloc(BUFSIZE*sizeof(char))), "buffer configurazione server\n","");
	CHECK_EQ_EXIT("malloc", NULL, (bufptr = malloc(NUM_CONF_VALS*sizeof(char*))), "bufptr configurazione server\n","");
	CHECK_EQ_EXIT("malloc", NULL, (conf = malloc(sizeof(config_t))), "config_t configurazione server\n", "");
	CHECK_EQ_EXIT("fopen", NULL, (fp = fopen(conf_path, "r")), "apertura file configurazione server\n", "");

	while(fgets(buffer, BUFSIZE, fp) != NULL) {
		if(buffer[0] == '#' || buffer[0] == '\n') continue; //ignoro le line che iniziano con # o vuote

		token = strtok_r(buffer, "\"", &saveptr);
		token = strtok_r(NULL, "\"", &saveptr);
		if(token == NULL) {
			fprintf(stderr, "formato file config errato\n");
			exit(EXIT_FAILURE);
		}

		n = strlen(token);
		CHECK_EQ_EXIT("malloc", NULL, (bufptr[i] = malloc(n * sizeof(char))), "bufptr configurazione server\n", "");
		strncpy(bufptr[i], token, n);

		if(++i == NUM_CONF_VALS + 1) {
			fprintf(stderr, "formato config errato: troppi argomenti\n");
			exit(EXIT_FAILURE);
		}
	}
	free(buffer);
	fclose(fp);

	//creo in ordine la struct config
	//SOCKET NAME (i=0)
	n = strlen(bufptr[0]) + 1;
	CHECK_EQ_EXIT("malloc", NULL, (conf->sock_name = malloc(n * sizeof(char))), 
		"sockname configurazione server\n", "");
	strncpy(conf->sock_name, bufptr[0], n);
	conf->sock_name[n] = '\0';
	//LOG NAME (i=1)
	n = strlen(bufptr[1]) + 1;
	CHECK_EQ_EXIT("malloc", NULL, (conf->log_name = malloc(n * sizeof(char))), 
		"logname configurazione server\n", "");
	strncpy(conf->log_name, bufptr[1], n);
	conf->log_name[n] = '\0';
	//NUM WORKERS (i=2)
	conf->num_workers = strtol(bufptr[2], NULL, 10);
	//MAX FILES IN MEMORY (i=3)
	conf->mem_files = strtol(bufptr[3], NULL, 10);
	//MEMORY SIZE (i=4)
	conf->mem_bytes = strtol(bufptr[4], NULL, 10);

	MAX_CAP = conf->mem_bytes;
	MAX_FIL = conf->mem_files;

	for(int j=0; j<i; j++) free(bufptr[j]);
	free(bufptr);
	return conf;
}

//utility per la stampa della configurazione attuale
void print_config(config_t * conf) {
	if(conf == NULL) {
		fprintf(stderr, "impossibile stampare configurazione\n");
	}
	else {
		printf("***CONFIGURAZIONE SERVER***\n\n"
			"indirizzo socket: %s\nindirizzo file log: %s\nnumero workers: %ld\n"
			"numero massimo file in memoria: %ld\nmemoria server: %ld bytes\n\n"
			"***FINE CONFIGURAZIONE SERVER***\n", conf->sock_name, conf->log_name,
			conf->num_workers, conf->mem_files, conf->mem_bytes);
	}
}