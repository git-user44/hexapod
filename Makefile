GCC=gcc
options= -Wall -g -O3

incl = /usr/include
comp = $(GCC) $(options)
build = $(LIBS) -pthread -lm -lc

all:	ctrl

ctrl : ctrl.o ctrl.h Makefile
	$(GCC) -Wall -o ctrl ctrl*.o $(build) $(LDFLAGS)

ctrl.o : ctrl.c ctrl.h
	$(comp) -c ctrl.c

.PHONY : clean
clean :
	-rm -f ctrl *.o core *~
