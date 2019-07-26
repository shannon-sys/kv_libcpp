Target=shannondb_cxx
LibName=lib${Target}.so
LibNameStatic=lib${Target}.a
HEAD=./include
CXXFLAGS = -std=c++11
.PHONY:clean test ${LibName} install uninstall

${LibName}:src/kv_db.o src/kv_impl.o src/status.o src/write_batch.o src/iter.o src/log_iter.o \
	src/column_family.o util/coding.o util/comparator.o util/bloom.o util/hash.o util/bloom.o util/filter_policy.o \
	util/crc32c.o util/xxhash.o util/fileoperate.o util/filename.o table/dbformat.o table/filter_block.o src/write_batch_with_index.o \
	cache/lru_cache.o cache/sharded_cache.o table/block_builder.o env/env.o table/format.o table/meta_block.o \
	table/sst_table.o table/table_builder.o env/env_posix.o util/random.o util/arena.o src/read_batch.o
	g++ $(CXXFLAGS) -g -fPIC --shared $^ -o $@ -lsnappy
	ar -rcs ${LibNameStatic} $^

db_test:test/db_test.c
	LD_RUN_PATH=. g++ $(CXXFLAGS) -l${Target} -I${HEAD} -g $^ -o $@ -l${Target}
analyze_sst_test:test/analyze_sst_test.cc
	LD_RUN_PATH=. g++ $(CXXFLAGS) -l${Target} -I${HEAD} -g $^ -o $@ -l${Target}
build_sst_test:test/build_sst_test.cc
	LD_RUN_PATH=. g++ $(CXXFLAGS) -l${Target} -I${HEAD} -g $^ -o $@ -l${Target}
log_iter_test:test/log_iter_test.cc
	LD_RUN_PATH=. g++ $(CXXFLAGS) -l${Target} -I${HEAD} -g $^ -o $@ -l${Target}
log_iter_thread_test:test/log_iter_thread_test.cc
	LD_RUN_PATH=. g++ $(CXXFLAGS) -l${Target} -lpthread -I${HEAD} -g $^ -o $@ -l${Target} -lpthread
skiplist_test:test/skiplist_test.cc
	LD_RUN_PATH=. g++ $(CXXFLAGS) -l${Target} -lpthread -I${HEAD} -g $^ -o $@ -l${Target} -lpthread
write_batch_test:test/write_batch_test.cc
	LD_RUN_PATH=. g++ $(CXXFLAGS) -l${Target} -lpthread -I${HEAD} -g $^ -o $@ -l${Target} -lpthread
read_batch_test:test/read_batch_test.cc
	LD_RUN_PATH=. g++ $(CXXFLAGS) -l${Target} -lpthread -I${HEAD} -g $^ -o $@ -l${Target} -lpthread
uninstall:
	sudo rm -rf /usr/include/swift

install:
	install -p -D -m 0755 ${LibName} /usr/lib
	rm -rf /usr/include/swift
	mkdir /usr/include/swift
	install -p -D -m 0664 include/swift/* /usr/include/swift/
	ldconfig
%.o:%.cc
	g++ $(CXXFLAGS) -g -fPIC -O2 -I${HEAD} -I. -c $^ -o $@

clean:
	rm -rf *.o *.so *.a *_test src/*.o util/*.o table/*.o env/*.o cache/*.o
