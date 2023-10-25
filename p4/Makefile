# Makefile, ECE252  
# Yiqing Huang
#f
CC = gcc 
CFLAGS_XML2 = $(shell xml2-config --cflags)
CFLAGS_CURL = $(shell curl-config --cflags)
CFLAGS = -Wall $(CFLAGS_XML2) $(CFLAGS_CURL) -std=gnu99 -g -DDEBUG1_
LD = gcc
LDFLAGS = -std=gnu99 -g 
LDLIBS_XML2 = $(shell xml2-config --libs)
LDLIBS_CURL = $(shell curl-config --libs)
LDLIBS = -lcurl -lz -pthread $(LDLIBS_XML2) $(LDLIBS_CURL) 

# For students  
SRCS   = findpng2.c 
OBJS_FINDPNG2   = findpng2.o $(LIB_UTIL) 

TARGETS= findpng2

all: ${TARGETS}

findpng2: $(OBJS_FINDPNG2) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 


.PHONY: clean
clean:
	rm -f *~ *.d *.o $(TARGETS) *.png *.html  *.txt
