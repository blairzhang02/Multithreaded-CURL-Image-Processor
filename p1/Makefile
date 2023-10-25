# Makefile, ECE252  
# Yiqing Huang
#f
CC = gcc       # compiler
CFLAGS = -Wall -g -std=c99 # compilation flags
LD = gcc       # linker
LDFLAGS = -g   # debugging symbols in build
LDLIBS = -lz   # link with libz

# For students 
LIB_UTIL = zutil.o crc.o
SRCS   = pnginfo.c findpng.c catpng.c crc.c zutil.c
OBJS_PNGINFO   = pnginfo.o $(LIB_UTIL) 
OBJS_FINDPNG   = findpng.o $(LIB_UTIL) 
OBJS_CATPNG   = catpng.o $(LIB_UTIL) 

TARGETS= pnginfo findpng catpng

all: ${TARGETS}

pnginfo: $(OBJS_PNGINFO) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

findpng: $(OBJS_FINDPNG) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

catpng: $(OBJS_CATPNG) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 



.PHONY: clean
clean:
	rm -f *.d *.o $(TARGETS) 
