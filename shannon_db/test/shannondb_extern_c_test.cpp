#include <gtest/gtest.h>
#include "../shannon_db.h"

class ShannonDBTest : public ::testing::Test {
protected:
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

	void TearDown() override {
		char *err = nullptr;
		shannon_destroy_db(db, &err);
		ASSERT_EQ(err, nullptr);
		shannon_db_options_destroy(options);
		shannon_db_readoptions_destroy(roptions);
		shannon_db_writeoptions_destroy(woptions);
		shannon_db_close(db);
	}

	struct shannon_db *db;
	struct db_options *options;
	struct db_readoptions *roptions;
	struct db_writeoptions *woptions;
	char const *dbname;
};

TEST_F(ShannonDBTest, Extern) {
	SetUp("extern.db");
	std::cout << "dbname=" << dbname << std::endl;

	char *err = nullptr;
	const int N = 1000000;
	__u64 key;

	std::cout << "stage[1] [init] begin" << std::endl;
	for (int i = 0; i < N; i++) {
		key = i;
		std::string value = std::to_string(key);
		shannon_db_put(db, woptions, (char *)&key, sizeof(key), value.data(), value.size(), &err);
		ASSERT_EQ(err, nullptr);
	}
	std::cout << "stage[1] end" << std::endl;

	std::cout << "stage[2] [check] begin" << std::endl;
	char buf[4096];
	for (int i = 0; i < N; i++) {
		key = i;
		unsigned int value_len = -1;
		int ret = shannon_db_get(db, roptions, (char *)&key, sizeof(key), buf, 4096, &value_len, &err);

		ASSERT_EQ(ret, 0) << "key=" << key;
		std::string value_1 = std::to_string(key);
		std::string value_2(buf, value_len);
		ASSERT_EQ(value_1, value_2);
	}
	std::cout << "stage[2] [check] end" << std::endl;
}

int main(int argc, char *argv[]) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
