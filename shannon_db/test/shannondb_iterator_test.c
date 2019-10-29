#include <shannon_db.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
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
		fprintf(stderr, "%s:%d: %s: %d %s\n", __FILE__, __LINE__, phase, (err)); \
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
static void CheckArrayEmpty(char *array)
{
	if ((array[0] == '\0') || strlen(array) == 0) {
		return;
	}
	printf("buffer:%s len:%ld", array, strlen(array));
	abort();
}

int test_iter_forward_kv(struct shannon_db_iterator *iter)
{
	char keybuf[128];
	char valbuf[128];
	int keylen;
	int valuelen;
	shannon_db_iter_seek_to_first(iter);
	while (shannon_db_iter_valid(iter)) {
		memset(keybuf, 0, 128);
		memset(valbuf, 0, 128);
		shannon_db_iter_cur_key(iter, keybuf, 128, &keylen);
		shannon_db_iter_cur_value(iter, valbuf, 128, &valuelen);
		printf("key:%s len:%d val:%s len:%d\n", keybuf, keylen, valbuf, valuelen);
		shannon_db_iter_move_next(iter);
	}
}
int test_iter_clear_kv(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	char keybuf[128];
	char valbuf[128];
	int keylen;
	int valuelen;
	char *err = NULL;
	struct shannon_db_iterator *iter = shannon_db_create_iterator(db, roptions);
	shannon_db_iter_seek_to_first(iter);
	while (shannon_db_iter_valid(iter)) {
		memset(keybuf, 0, 128);
		memset(valbuf, 0, 128);
		shannon_db_iter_cur_key(iter, keybuf, 128, &keylen);
		shannon_db_iter_cur_value(iter, valbuf, 128, &valuelen);
		shannon_db_delete(db, woptions, keybuf, keylen, &err);
		if (err != NULL) {
			printf("test_iter_clear_kv shannon_db_delete failed\n");
			exit(-1);
		}
		shannon_db_iter_move_next(iter);
	}
	shannon_db_iter_destroy(iter);
}
int test_iter_get_count(struct shannon_db_iterator *iter)
{
	__u64 count = 0;
	shannon_db_iter_seek_to_first(iter);
	while (shannon_db_iter_valid(iter)) {
		count++;
		shannon_db_iter_move_next(iter);
	}
	return count;
}
int test_iter_reverse_kv(struct shannon_db_iterator *iter)
{
	char keybuf[128];
	char valbuf[128];
	int keylen;
	int valuelen;
	shannon_db_iter_seek_to_last(iter);
	while (shannon_db_iter_valid(iter)) {
		memset(keybuf, 0, 128);
		memset(valbuf, 0, 128);
		shannon_db_iter_cur_key(iter, keybuf, 128, &keylen);
		shannon_db_iter_cur_value(iter, valbuf, 128, &valuelen);
		printf("key:%s len:%d val:%s len:%d\n", keybuf, keylen, valbuf, valuelen);
		shannon_db_iter_move_prev(iter);
	}
}

int test_iter_empty(struct shannon_db *db, struct db_readoptions *roptions)
{
	char buffer[128];
	int keylen;
	int valuelen;
	struct shannon_db_iterator *iter = shannon_db_create_iterator(db, roptions);
	memset(buffer, 0, 128);
	shannon_db_iter_seek_to_first(iter);
	shannon_db_iter_cur_key(iter, buffer, 128, &keylen);
	CheckArrayEmpty(buffer);
	memset(buffer, 0, 128);
	shannon_db_iter_cur_value(iter, buffer, 128, &valuelen);
	CheckArrayEmpty(buffer);
	memset(buffer, 0, 128);
	shannon_db_iter_seek_to_last(iter);
	shannon_db_iter_cur_key(iter, buffer, 128, &keylen);
	CheckArrayEmpty(buffer);
	memset(buffer, 0, 128);
	shannon_db_iter_cur_value(iter, buffer, 128, &valuelen);
	CheckArrayEmpty(buffer);
	shannon_db_iter_destroy(iter);
}

