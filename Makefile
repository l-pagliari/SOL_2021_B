CC			= gcc
CFLAGS 	   += -std=c99 -Wall -Werror -g
AR   		= ar
ARFLAGS     = rvs
INCDIR		= ./includes
INCLUDES	= -I. -I $(INCDIR)
LIBS		= -lpthread

SRCDIR		= ./src/
LIBDIR		= ./lib/
OBJDIR		= ./obj/
BINDIR		= ./bin/

TARGETS		= $(BINDIR)server	\
			  $(BINDIR)client

.PHONY: all clean cleanall cleantest test1 test2 test3
.SUFFIXES: .c .h 

$(OBJDIR)%.o: $(SRCDIR)%.c
	@ mkdir -p obj
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all: $(TARGETS)

$(BINDIR)server: $(OBJDIR)server.o $(OBJDIR)sconfig.o $(OBJDIR)worker.o $(OBJDIR)list.o $(OBJDIR)queue.o $(LIBDIR)libPool.a $(LIBDIR)libHash.a
	@ mkdir -p bin log tmp
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(BINDIR)client: $(OBJDIR)client.o $(LIBDIR)libApi.a
	@ mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(LIBDIR)libPool.a: $(OBJDIR)threadpool.o
	@ mkdir -p lib
	$(AR) $(ARFLAGS) $@ $<

$(LIBDIR)libHash.a: $(OBJDIR)icl_hash.o 
	@ mkdir -p lib
	$(AR) $(ARFLAGS) $@ $<

$(LIBDIR)libApi.a: $(OBJDIR)api.o 
	@ mkdir -p lib
	$(AR) $(ARFLAGS) $@ $<	

$(OBJDIR)server.o: $(SRCDIR)server.c

$(OBJDIR)sconfig.o: $(SRCDIR)sconfig.c

$(OBJDIR)worker.o: $(SRCDIR)worker.c

$(OBJDIR)list.o: $(SRCDIR)list.c

$(OBJDIR)queue.o: $(SRCDIR)queue.c

$(OBJDIR)threadpool.o: $(SRCDIR)threadpool.c

$(OBJDIR)icl_hash.o: $(SRCDIR)icl_hash.c

$(OBJDIR)client.o: $(SRCDIR)client.c 

$(OBJDIR)api.o: $(SRCDIR)api.c

test1		: 	cleantest
	@echo [INIZIO TEST 1]
	bash script/test1.sh
	@echo [FINE TEST 1]

test2		: 	cleantest
	@echo [INIZIO TEST 2]
	bash script/test2.sh
	@echo [FINE TEST 2]

test3		: 	cleantest
	@echo [INIZIO TEST 3]
	bash script/test3.sh
	@echo [FINE TEST 3]

cleantest	:
	rm -f test/expelled/* test/readsave/*
clean		: 
	rm -f $(TARGETS)
cleanall	: clean cleantest
	\rm -f *~ $(OBJDIR)*.o $(LIBDIR)*.a ./log/* ./tmp/*