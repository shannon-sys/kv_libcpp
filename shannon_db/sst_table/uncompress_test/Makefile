CFLAGS = -g -lsnappy -lshannon_db
LIBS = -lsnappy -lshannon_db

all:
	gcc $(CFLAGS) ../util.c ../blocks.c uncompress_test.c -o uncompress_test $(LIBS)

clean:
	rm -rf *.o uncompress_test
