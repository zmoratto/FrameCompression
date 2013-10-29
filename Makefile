BREWDIR=/Users/zmoratto/homebrew

CFLAGS += -g -O0 -Wall -I$(BREWDIR)/include -I$(PWD)

LDFLAGS += -L$(BREWDIR)/lib -lavcodec -lavfilter -lavformat -lavutil

%.o : %.cc
	$(CC) -c -o $@ $< $(CFLAGS)

all: compress compare_pgm

compress : compress.o rejigger.o
	$(CC) -o $@ $^ $(LDFLAGS)

compare_pgm : compare_pgm.o pgm_io.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f compress compare_pgm *~ *.o
