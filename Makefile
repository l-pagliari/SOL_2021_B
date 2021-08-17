CC			= gcc
CFLAGS 	   += -std=c99 -Wall -Werror -g
AR   		= ar
ARFLAGS     = rvs
INCDIR		= ./includes
INCLUDES	= -I. -I $(INCDIR)
LIBS		= -lpthread

SRCDIR		= ./src/
LIBDIR		= ./lib/

TARGETS		= server	\
			  client

.PHONY: all clean cleanall
.SUFFIXES: .c .h 

%.o: $(SRCDIR)%.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all: $(TARGETS)

server: server.o sconfig.o worker.o list.o queue.o $(LIBDIR)libPool.a $(LIBDIR)libHash.a
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

client: client.o api.o
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(LIBDIR)libPool.a: threadpool.o
	$(AR) $(ARFLAGS) $@ $<

$(LIBDIR)libHash.a: icl_hash.o 
	$(AR) $(ARFLAGS) $@ $<

#$(LIBDIR)libApi.a: api.o 
#	$(AR) $(ARFLAGS) $@ $<	

server.o: $(SRCDIR)server.c

sconfig.o: $(SRCDIR)sconfig.c

worker.o: $(SRCDIR)worker.c

list.o: $(SRCDIR)list.c

queue.o: $(SRCDIR)queue.c

threadpool.o: $(SRCDIR)threadpool.c 

icl_hash.o: $(SRCDIR)icl_hash.c

client.o: $(SRCDIR)client.c 

api.o: $(SRCDIR)api.c

clean		: 
	rm -f $(TARGETS)
cleanall	: clean
	\rm -f *.o *~ $(LIBDIR)*.a LSOfilestorage