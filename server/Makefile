# the compiler: gcc for C program, define as g++ for C++
CC = gcc
RM = rm -rf

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -Wall -pthread

# Include library directories
LIBDIR = serverHandler
OBJDIR = obj
INCOBJDIR = ../socket/obj

# Library 
LIB = $(shell find $(LIBDIR) -name "*.c")
OBJ = $(LIB:$(LIBDIR)/%.c=$(OBJDIR)/%.o)
INCOBJ = $(shell find $(INCOBJDIR) -name "*.o")

# Compile executor
BIN = server

all: $(BIN)

$(OBJDIR)/%.o:$(LIBDIR)/%.c
	$(CC) -c $(CFLAGS) -o $@ $< 

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(INCOBJ)

# Clean old compilation
clean:
	$(RM) $(BIN) $(OBJ)