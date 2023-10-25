# Makefile, ECE252  
# Yiqing Huang
#f
CC = gcc       # compiler
CFLAGS = -Wall -g -std=gnu99 # compilation flags
LD = gcc      # linker
LDFLAGS = -g  -std=gnu99 # debugging symbols in build
LDLIBS = -lcurl  -lz -pthread

# For students  
LIB_UTIL = zutil.o crc.o
SRCS   = paster2.c crc.c zutil.c
OBJS_PASTER2   = paster2.o $(LIB_UTIL) 

TARGETS= paster2

all: ${TARGETS}

paster2: $(OBJS_PASTER2) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 


.PHONY: clean
clean:
	rm -f *.d *.o $(TARGETS) *.png
