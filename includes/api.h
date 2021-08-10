#if !defined(API_H)
#define API_H

#include <time.h>

int openConnection(const char *sockname, int msec, const struct timespec abstime);

int closeConnection(const char *sockname);

int openFile(const char *pathname, int flags);

int writeFile(const char *pathname, const char *dirname);

int readFile(const char* pathname, void** buf, size_t* size);

#endif /* API_H */