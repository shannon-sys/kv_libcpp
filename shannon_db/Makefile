CC = gcc
CXX = g++
CFLAGS = -g
CXXFLAGS=-std=c++11
GTEST_LIB=-lgtest -lgtest_main -lpthread
SHANNON_LIB = -lshannon_db
SNAPPY_LIB = -lsnappy
OBJS = shannon_db.o
SST_OBJS = ./sst_table/util.o ./sst_table/blocks.o \
	./sst_table/bloom.o ./sst_table/filter_block.o \
	./sst_table/block_builder.o ./sst_table/table_builder.o
OBJS += $(SST_OBJS)
LIB = libshannon_db.so
LibStatic=libshannon_db.a

TESTS = db_test writebatch_test writebatch_nonatomic_test snapshot_test iterator_test \
		prefixiterator_test deletedatabase_test columnfamily_test recover_test \
		readbatch_test analyze_sst_test iterator_direction_test compress_test build_sst_test \
		property_test

.PHONY: clean test install

# make dynamic library
all:$(LIB)
%.o:%.c
	$(CC) $(CFLAGS) -fpic -c $< -o $@
./sst_table/%.o:./sst_table/%.c
	$(CC) $(CFLAGS) -fpic -c $< -o $@
$(LIB):$(OBJS)
	${CC} -shared -o $@ $(OBJS) $(SNAPPY_LIB)
	ar -rcs ${LibStatic} $^

install: install_header install_lib
	ldconfig

install_header:
	install -p -D -m 0664 shannon_db.h /usr/include

install_lib:
	install -p -D -m 0755 libshannon_db.so /usr/lib

# make test program

test: $(TESTS)

db_test: test/shannondb_test.c $(OBJS)
	 $(CC) $(CFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB)
writebatch_test: test/shannondb_writebatch_test.c $(OBJS)
	 $(CC) $(CFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB) -lpthread
writebatch_nonatomic_test: test/shannondb_writebatch_nonatomic_test.c $(OBJS)
	 $(CC) $(CFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB) -lpthread
snapshot_test: test/shannondb_snapshot_test.c $(OBJS)
	 $(CC) $(CFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB) -lpthread
iterator_test: test/shannondb_iterator_test.c $(OBJS)
	 $(CC) $(CFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB)
prefixiterator_test: test/shannondb_prefixiterator_test.c $(OBJS)
	 $(CC) $(CFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB)
deletedatabase_test: test/shannondb_deletedatabase_test.c $(OBJS)
	 $(CC) $(CFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB) -lpthread
columnfamily_test: test/shannondb_column_family_test.c $(OBJS)
	 $(CC) $(CFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB)
recover_test: test/shannondb_recover_test.c $(OBJS)
	 $(CC) $(CFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB)
readbatch_test: test/shannondb_readbatch_test.c $(OBJS)
	 $(CC) $(CFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB)
analyze_sst_test: test/shannondb_sst_test.c $(OBJS)
	 $(CC) $(CFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB)
iterator_direction_test: test/shannondb_iterator_direction_test.c $(OBJS)
	 $(CC) $(CFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB)
compress_test: test/shannondb_compress_test.c $(OBJS)
	 $(CC) $(CFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB)
build_sst_test: test/shannondb_build_sst_test.c $(OBJS)
	 $(CC) $(CFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB)
property_test: test/shannondb_property_test.c $(OBJS)
	 $(CC) $(CFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB)
extern_c_test: test/shannondb_extern_c_test.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB) $(GTEST_LIB)
remove_db_test: test/shannondb_remove_db_test.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) -I./ $^ -o $@ $(SNAPPY_LIB) $(GTEST_LIB)
clean:
	rm -rf a.out *.o *.so *.a *.pyc *_test $(SST_OBJS) $(TESTS)