/*
 * first(key1--value1)-->move->next/prev-->buffer null
 * last(key1--value1)-->move->next/prev-->buffer null
 * seek(key1--value1)-->move->next/prev-->buffer null
 * seek(key2)-->buffer null
*/
int test_iter_single(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	char buffer[128];
	int keylen;
	int valuelen;
	char *err = NULL;
	struct shannon_db_iterator *iter;
	StartPhase("put");
	shannon_db_put(db, woptions, "key1", 4, "value1", 6, &err);
	if (err != NULL) {
		printf("test_iter_single shannon_db_put failed\n");
		exit(-1);
	}
	memset(buffer, 0, 128);
	iter = shannon_db_create_iterator(db, roptions);
	shannon_db_iter_seek_to_first(iter);
	if (shannon_db_iter_valid(iter)) {
		shannon_db_iter_cur_key(iter, buffer, 128, &keylen);
		CheckEqual("key1", buffer, keylen);
		memset(buffer, 0, 128);
		shannon_db_iter_cur_value(iter, buffer, 128, &valuelen);
		CheckEqual("value1", buffer, valuelen);
		memset(buffer, 0, 128);
	}
	shannon_db_iter_move_next(iter);
	if (shannon_db_iter_valid(iter)) {
		shannon_db_iter_cur_key(iter, buffer, 128, &keylen);
		CheckArrayEmpty(buffer);
		memset(buffer, 0, 128);
		shannon_db_iter_cur_value(iter, buffer, 128, &valuelen);
		CheckArrayEmpty(buffer);
	}
	shannon_db_iter_seek_to_last(iter);
	if (shannon_db_iter_valid(iter)) {
		memset(buffer, 0, 128);
		shannon_db_iter_cur_key(iter, buffer, 128, &keylen);
		CheckEqual("key1", buffer, keylen);
		memset(buffer, 0, 128);
		shannon_db_iter_cur_value(iter, buffer, 128, &valuelen);
		CheckEqual("value1", buffer, valuelen);
	}
	shannon_db_iter_move_next(iter);
	if (shannon_db_iter_valid(iter)) {
		memset(buffer, 0, 128);
		shannon_db_iter_cur_key(iter, buffer, 128, &keylen);
		CheckArrayEmpty(buffer);
		memset(buffer, 0, 128);
		shannon_db_iter_cur_value(iter, buffer, 128, &valuelen);
		CheckArrayEmpty(buffer);
	}

	shannon_db_iter_seek(iter, "key1", 4);
	if (shannon_db_iter_valid(iter)) {
		memset(buffer, 0, 128);
		shannon_db_iter_cur_key(iter, buffer, 128, &keylen);
		CheckEqual("key1", buffer, keylen);
		memset(buffer, 0, 128);
		shannon_db_iter_cur_value(iter, buffer, 128, &valuelen);
		CheckEqual("value1", buffer, valuelen);
	}
	memset(buffer, 0, 128);
	shannon_db_iter_move_prev(iter);
	shannon_db_iter_cur_key(iter, buffer, 128, &keylen);
	CheckArrayEmpty(buffer);
	memset(buffer, 0, 128);
	shannon_db_iter_cur_value(iter, buffer, 128, &valuelen);
	CheckArrayEmpty(buffer);

	shannon_db_iter_seek(iter, "key1", 4);
	memset(buffer, 0, 128);
	shannon_db_iter_cur_key(iter, buffer, 128, &keylen);
	CheckEqual("key1", buffer, keylen);
	memset(buffer, 0, 128);
	shannon_db_iter_cur_value(iter, buffer, 128, &valuelen);
	CheckEqual("value1", buffer, valuelen);

	memset(buffer, 0, 128);
	shannon_db_iter_move_next(iter);
	shannon_db_iter_cur_key(iter, buffer, 128, &keylen);
	CheckArrayEmpty(buffer);
	memset(buffer, 0, 128);
	shannon_db_iter_cur_value(iter, buffer, 128, &valuelen);
	CheckArrayEmpty(buffer);
	StartPhase("delete");
	test_iter_forward_kv(iter);
	shannon_db_delete(db, woptions, "key1", 4, &err);
	if (err != NULL) {
		printf("test_iter_single shannon_db_delete failed\n");
		exit(-1);
	}
	shannon_db_iter_destroy(iter);
}

