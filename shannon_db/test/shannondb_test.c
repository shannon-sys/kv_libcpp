#include <shannon_db.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
char *phase = "";
#define BUF_LEN 128
static void StartPhase(char *name)
{
	fprintf(stderr, "=== Test %s\n", name);
	phase = name;
}
#define CheckCondition(cond) \
	({if (!(cond)) { \
		fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, phase, #cond); \
		abort(); \
	} })

#define CheckNoError(err) \
	({if ((err) != 0) { \
		fprintf(stderr, "%s:%d: %s: %d\n", __FILE__, __LINE__, phase, (err)); \
		abort(); \
	} })
static void CheckEqual(char *expected, const char *v, size_t n)
{
	if (expected == NULL && v == NULL) {
	} else if (expected != NULL && v != NULL && n == strlen(expected) && memcmp(expected, v, n) == 0) {
		return;
	} else {
		fprintf(stderr, "%s: expected '%s', got '%s'\n", phase, (expected ? expected : "(null)"), (v ? v : "(null"));
		abort();
	}
}

int test_shannondb_put(struct shannon_db *db, struct db_writeoptions *woptions, struct db_readoptions *roptions)
{
	int i;
	int ret = 0;
	int value_len;
	int n = 20000;
	char keybuf[100];
	char valbuf[100];
	char buffer[100];
	char *err = NULL;
	for (i = 0; i < n; i++) {
		memset(keybuf, 0 , 100);
		memset(valbuf, 0 , 100);
		memset(buffer, 0 , 100);
		snprintf(keybuf, sizeof(keybuf), "key%d", i);
		snprintf(valbuf, sizeof(valbuf), "value%d", i);
		shannon_db_put(db, woptions, keybuf, strlen(keybuf), valbuf, strlen(valbuf), &err);
		if (err) {
			printf("add ioctl failed\n");
			exit(-1);
		}
		shannon_db_get(db, roptions, keybuf, strlen(keybuf), buffer, BUF_LEN, &value_len, &err);
		if (err) {
			printf("get ioctl failed\n");
			exit(-1);
		}
		if (strcmp(valbuf, buffer)) {
			printf("dismatch value:%s buffer:%s\n", valbuf, buffer);
			exit(-1);
		}
	}
	return ret;
}

int test_shannondb_delete(struct shannon_db *db, struct db_writeoptions *woptions, struct db_readoptions *roptions)
{
	int i;
	int ret = 0;
	int value_len;
	int n = 20000;
	char keybuf[100];
	char valbuf[100];
	char buffer[100];
	char *err = NULL;
	for (i = 0; i < n; i++) {
		memset(keybuf, 0 , 100);
		memset(valbuf, 0 , 100);
		memset(buffer, 0 , 100);
		snprintf(keybuf, sizeof(keybuf), "key%d", i);
		snprintf(valbuf, sizeof(valbuf), "value%d", i);
		shannon_db_get(db, roptions, keybuf, strlen(keybuf), buffer, BUF_LEN, &value_len, &err);
		if (err) {
			printf("get ioctl failed\n");
			exit(-1);
		}
		if (strcmp(valbuf, buffer)) {
			printf("dismatch value:%s buffer:%s\n", valbuf, buffer);
			exit(-1);
		}
		shannon_db_delete(db, woptions, keybuf, strlen(keybuf), &err);
		if (err) {
			printf("del ioctl failed\n");
			exit(-1);
		}
		shannon_db_get(db, roptions, keybuf, strlen(keybuf), buffer, BUF_LEN, &value_len, &err);
		err = NULL;
		if (value_len > 0) {
			printf("not del  key:%s value:%s len:%d\n", keybuf, buffer, value_len);
			exit(-1);
		}
	}
	return ret;
}
int test_shannondb_modify(struct shannon_db *db, struct db_writeoptions *woptions, struct db_readoptions *roptions)
{
	int i;
	int ret = 0;
	int value_len;
	int n = 20000;
	char keybuf[100];
	char valbuf[100];
	char buffer[100];
	char mvalbuf[100];
	char *err = NULL;
	for (i = 0; i < n; i++) {
		memset(keybuf, 0 , 100);
		memset(valbuf, 0 , 100);
		memset(buffer, 0 , 100);
		snprintf(keybuf, sizeof(keybuf), "key%d", i);
		snprintf(valbuf, sizeof(valbuf), "value%d", i);
		shannon_db_put(db, woptions, keybuf, strlen(keybuf), valbuf, strlen(valbuf), &err);
		if (err) {
			printf("modufy put begin ioctl failed\n");
			exit(-1);
		}
		shannon_db_get(db, roptions, keybuf, strlen(keybuf), buffer, BUF_LEN, &value_len, &err);
		if (err) {
			printf("modify get begin ioctl failed\n");
			exit(-1);
		}
		if (strcmp(valbuf, buffer)) {
			printf("modify begin dismatch value:%s buffer:%s\n", valbuf, buffer);
			exit(-1);
		}
		memset(mvalbuf, 0 , 100);
		memset(buffer, 0 , 100);
		snprintf(mvalbuf, sizeof(mvalbuf), "val%d", i);
		shannon_db_put(db, woptions, keybuf, strlen(keybuf), mvalbuf, strlen(mvalbuf), &err);
		if (err) {
			printf("modify put after ioctl failed\n");
			exit(-1);
		}
		shannon_db_get(db, roptions, keybuf, strlen(keybuf), buffer, BUF_LEN, &value_len, &err);
		if (err) {
			printf("modify get after failed key:%s value:%s len:%d\n", keybuf, buffer, value_len);
			exit(-1);
		}
		if (strcmp(mvalbuf, buffer)) {
			printf("modify after dismatch value:%s buffer:%s\n", mvalbuf, buffer);
			exit(-1);
		}
	}
	return ret;
}


