#!/usr/bin/make
#
#	Makefile	WJ107
#

CC = clang

CFLAGS = -g -Wall -Iinclude `sdl-config --cflags`
LIB_OPTS = `sdl-config --libs` -lm -framework OpenGL
EXECNAME = glwhyz

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

CFILES = glwhyz.c tga.c
OBJS = glwhyz.o tga.o

all: glwhyz

include .depend

#
#   Targets
#

glwhyz: $(OBJS)
	$(CC) $(OBJS) -o $(EXECNAME) $(LIB_OPTS)

run: glwhyz
	./$(EXECNAME)

neat:
	rm -f core *~

clean:
	rm -f core $(OBJS) $(EXECNAME) *~

mrproper: clean
	rm -f .depend
	touch .depend

depend dep .depend:
	$(CC) -M -Iinclude `sdl-config --cflags` $(CFILES) > .depend

# EOB
