#include <shannon_db.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define BUF_LEN 128

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
		fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, phase, (err)); \
		abort(); \
	} })
static void CheckEqual(char *expected, char *v, size_t n)
{
	if (expected == NULL && v == NULL) {
	} else if (expected != NULL && v != NULL && n == strlen(expected) && memcmp(expected, v, n) == 0) {
		return;
	} else {
		fprintf(stderr, "%s: expected '%s', got '%s'\n", phase, (expected ? expected : "(null)"), (v ? v : "(null"));
		abort();
	}
}

static void CheckGetCF(struct shannon_db *db, db_readoptions_t *options, shannon_cf_handle_t *handle,
		char *key, char *expected) {
	char *err = NULL;
	unsigned int val_len = 0;
	char valbuf[BUF_LEN];
	shannon_db_get_cf(db, options, handle, key, strlen(key), valbuf, BUF_LEN, &val_len, &err);
	if (expected == NULL) {
		if (val_len > 0) {
			fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, phase, (err));
			abort();
		}
		return;
	}
	CheckNoError(err);
	CheckEqual(expected, valbuf, val_len);
}

int test_shannondb_put_cf(struct shannon_db *db, db_writeoptions_t *woptions, const db_readoptions_t *roptions, struct shannon_cf_handle *column_family, int n)
{
	int i;
	int ret;
	int value_len;
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
		shannon_db_put_cf(db, woptions, column_family, keybuf, strlen(keybuf), valbuf, strlen(valbuf), &err);
		if (err != NULL) {
			printf("add ioctl failed\n");
			exit(-1);
		}
		shannon_db_get_cf(db, roptions, column_family, keybuf, strlen(keybuf), buffer, BUF_LEN, &value_len, &err);
		if (err != NULL) {
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
void print_key_value(char *key, int key_len, char *value, int value_len)
{
	char tkey[256];
	char tvalue[256];

	strncpy(tkey, key, key_len);
	strncpy(tvalue, value, value_len);
	tkey[key_len] = '\0';
	tvalue[value_len] = '\0';
	printf("%s:%s\n", tkey, tvalue);
}

int test_prefixiter_firstandnext(struct shannon_db_iterator *iter)
{
	int key_len = 0;
	int value_len = 0;
	int i = 0;
	char value[1024];
	char key[1024];

	for (shannon_db_iter_seek_to_first(iter); shannon_db_iter_valid(iter); \
					shannon_db_iter_move_next(iter)) {
		key_len = 0;
		value_len = 0;
		shannon_db_iter_cur_key(iter, key, 1024, &key_len);
		shannon_db_iter_cur_value(iter, value, 1024, &value_len);
		key[key_len] = '\0';
		value[value_len] = '\0';
		print_key_value(key, key_len, value, value_len);
		if (strncmp(iter->prefix, key, iter->prefix_len)) {
			printf("prefix error of key:%s\n", key);
			break;
		}
		i++;
	}
	printf("count:%d\n", i);
	printf("--------------------\n");
	return 0;
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
		if (err) {
			printf("get ioctl failed\n");
			exit(-1);
		}
		if (value_len > 0) {
			printf("not del  key:%s value:%s len:%d\n", keybuf, buffer, value_len);
			exit(-1);
		}
	}
	return ret;
}
int main()
{
	struct shannon_db *db;
	struct shannon_db *column_db;
	struct db_options *options;
	db_options_t *cf_options = NULL;
	struct shannon_cf_handle *cfh1 = NULL;
	struct shannon_cf_handle *cfh2 = NULL;
	struct db_readoptions *roptions;
	struct db_writeoptions *woptions;
	struct shannon_db_iterator *iterators;
	struct shannon_db_iterator *iter1;
	struct shannon_db_iterator *iter2;
	struct shannon_db_iterator *piter1;
	char *dbname = "test.db";
	char *devname = "/dev/kvdev0";
	int cf_count = 0;
	int i;
	char *err = NULL;
	char *err1 = NULL;
	char **column_names = NULL;
	options = shannon_db_options_create();
	woptions = shannon_db_writeoptions_create();
	roptions = shannon_db_readoptions_create();
	cf_options = shannon_db_options_create();
	shannon_db_writeoptions_set_sync(woptions, 1);
	shannon_db_options_set_create_if_missing(options, 1);
	StartPhase("columnfamilies");
	db = shannon_db_open(options, "/dev/kvdev0", "test.db", &err);
	if (db == NULL) {
		printf("shannon_db_open ioctl failed!!!\n");
		exit(EXIT_FAILURE);
	}
	shannon_destroy_db(db, &err);
	shannon_db_close(db);
	CheckNoError(err);

	db = shannon_db_open(options, "/dev/kvdev0", "test.db", &err);
	CheckNoError(err);
	shannon_cf_handle_t *cfh;
	cfh = shannon_db_create_column_family(db, options, "cf1", &err);
	shannon_db_column_family_handle_destroy(cfh);
	CheckNoError(err);
	shannon_db_close(db);

	unsigned int cflen = 0;
	char **column_fams = shannon_db_list_column_families(options, devname, dbname, &cflen, &err);
	CheckNoError(err);
	CheckEqual("default", column_fams[0], 7);
	CheckEqual("cf1", column_fams[1], 3);
	CheckCondition((cflen == 2));
	shannon_db_list_column_families_destroy(column_fams, cflen);
	const char *cf_names[2] = {"default", "cf1"};
	const struct db_options *cf_opts[2] = {cf_options, cf_options};
	int num_column_families = 2;
	int num;
	shannon_cf_handle_t **handles = (shannon_cf_handle_t **) malloc (sizeof (shannon_cf_handle_t *) * num_column_families);
	for (num = 0; num < num_column_families; num++) {
		handles[num] = (shannon_cf_handle_t *) malloc (sizeof (shannon_cf_handle_t));
	}
	db = shannon_db_open_column_families(options, devname, dbname, 2, cf_names, cf_opts, handles, &err);
	CheckNoError(err);
	shannon_db_put_cf(db, woptions, handles[1], "foo", 3, "hello", 5, &err);
	CheckNoError(err);

	CheckGetCF(db, roptions, handles[1], "foo", "hello");

	shannon_db_delete_cf(db, woptions, handles[1], "foo", 3, &err);
	CheckNoError(err);

	CheckGetCF(db, roptions, handles[1], "foo", NULL);
	db_writebatch_t *wb = shannon_db_writebatch_create();
	shannon_db_writebatch_put_cf(wb, handles[1], "baz", 3, "a", 1, &err);
	shannon_db_writebatch_clear(wb);
	shannon_db_writebatch_put_cf(wb, handles[1], "bar", 3, "b", 1, &err);
	shannon_db_writebatch_put_cf(wb, handles[1], "box", 3, "c", 1, &err);
	shannon_db_writebatch_delete_cf(wb, handles[1], "bar", 3, &err);
	shannon_db_write(db, woptions, wb, &err);
	CheckNoError(err);
	CheckGetCF(db, roptions, handles[1], "baz", NULL);
	CheckGetCF(db, roptions, handles[1], "bar", NULL);
	CheckGetCF(db, roptions, handles[1], "box", "c");
	shannon_db_writebatch_destroy(wb);
	struct shannon_db_iterator *iter = shannon_db_create_iterator_cf(db, roptions, handles[1]);
	CheckCondition(!shannon_db_iter_valid(iter));
	shannon_db_iter_seek_to_first(iter);
	CheckCondition(shannon_db_iter_valid(iter));

	for (i = 0; shannon_db_iter_valid(iter) != 0; shannon_db_iter_move_next(iter))
		i++;
	CheckCondition(i == 1);
	shannon_db_iter_get_error(iter, &err);
	CheckNoError(err);
	shannon_db_iter_destroy(iter);
	//shannon_db_free_iter(iter);
	shannon_cf_handle_t *iters_cf_handles[2] = { handles[0], handles[1] };
	struct shannon_db_iterator **iters_handles = (struct shannon_db_iterator **) malloc (sizeof(struct shannon_db_iterator *) * 2);
	for (num = 0; num < 2; num++)
		iters_handles[num] = (struct shannon_db_iterator *) malloc (sizeof (struct shannon_db_iterator));
	shannon_db_create_iterators(db, roptions, iters_cf_handles, iters_handles, 2, &err);
	CheckNoError(err);

	iter1 = iters_handles[0];
	CheckCondition(!shannon_db_iter_valid(iter1));
	shannon_db_iter_seek_to_first(iter1);
	CheckCondition(!shannon_db_iter_valid(iter1));

	iter2 = iters_handles[1];
	CheckCondition(!shannon_db_iter_valid(iter2));
	shannon_db_iter_seek_to_first(iter2);
	CheckCondition(shannon_db_iter_valid(iter2));
	for (i = 0; shannon_db_iter_valid(iter2) != 0; shannon_db_iter_move_next(iter2))
		i++;
	CheckCondition(i == 1);
	shannon_db_iter_get_error(iter2, &err);
	CheckNoError(err);
	printf("iter1-->db_index--cf_index--iter_index--count[%d--%d--%d--%d]\n", iter1->db.db_index, iter1->cf_index, iter1->iter_index, test_iter_get_count(iter1));
	printf("iter2-->db_index--cf_index--iter_index--count[%d--%d--%d--%d]\n", iter2->db.db_index, iter2->cf_index, iter2->iter_index, test_iter_get_count(iter2));
	shannon_db_iter_destroy(iter1);
	shannon_db_iter_destroy(iter2);
	for (num = 0; num < 2; num++) {
		//free(iters_handles[num]);
		iters_handles = NULL;
	}
	free(iters_handles);
	for (i = 0; i < num_column_families; i++) {
		shannon_db_put_cf(db, woptions, handles[i], "aaa", 3, "value1", 6, &err);
		shannon_db_put_cf(db, woptions, handles[i], "aab", 3, "value2", 6, &err);
		shannon_db_put_cf(db, woptions, handles[i], "aac", 3, "value3", 6, &err);
		shannon_db_put_cf(db, woptions, handles[i], "bba", 3, "value4", 6, &err);
		shannon_db_put_cf(db, woptions, handles[i], "bbb", 3, "value5", 6, &err);
		shannon_db_put_cf(db, woptions, handles[i], "bbc", 3, "value6", 6, &err);
		shannon_db_put_cf(db, woptions, handles[i], "cca", 3, "value7", 6, &err);
		shannon_db_put_cf(db, woptions, handles[i], "ccb", 3, "value8", 6, &err);
		shannon_db_put_cf(db, woptions, handles[i], "ccc", 3, "value9", 6, &err);
	}
	struct shannon_db_iterator **prefix_iterators = (struct shannon_db_iterator **) malloc (sizeof(struct shannon_db_iterator *) * 2);
	for (num = 0; num < 2; num++) {
		prefix_iterators[num] = (struct shannon_db_iterator *) malloc (sizeof (struct shannon_db_iterator));
	}
	shannon_db_create_prefix_iterators(db, roptions, iters_cf_handles, prefix_iterators, 2, "bb", 2, &err);
	piter1 = prefix_iterators[1];
	printf("prefix_iterators-->cf_index--iter_index--prefix[%d-%d-%s]\n", piter1->cf_index, piter1->iter_index, piter1->prefix);
	test_prefixiter_firstandnext(piter1);
	shannon_db_iter_destroy(piter1);
	for (num = 0; num < 2; num++) {
		//free(prefix_iterators[num]);
		prefix_iterators[num] = NULL;
	}
	free(prefix_iterators);
	shannon_db_options_destroy(options);
	shannon_db_options_destroy(cf_options);
	shannon_db_readoptions_destroy(roptions);
	shannon_db_writeoptions_destroy(woptions);
	for (i = 0; i < num_column_families; i++) {
		free(handles[i]);
		handles[i] = NULL;
	}
	free(handles);
	shannon_destroy_db(db, &err);
	shannon_db_close(db);
	return 0;
}
