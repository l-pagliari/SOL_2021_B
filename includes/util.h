#if !defined(UTIL_H)
#define UTIL_H

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <linux/limits.h>

#if !defined(BUFSIZE)
#define BUFSIZE 256
#endif

#if !defined(MAXBACKLOG)
#define MAXBACKLOG 32
#endif

#if !defined(EXTRA_LEN_PRINT_ERROR)
#define EXTRA_LEN_PRINT_ERROR   512
#endif

#define CHECK_EQ_EXIT(name, X, val, str, ...)	\
    if ((X)==val) {								\
        perror(#name);							\
		int errno_copy = errno;					\
		print_error(str, __VA_ARGS__);			\
		exit(errno_copy);						\
    }

#define SYSCALL_EXIT(name, r, sc, str, ...)		\
    if ((r=sc) == -1) {							\
		perror(#name);							\
		int errno_copy = errno;					\
		print_error(str, __VA_ARGS__);			\
		exit(errno_copy);						\
    }

#define SYSCALL_RETURN(name, r, sc, str, ...)   \
    if ((r=sc) == -1) {                         \
        perror(#name);                          \
        int errno_copy = errno;                 \
        print_error(str, __VA_ARGS__);          \
        errno = errno_copy;                     \
        return r;                               \
    }

#define LOCK_RETURN(l, r)  							\
    if (pthread_mutex_lock(l)!=0) {					\
   		fprintf(stderr, "ERRORE FATALE lock\n");	\
    	return r;									\
 	}

 #define UNLOCK_RETURN(l,r)    						\
 	if (pthread_mutex_unlock(l)!=0) {				\
   		fprintf(stderr, "ERRORE FATALE unlock\n");	\
    	return r;									\
  	}

/**
 * \brief Procedura di utilita' per la stampa degli errori
 */
static inline void print_error(const char * str, ...) {
    const char err[]="ERROR: ";
    va_list argp;
    char * p=(char *)malloc(strlen(str)+strlen(err)+EXTRA_LEN_PRINT_ERROR);
    if (!p) {
	perror("malloc");
        fprintf(stderr,"FATAL ERROR nella funzione 'print_error'\n");
        return;
    }
    strcpy(p,err);
    strcpy(p+strlen(err), str);
    va_start(argp, str);
    vfprintf(stderr, p, argp);
    va_end(argp);
    free(p);
}

/** Evita letture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la lettura da fd leggo EOF
 *   \retval size se termina con successo
 */
static inline int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
        if ((r=read((int)fd ,bufptr,left)) == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0;   // EOF
            left    -= r;
            bufptr  += r;
    }
    return size;
}

/** Evita scritture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura la write ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
static inline int writen(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
        if ((r=write((int)fd ,bufptr,left)) == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0;  
            left    -= r;
            bufptr  += r;
    }
    return 1;
}

/**
 * @brief Restituisce la stringa nel formato [hh:mm:ss] 
 */
static inline char * tStamp(char * timeString) {
    struct tm *tmp;
    time_t curtime;
    time(&curtime);
    if((tmp = localtime(&curtime)) == NULL ){
        perror("localtime");
        return NULL;
    }
    if((strftime(timeString, 11, "[%H:%M:%S]", tmp)) == 0) {
        perror("strftime");
        return NULL;
    }
    return timeString;
}

#endif /* UTIL_H */