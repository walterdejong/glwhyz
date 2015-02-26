#!/usr/bin/make
#
#	Makefile	WJ107
#

CC = clang
CXX = clang++

INCLUDE = include `sdl2-config --cflags` \
	-I/System/Library/Frameworks/OpenGL.framework/Headers

CFLAGS = -g -Wall -std=c99 -I$(INCLUDE)
CXXFLAGS = -g -Wall -std=c++11 -I$(INCLUDE)
LIB_OPTS = `sdl2-config --libs` -lm -framework OpenGL
EXECNAME = glwhyz

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

.cpp.o:
	$(CXX) $(CXXFLAGS) -c -o $*.o $<

CFILES = tga.c
CXXFILES = glwhyz.cpp
OBJS = glwhyz.o tga.o

all: glwhyz

include .depend

#
#   Targets
#

glwhyz: $(OBJS)
	$(CXX) $(OBJS) -o $(EXECNAME) $(LIB_OPTS)

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
	$(CC) -M -std=c99 -I$(INCLUDE) $(CFILES) >.depend
	$(CXX) -M -std=c++11 -I$(INCLUDE) $(CXXFILES) >>.depend

# EOB
