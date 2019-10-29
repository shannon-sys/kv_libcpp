CFLAGS = -g
LIBS = -lsnappy -lshannondb

all:
	gcc $(CFLAGS) ../util.c ../blocks.c ../bloom.c ../filter_block.c filter_block_test.c -o filter_block_test $(LIBS)

clean:
	rm -rf *.o filter_block_test test_filter.txt
