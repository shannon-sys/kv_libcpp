#include <gtest/gtest.h>
#include <thread>
#include "../shannon_db.h"

class ShannonDBTest {
public:
	void SetUp(char const *dbname) {
		this->dbname = dbname;

		// create options
		options = shannon_db_options_create();
		shannon_db_options_set_create_if_missing(options, 1);
		woptions = shannon_db_writeoptions_create();
		shannon_db_writeoptions_set_sync(woptions, 1);
		roptions = shannon_db_readoptions_create();

		char *err = nullptr;
		db = shannon_db_open(options, "/dev/kvdev0", dbname, &err);
		ASSERT_NE(db, nullptr);
	}

	void TearDown() {
		char *err = nullptr;
		shannon_destroy_db(db, &err);
		// ASSERT_EQ(err, nullptr);
		shannon_db_options_destroy(options);
		shannon_db_readoptions_destroy(roptions);
		shannon_db_writeoptions_destroy(woptions);
		shannon_db_close(db);
	}

	int do_one_loop() {
		char *err = nullptr;
		const int N = 10000000;
		__u64 key;
		char value[4096] = { 0 };

		for (int i = 0; i < N; i++) {
			key = i;
			shannon_db_put(db, woptions, (char *)&key, sizeof(key), value, 4096, &err);
		}
	}

private:
	struct shannon_db *db;
	struct db_options *options;
	struct db_readoptions *roptions;
	struct db_writeoptions *woptions;
	char const *dbname;
};

void run_thread(int index) {
	for (int i = 0; i < 50; i++) {
		std::cout << "db=" << index << ", loop=" << i << std::endl;
		ShannonDBTest test;
		test.SetUp(std::to_string(index).c_str());
		test.do_one_loop();
		test.TearDown();
	}
}

int main(int argc, char *argv[]) {
	const int N = 6;
	std::thread *threads = new std::thread[N];
	for (int i = 0; i < N; ++i) {
		threads[i] = std::thread(run_thread, i);
	}
	for (int i = 0; i < N; ++i) {
		threads[i].join();
	}

	return 0;
}