int test_iter_multi(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	char keybuf[128];
	char valbuf[128];
	int keylen;
	int valuelen;
	char *err = NULL;
	struct shannon_db_iterator *iter;
	shannon_db_put(db, woptions, "key2", 4, "value2", 6, &err);
	if (err != NULL) {
		printf("test_iter_multi shannon_db_put 1 failed\n");
		exit(-1);
	}
	shannon_db_put(db, woptions, "key3", 4, "value3", 6, &err);
	if (err != NULL) {
		printf("test_iter_multi shannon_db_put 2 failed\n");
		exit(-1);
	}
	shannon_db_put(db, woptions, "key4", 4, "value4", 6, &err);
	if (err != NULL) {
		printf("test_iter_multi shannon_db_put 3 failed\n");
		exit(-1);
	}
	iter = shannon_db_create_iterator(db, roptions);
	test_iter_forward_kv(iter);
	shannon_db_delete(db, woptions, "key2", 4, &err);
	if (err != NULL) {
		printf("test_iter_multi shannon_db_delete 1 failed\n");
		exit(-1);
	}
	shannon_db_delete(db, woptions, "key3", 4, &err);
	if (err != NULL) {
		printf("test_iter_multi shannon_db_delete 2 failed\n");
		exit(-1);
	}
	shannon_db_delete(db, woptions, "key4", 4, &err);
	if (err != NULL) {
		printf("test_iter_multi shannon_db_delete 3 failed\n");
		exit(-1);
	}
	shannon_db_iter_destroy(iter);
}
int test_iter_multi_with_delete(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	char keybuf[128];
	char valbuf[128];
	int keylen;
	int valuelen;
	char *err = NULL;
	struct shannon_db_iterator *iter;
	shannon_db_put(db, woptions, "key5", 4, "value5", 6, &err);
	if (err != NULL) {
		printf("test_iter_multi_with_delete shannon_db_put 1 failed\n");
		exit(-1);
	}
	shannon_db_put(db, woptions, "key6", 4, "value6", 6, &err);
	if (err != NULL) {
		printf("test_iter_multi_with_delete shannon_db_put 2 failed\n");
		exit(-1);
	}
	shannon_db_put(db, woptions, "key7", 4, "value7", 6, &err);
	if (err != NULL) {
		printf("test_iter_multi_with_delete shannon_db_put 3 failed\n");
		exit(-1);
	}
	shannon_db_delete(db, woptions, "key5", 4, &err);
	if (err != NULL) {
		printf("test_iter_multi_with_delete shannon_db_delete 1 failed\n");
		exit(-1);
	}
	iter = shannon_db_create_iterator(db, roptions);
	shannon_db_iter_seek_to_first(iter);
	if (shannon_db_iter_valid(iter)) {
		memset(keybuf, 0, 128);
		memset(valbuf, 0, 128);
		shannon_db_iter_cur_key(iter, keybuf, 128, &keylen);
		shannon_db_iter_cur_value(iter, valbuf, 128, &valuelen);
		CheckEqual("key6", keybuf, keylen);
		CheckEqual("value6", valbuf, valuelen);
	}

	shannon_db_iter_seek(iter, "key5", 4);
	memset(keybuf, 0, 128);
	memset(valbuf, 0, 128);
	shannon_db_iter_cur_key(iter, keybuf, 128, &keylen);
	CheckEqual("key6", keybuf, keylen);
	shannon_db_iter_cur_value(iter, valbuf, 128, &valuelen);
	CheckEqual("value6", valbuf, valuelen);

	shannon_db_iter_move_next(iter);
	memset(keybuf, 0, 128);
	memset(valbuf, 0, 128);
	shannon_db_iter_cur_key(iter, keybuf, 128, &keylen);
	CheckEqual("key7", keybuf, keylen);
	shannon_db_iter_cur_value(iter, valbuf, 128, &valuelen);
	CheckEqual("value7", valbuf, valuelen);
	shannon_db_iter_destroy(iter);
}
int test_iter_case1(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int loop_times = 100;
	int i;
	int ret;
	struct shannon_db_iterator *iter;
	for (i = 0; i < loop_times; i++) {
		iter = shannon_db_create_iterator(db, roptions);
		if (!iter) {
			printf("test_iterator_case1 create_iterator--failed!!!.\n");
			exit(-1);
		}
		shannon_db_iter_destroy(iter);
	}
}

