# for combined C/C++ library: libshannondb.so
C_LIB_PATH = ./shannon_db
C_LIB = $(C_LIB_PATH)/libshannon_db.a
CPP_LIB_PATH = .
CPP_LIB = $(CPP_LIB_PATH)/libshannondb_cxx.a

LIB = libshannondb.so

COMPRESS_FLAG =
COMPRESS_LIB =

ifdef ALL_COMPRESS
	COMPRESS_FLAG = -DALL_COMPRESS
	COMPRESS_LIB =  -llz4 -lzstd -lbz2 -lz
else
ifdef ZLIB
	COMPRESS_FLAG += -DZLIB
	COMPRESS_LIB += -lz
endif
ifdef ZSTD
	COMPRESS_FLAG += -DZSTD
	COMPRESS_LIB += -lzstd
endif
ifdef BZIP2
	COMPRESS_FLAG += -DBZIP2
	COMPRESS_LIB += -lbz2
endif
ifdef LZ4
	COMPRESS_FLAG += -DLZ4
	COMPRESS_LIB += -llz4
endif
endif
.PHONY: $(C_LIB) $(CPP_LIB) install clean

all: $(LIB)

$(LIB): $(C_LIB) $(CPP_LIB)
	$(CC) -shared -o ${LIB} -Wl,--whole-archive $(C_LIB) $(CPP_LIB) -Wl,--no-whole-archive -lsnappy -lstdc++

$(C_LIB):
	+$(MAKE) -C $(C_LIB_PATH)

$(CPP_LIB):
	+$(MAKE) -C $(CPP_LIB_PATH) cpp

install:
	install -p -D -m 0755 ${LIB} /usr/lib
	make install_header -C $(C_LIB_PATH)
	make cpp_install_header -C $(CPP_LIB_PATH)
	ldconfig

uninstall:
	rm -rf /usr/include/swift
	rm -rf /usr/include/shannon_db.h
	rm /usr/lib/${LIB}

clean:
	make clean -C $(C_LIB_PATH)
	make cpp_clean -C $(CPP_LIB_PATH)
	rm -rf ${LIB}

# for c++ only library: libshannondb_cxx.so, libshannondb_cxx.a

Target=shannondb_cxx
LibName=lib${Target}.so
LibNameStatic=lib${Target}.a
SNAPPY_LIB=-lsnappy
HEAD=./include
CXXFLAGS = -std=c++11
CXXFLAGS += ${COMPRESS_FLAG}

OBJS = src/kv_db.o src/kv_impl.o src/status.o src/write_batch.o src/iter.o src/log_iter.o \
	src/column_family.o util/coding.o util/comparator.o util/bloom.o util/hash.o util/bloom.o util/filter_policy.o \
	util/crc32c.o util/xxhash.o util/fileoperate.o util/filename.o table/dbformat.o table/filter_block.o src/write_batch_with_index.o \
	cache/lru_cache.o cache/sharded_cache.o table/block_builder.o env/env.o table/format.o table/meta_block.o \
	table/sst_table.o table/table_builder.o env/env_posix.o util/random.o util/arena.o src/read_batch.o

TESTS = db_test analyze_sst_test build_sst_test log_iter_test log_iter_thread_test \
		skiplist_test write_batch_test read_batch_test kvlib_test

.PHONY: clean test install uninstall

cpp: ${LibName}

${LibName}: $(OBJS)
	g++ $(CXXFLAGS) -g -fPIC --shared $^ -o $@ $(SNAPPY_LIB) $(COMPRESS_LIB)
	ar -rcs ${LibNameStatic} $^

cpp_test: $(TESTS)

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
kvlib_test: test/kvlib_test.cc $(OBJS)
	g++ $(CXXFLAGS) -I${HEAD} -g $^ -o $@ $(SNAPPY_LIB) -lpthread -lgtest

migrate: table/migrate.cc $(OBJS)
	g++ $(CXXFLAGS) -I${HEAD} -g $^ -o $@ $(SNAPPY_LIB) $(COMPRESS_LIB)
cpp_uninstall:
	rm -rf /usr/include/swift
	rm /usr/lib/${LibName}

cpp_install: install_header install_lib
	ldconfig

cpp_install_header:
	rm -rf /usr/include/swift
	mkdir /usr/include/swift
	install -p -D -m 0664 include/swift/* /usr/include/swift/

cpp_install_lib:
	install -p -D -m 0755 ${LibName} /usr/lib

%.o: %.cc
	g++ $(CXXFLAGS) -g -fPIC -O2 -I${HEAD} -I. -c $^ -o $@

cpp_clean:
	rm -rf *.o *.so *.a $(TESTS) src/*.o util/*.o table/*.o env/*.o cache/*.o
