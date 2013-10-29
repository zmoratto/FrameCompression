BREWDIR=/Users/zmoratto/homebrew

CFLAGS += -g -O0 -Wall -I$(BREWDIR)/include

LDFLAGS += -L$(BREWDIR)/lib -lavcodec -lavfilter -lavformat -lavutil

%.o : %.cc
	$(CC) -c -o $@ $< $(CFLAGS)

compress : compress.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f compress *~ *.o
