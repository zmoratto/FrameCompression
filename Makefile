BREWDIR=/Users/zmoratto/homebrew

CFLAGS += -g -O3 -Wall -I$(BREWDIR)/include -I$(PWD)

LDFLAGS += -L$(BREWDIR)/lib -lavcodec -lavfilter -lavformat -lavutil

%.o : %.cc
	$(CC) -c -o $@ $< $(CFLAGS)

all: compress compare_pgm decompress

compress : compress.o rejigger.o depth_code.o
	$(CC) -o $@ $^ $(LDFLAGS)

decompress : decompress.o rejigger.o pgm_io.o depth_code.o
	$(CC) -o $@ $^ $(LDFLAGS)

compare_pgm : compare_pgm.o pgm_io.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f compress compare_pgm *~ *.o
