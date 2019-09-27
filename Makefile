Target=shannondb_cxx
LibName=lib${Target}.so
LibNameStatic=lib${Target}.a
SNAPPY_LIB=-lsnappy
HEAD=./include
CXXFLAGS = -std=c++11

OBJS = src/kv_db.o src/kv_impl.o src/status.o src/write_batch.o src/iter.o src/log_iter.o \
	src/column_family.o util/coding.o util/comparator.o util/bloom.o util/hash.o util/bloom.o util/filter_policy.o \
	util/crc32c.o util/xxhash.o util/fileoperate.o util/filename.o table/dbformat.o table/filter_block.o src/write_batch_with_index.o \
	cache/lru_cache.o cache/sharded_cache.o table/block_builder.o env/env.o table/format.o table/meta_block.o \
	table/sst_table.o table/table_builder.o env/env_posix.o util/random.o util/arena.o src/read_batch.o

TESTS = db_test analyze_sst_test build_sst_test log_iter_test log_iter_thread_test \
		skiplist_test write_batch_test read_batch_test

.PHONY: clean test install uninstall

all: ${LibName}

${LibName}: $(OBJS)
	g++ $(CXXFLAGS) -g -fPIC --shared $^ -o $@ $(SNAPPY_LIB)
	ar -rcs ${LibNameStatic} $^

test: $(TESTS)

db_test: test/db_test.c $(OBJS)
	g++ $(CXXFLAGS) -I${HEAD} -g $^ -o $@ $(SNAPPY_LIB)
analyze_sst_test: test/analyze_sst_test.cc $(OBJS)
	g++ $(CXXFLAGS) -I${HEAD} -g $^ -o $@ $(SNAPPY_LIB)
build_sst_test: test/build_sst_test.cc $(OBJS)
	g++ $(CXXFLAGS) -I${HEAD} -g $^ -o $@ $(SNAPPY_LIB)
log_iter_test: test/log_iter_test.cc $(OBJS)
	g++ $(CXXFLAGS) -I${HEAD} -g $^ -o $@ $(SNAPPY_LIB)
log_iter_thread_test: test/log_iter_thread_test.cc $(OBJS)
	g++ $(CXXFLAGS) -I${HEAD} -g $^ -o $@ $(SNAPPY_LIB) -lpthread
skiplist_test: test/skiplist_test.cc $(OBJS)
	g++ $(CXXFLAGS) -I${HEAD} -g $^ -o $@ $(SNAPPY_LIB) -lpthread
write_batch_test: test/write_batch_test.cc $(OBJS)
	g++ $(CXXFLAGS) -I${HEAD} -g $^ -o $@ $(SNAPPY_LIB) -lpthread
read_batch_test: test/read_batch_test.cc $(OBJS)
	g++ $(CXXFLAGS) -I${HEAD} -g $^ -o $@ $(SNAPPY_LIB) -lpthread

uninstall:
	rm -rf /usr/include/swift
	rm /usr/lib/${LibName}

install: install_header install_lib
	ldconfig

install_header:
	rm -rf /usr/include/swift
	mkdir /usr/include/swift
	install -p -D -m 0664 include/swift/* /usr/include/swift/

install_lib:
	install -p -D -m 0755 ${LibName} /usr/lib

%.o: %.cc
	g++ $(CXXFLAGS) -g -fPIC -O2 -I${HEAD} -I. -c $^ -o $@

clean:
	rm -rf *.o *.so *.a $(TESTS) src/*.o util/*.o table/*.o env/*.o cache/*.o
