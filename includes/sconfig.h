#if !defined(SCONFIG_H)
#define SCONFIG_H

#include <mydata.h>

config_t * read_config(char * conf_path);
void print_config(config_t * conf);

#endif /* SCONFIG_H */