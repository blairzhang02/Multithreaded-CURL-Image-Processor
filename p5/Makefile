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
SRCS   = findpng3.c 
OBJS_FINDPNG2   = findpng3.o $(LIB_UTIL) 

TARGETS= findpng3

all: ${TARGETS}

findpng3: $(OBJS_FINDPNG2) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 


.PHONY: clean
clean:
	rm -f *~ *.d *.o $(TARGETS) *.png *.html  *.txt
