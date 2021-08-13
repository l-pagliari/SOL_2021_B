#if !defined(API_H)
#define API_H

#include <time.h>

int openConnection(const char *sockname, int msec, const struct timespec abstime);

int closeConnection(const char *sockname);

int openFile(const char *pathname, int flags);

int writeFile(const char *pathname, const char *dirname);

int writeDirectory(const char *dirname, int max_files);

int readFile(const char* pathname, void** buf, size_t* size);

int readNFiles(int N, const char* dirname);

int unlockFile(const char* pathname);

int lockFile(const char* pathname);

int removeFile(const char* pathname);

int saveFile(const char* dirname, const char* pathname, void* buf, size_t size);

int setDelay(long msec);

#endif /* API_H */