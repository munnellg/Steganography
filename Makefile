OBJS = $(wildcard src/*.cpp)

CC = g++

LINKER_FLAGS = -O2 -L/usr/X11R6/lib -lm -lpthread -lX11 -lboost_system -lboost_filesystem

COMPILER_FLAGS = -Wall -g

BINARY = steg

all : $(OBJS)
	$(CC) $(COMPILER_FLAGS) $(OBJS) -o $(BINARY) $(LINKER_FLAGS)