int test_iter_prefix(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	char keybuf[128];
	char valbuf[128];
	int keylen;
	int valuelen;
	char search_key[128];
	char *err = NULL;
	struct shannon_db_iterator *iter;
	shannon_db_put(db, woptions, "aaa", 3, "value1", 6, &err);
	shannon_db_put(db, woptions, "aab", 3, "value2", 6, &err);
	shannon_db_put(db, woptions, "aac", 3, "value3", 6, &err);
	shannon_db_put(db, woptions, "bba", 3, "value4", 6, &err);
	shannon_db_put(db, woptions, "bbb", 3, "value5", 6, &err);
	shannon_db_put(db, woptions, "bbc", 3, "value6", 6, &err);
	shannon_db_put(db, woptions, "cca", 3, "value7", 6, &err);
	shannon_db_put(db, woptions, "ccb", 3, "value8", 6, &err);
	shannon_db_put(db, woptions, "ccc", 3, "value9", 6, &err);
	sprintf(search_key, "%c%c%c", 0xFF, 'a', 'b');
	shannon_db_put(db, woptions, search_key, 3, "valueA", 6, &err);
	sprintf(search_key, "%c%c%c", 0xFF, 0xFF, 'b');
	shannon_db_put(db, woptions, search_key, 3, "valueB", 6, &err);
	sprintf(search_key, "%c%c%c", 0xFF, 0xFF, 0xFF);
	shannon_db_put(db, woptions, search_key, 3, "valueC", 6, &err);
	iter = shannon_db_create_prefix_iterator(db, roptions, "bb", 2);
	test_iter_forward_kv(iter);
	shannon_db_iter_seek_to_first(iter);
	memset(keybuf, 0, 128);
	memset(valbuf, 0, 128);
	shannon_db_iter_cur_key(iter, keybuf, 128, &keylen);
	CheckEqual("bba", keybuf, keylen);
	shannon_db_iter_cur_value(iter, valbuf, 128, &valuelen);
	CheckEqual("value4", valbuf, valuelen);
	shannon_db_iter_move_next(iter);
	memset(keybuf, 0, 128);
	memset(valbuf, 0, 128);
	shannon_db_iter_cur_key(iter, keybuf, 128, &keylen);
	CheckEqual("bbb", keybuf, keylen);
	shannon_db_iter_cur_value(iter, valbuf, 128, &valuelen);
	CheckEqual("value5", valbuf, valuelen);
	shannon_db_iter_seek_to_last(iter);
	memset(keybuf, 0, 128);
	memset(valbuf, 0, 128);
	shannon_db_iter_cur_key(iter, keybuf, 128, &keylen);
	CheckEqual("bbc", keybuf, keylen);
	shannon_db_iter_cur_value(iter, valbuf, 128, &valuelen);
	CheckEqual("value6", valbuf, valuelen);
	shannon_db_iter_destroy(iter);

	// test seek_to_last

	// "aa" -> "aac"
	iter = shannon_db_create_prefix_iterator(db, roptions, "aa", 2);

	shannon_db_iter_seek_to_last(iter);
	assert(iter->valid != 0);

	memset(keybuf, 0, 128);
	memset(valbuf, 0, 128);

	shannon_db_iter_cur_key(iter, keybuf, 128, &keylen);
	CheckEqual("aac", keybuf, keylen);

	shannon_db_iter_destroy(iter);


	// "aab" -> "aab"
	iter = shannon_db_create_prefix_iterator(db, roptions, "aab", 3);

	shannon_db_iter_seek_to_last(iter);
	assert(iter->valid != 0);

	memset(keybuf, 0, 128);
	memset(valbuf, 0, 128);

	shannon_db_iter_cur_key(iter, keybuf, 128, &keylen);
	CheckEqual("aab", keybuf, keylen);

	shannon_db_iter_destroy(iter);

	// "aac" -> "aac"
	iter = shannon_db_create_prefix_iterator(db, roptions, "aac", 3);

	shannon_db_iter_seek_to_last(iter);
	assert(iter->valid != 0);

	memset(keybuf, 0, 128);
	memset(valbuf, 0, 128);

	shannon_db_iter_cur_key(iter, keybuf, 128, &keylen);
	CheckEqual("aac", keybuf, keylen);

	shannon_db_iter_destroy(iter);

	// "NONE" -> 不存在
	iter = shannon_db_create_prefix_iterator(db, roptions, "NONE", 4);

	shannon_db_iter_seek_to_last(iter);
	assert(iter->valid == 0);

	shannon_db_iter_destroy(iter);

	// "aa\x60" -> 不存在，'a'为'\x61
	iter = shannon_db_create_prefix_iterator(db, roptions, "aa\x60", 3);

	shannon_db_iter_seek_to_last(iter);
	assert(iter->valid == 0);

	shannon_db_iter_destroy(iter);

	// "\xFF" -> "\xFF\xFF\xFF"
	iter = shannon_db_create_prefix_iterator(db, roptions, "\xFF", 1);

	shannon_db_iter_seek_to_last(iter);
	assert(iter->valid != 0);

	memset(keybuf, 0, 128);
	memset(valbuf, 0, 128);

	shannon_db_iter_cur_key(iter, keybuf, 128, &keylen);
	CheckEqual("\xFF\xFF\xFF", keybuf, keylen);

	shannon_db_iter_destroy(iter);
}

