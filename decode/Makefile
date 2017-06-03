OBJS = $(wildcard src/*.cpp)

CC = g++

LINKER_FLAGS = -O2 -L/usr/X11R6/lib -lm -lpthread -lX11

COMPILER_FLAGS = -Wall

BINARY = stegdec

all : $(OBJS)
	$(CC) $(COMPILER_FLAGS) $(OBJS) -o $(BINARY) $(LINKER_FLAGS)

