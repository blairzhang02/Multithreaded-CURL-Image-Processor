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
SRCS   = paster.c crc.c zutil.c
OBJS_PASTER   = paster.o $(LIB_UTIL) 

TARGETS= paster 

all: ${TARGETS}

paster: $(OBJS_PASTER) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 


.PHONY: clean
clean:
	rm -f *.d *.o $(TARGETS) *.png