int test_shannondb_case1(int times, struct db_options *options)
{
	struct shannon_db *db;
	char dbname[32];
	int i;
	char *err = NULL;
	for (i = 0; i < times; i++) {
		snprintf(dbname, sizeof(dbname), "shannon%d.db", i);
		db = shannon_db_open(options, "/dev/kvdev0", dbname, &err);
		shannon_destroy_db(db, &err);
	}
}
int test_shannondb_case2(int times, struct db_options *options, struct db_writeoptions *woptions, struct db_readoptions *roptions)
{
	struct shannon_db *db;
	char dbname[32];
	int i, ret = 0;
	char *err = NULL;
	__u64 n = 20000 * times;
	__u64 ts1, ts2;

	ret = shannon_db_get_sequence("/dev/kvdev0", &ts1, &err);
	if (ret < 0 || err) {
		printf("get sequence ioctl failed.\n");
		exit(-1);
	}
	printf("cur_timestamp1=%llu.\n", ts1);
	ts1 += 100;
	ret = shannon_db_set_sequence("/dev/kvdev0", ts1, &err);
	if (ret < 0 || err) {
		printf("set sequence ioctl failed.\n");
		exit(-1);
	}

	for (i = 0; i < times; i++) {
		snprintf(dbname, sizeof(dbname), "shannon%d.db", i);
		db = shannon_db_open(options, "/dev/kvdev0", dbname, &err);
		test_shannondb_put(db, woptions, roptions);
		shannon_destroy_db(db, &err);
	}

	ret = shannon_db_get_sequence("/dev/kvdev0", &ts2, &err);
	if (ret < 0 || err || ((ts2 - ts1) != n)) {
		printf("get sequence ioctl failed.\n");
		exit(-1);
	}
	printf("cur_timestamp2=%llu.\n", ts2);
	ret = shannon_db_set_sequence("/dev/kvdev0", ts2, &err);
	if (ret >= 0 || err == NULL) {
		exit(-1);
	}
}

char *randomstr()
{
	char *buf;
	int len = rand() % 64 + 37;
	int i;
	buf = malloc(len + 1);
	memset(buf, 0, len + 1);
	for (i = 0; i < len; ++i) {
		buf[i] = 'A' + rand() % 26;
	}
	return buf;
}

int test_shannondb_put_randomdata(struct shannon_db *db, struct db_writeoptions *woptions, struct db_readoptions *roptions)
{
	int times = 100;
	int i = 0, ret = 0;
	int value_len = 0;
	char buffer[BUF_LEN];
	char *err = NULL;
	char **value = (char **)malloc(times * sizeof(char *));
	char **key = (char **)malloc(times * sizeof(char *));
	for (i = 0; i < times; i++) {
		value[i] = randomstr();
		key[i] = randomstr();
		shannon_db_put(db, woptions, key[i], strlen(key[i]), value[i], strlen(value[i]), &err);
		if (err) {
			printf("add ioctl failed\n");
			exit(-1);
		}
		memset(buffer, 0, BUF_LEN);
		shannon_db_get(db, roptions, key[i], strlen(key[i]), buffer, BUF_LEN, &value_len, &err);
		if (err) {
			printf("test_writebatch_put get ioctl failed\n");
			exit(-1);
		}
		if (strcmp(value[i], buffer)) {
			printf("dismatch value:%s len %d buffer:%s\n", value[i], value_len, buffer);
			exit(-1);
		}
		free(value[i]);
		free(key[i]);
	}
	free(value);
	free(key);
	return 0;
}

int main()
{
	struct shannon_db *db;
	struct db_options *options;
	struct db_readoptions *roptions;
	struct db_writeoptions *woptions;
	char *err = NULL;
	char *dbname = "test.db";
	StartPhase("create_objects");
	{
		CheckCondition(dbname != NULL);
		options = shannon_db_options_create();
		woptions = shannon_db_writeoptions_create();
		roptions = shannon_db_readoptions_create();
		shannon_db_writeoptions_set_sync(woptions, 1);
		shannon_db_options_set_create_if_missing(options, 1);
	}
	StartPhase("testcase");
	{
		StartPhase("test_shannondb_case1: create and remove database times with empty kvdata");
		test_shannondb_case1(20, options);
		StartPhase("test_shannondb_case2: create and remove database times with kvdata");
		test_shannondb_case2(20, options, woptions, roptions);
		StartPhase("test.db open");
		db = shannon_db_open(options, "/dev/kvdev0", "test.db", &err);
		StartPhase("test_shannondb_put:put and get kvdata, then compare data");
		test_shannondb_put(db, woptions, roptions);
		StartPhase("test_shannondb_put_randomdata:put and get kvdata, then compare data");
		test_shannondb_put_randomdata(db, woptions, roptions);
		StartPhase("test_shannondb_delete:get exits kvdata, then delete, after get data");
		test_shannondb_delete(db, woptions, roptions);
		StartPhase("test_shannondb_modify:put again with different value, then compare data with newvalue");
		test_shannondb_modify(db, woptions, roptions);
	}
	StartPhase("cleanup");
	{
		shannon_destroy_db(db, &err);
		shannon_db_options_destroy(options);
		shannon_db_readoptions_destroy(roptions);
		shannon_db_writeoptions_destroy(woptions);
		shannon_db_close(db);
	}
	fprintf(stderr, "PASS\n");
	return 0;
}
