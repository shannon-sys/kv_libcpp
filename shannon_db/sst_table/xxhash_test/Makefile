CC = gcc

all:
	g++ -c xxhash.cc -std=c++11 -o xxhash.o
	gcc shannon_xxhash_test.c ../util.c xxhash.o -o test_xxhash

clean:
	rm -rf *.o test_xxhash
