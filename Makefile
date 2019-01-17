Target=shannondb_cxx
LibName=lib${Target}.so
LibNameStatic=lib${Target}.a
HEAD=./include
CXXFLAGS = -std=c++11
.PHONY:clean test ${LibName} install uninstall

${LibName}:kv_db.o kv_impl.o status.o write_batch.o iter.o column_family.o \
	util/coding.o util/comparator.o util/bloom.o util/hash.o \
	util/crc32c.o util/xxhash.o util/fileoperate.o \
	cache/lru_cache.o cache/sharded_cache.o \
	table/block_based_table_factory.o table/sst_table.o env/env_posix.o
	g++ $(CXXFLAGS) -g -fPIC --shared $^ -o $@ -lsnappy
	ar -rcs ${LibNameStatic} $^

db_test:test/db_test.c
	LD_RUN_PATH=. g++ $(CXXFLAGS) -l${Target} -I${HEAD} -g $^ -o $@ -l${Target}
sst_test:test/sst_test.cc
	LD_RUN_PATH=. g++ $(CXXFLAGS) -l${Target} -I${HEAD} -g $^ -o $@ -l${Target}
uninstall:
	sudo rm -rf /usr/include/swift

install:
	install -p -D -m 0755 ${LibName} /usr/lib
	rm -rf /usr/include/swift
	mkdir /usr/include/swift
	install -p -D -m 0664 include/swift/* /usr/include/swift/
	ldconfig
%.o:%.cc
	g++ $(CXXFLAGS) -g -fPIC -I${HEAD} -I. -c $^ -o $@

clean:
	rm -rf *.o *.so *.a *_test util/*.o table/*.o env/*.o cache/*.o