int main()
{
	struct shannon_db *db;
	struct db_options *options;
	struct db_readoptions *roptions;
	struct db_writeoptions *woptions;
	char *dbname = "test.db";
	char buffer[128];
	int value_len;
	__u64 count;
	char *err = NULL;
	StartPhase("create_objects");
	{
		CheckCondition(dbname != NULL);
		options = shannon_db_options_create();
		woptions = shannon_db_writeoptions_create();
		roptions = shannon_db_readoptions_create();
		shannon_db_writeoptions_set_sync(woptions, 1);
		shannon_db_options_set_create_if_missing(options, 1);
		StartPhase("open");
		db = shannon_db_open(options, "/dev/kvdev0", "test.db", &err);
		if (err != NULL) {
			printf("open_db failed\n");
			exit(-1);
		}
		StartPhase("destroy");
		shannon_destroy_db(db, &err);
		if (err != NULL) {
			printf("destroy_db failed\n");
			exit(-1);
		}
		StartPhase("open");
		db = shannon_db_open(options, "/dev/kvdev0", "test.db", &err);
		if (err != NULL) {
			printf("open_db failed\n");
			exit(-1);
		}
	}
	StartPhase("testcase");
	{
		StartPhase("test_iter_case1 create and destroy iter multi times");
		test_iter_case1(db, roptions, woptions);
		StartPhase("test_iter_empty checkout iter with empty data");
		test_iter_empty(db, roptions);
		StartPhase("test_iter_single checkout iter with single data");
		test_iter_single(db, roptions, woptions);
		StartPhase("test_prefix_iter checkout iter with data");
		test_iter_prefix(db, roptions, woptions);
		StartPhase("test_iter_clearkv delete all kv");
		test_iter_clear_kv(db, roptions, woptions);
		StartPhase("test_iter_multi checkout iter with multi data");
		test_iter_multi(db, roptions, woptions);
		StartPhase("test_iter_multi_with_delete  checkout iter after delete");
		test_iter_multi_with_delete(db, roptions, woptions);
	}
	StartPhase("cleanup");
	{
		shannon_destroy_db(db, &err);
		if (err != NULL) {
			printf("open_column_family failed\n");
			exit(-1);
		}
		shannon_db_options_destroy(options);
		shannon_db_readoptions_destroy(roptions);
		shannon_db_writeoptions_destroy(woptions);
		shannon_db_close(db);
	}
	fprintf(stderr, "PASS\n");
	return 0;
}
