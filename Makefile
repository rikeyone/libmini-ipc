#CC=aarch64-linux-gnu-gcc
#STRIP=aarch64-linux-gnu-strip
CC=gcc
STRIP=strip

CFLAGS= -g -Os -Wall  -I./include/ -I. -L.
LDFLAGS= -lpthread -lrt -lmini-ipc

LIB_CFLAGS= -c -g -fPIC -Os -Wall -I./include/ -I.
LIB_LDFLAGS= -shared -lpthread -lrt 



LIBDIR = .
LIBSRC := $(wildcard $(LIBDIR)/*.c)
LIBOBJ := $(patsubst %.c,%.o,$(LIBSRC))
LIBSO = libmini-ipc.so

SAMPLE = ./samples
SAMPLESRC := $(wildcard $(SAMPLE)/*.c)
SAMPLEBIN := $(patsubst %.c,%,$(SAMPLESRC))
SAMPLENEWBIN := $(notdir %,$(SAMPLEBIN))

.PHONY: lib test clean

all: lib test

test: $(SAMPLEBIN)
$(SAMPLEBIN): %:%.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) 
	$(STRIP) $@ 
	cp $@ .

lib: $(LIBSO)
$(LIBSO): $(LIBOBJ)
	$(CC) $^ $(LIB_LDFLAGS) -o $@
	$(STRIP) $(LIBSO)
$(LIBOBJ):%.o:%.c
	$(CC) $(LIB_CFLAGS) $< -o $@
clean:
	rm -f *.o $(LIBSO) $(SAMPLEBIN) $(SAMPLENEWBIN)
