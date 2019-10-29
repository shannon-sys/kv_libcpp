#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "shannon_db.h"
char *phase = "";
#define BUF_LEN 128
static int gkeylen;
static char gkey[256];
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

int gen_key_value(char *key, int *key_len, char *value, int *value_len, int position)
{
	int i = 0;
	int flage = 0;
	int ret = 0;

	for (i = gkeylen - 1; i >= 0; i--) {
		if (i == gkeylen - 1)
			gkey[i] = gkey[i] + 1;
		else
			gkey[i] = gkey[i] + flage;
		if (position == i)
			gkey[i] = gkey[i] + 1;
		if (gkey[i] > 'z') {
			flage = 1;
			gkey[i] = 'a';
		} else {
			flage = 0;
			break;
		}

	}
	strncpy(key, gkey, gkeylen);
	strncpy(value, gkey, gkeylen);
	*key_len = gkeylen;
	*value_len = gkeylen;
	if (flage == 1)
		ret = -1;
	return ret;
}

void batch_add_key_value(struct shannon_db *db, int postion)
{
	struct db_writebatch_t *batch = shannon_db_writebatch_create();
	struct db_writeoptions *options = shannon_db_writeoptions_create();
	int ret = 0;
	int i;
	char key[256];
	char value[100][256];
	int key_len = 0;
	int value_len = 0;
	char *err = NULL;
	while (1) {
		ret = gen_key_value(key, &key_len, value[i], &value_len, postion);
		if (ret != 0)
			break;
		shannon_db_writebatch_put(batch, key, key_len, value[i], value_len, &err);
		if (err != NULL)
			printf("put error\n");
		i++;
		if (i == 100) {
			shannon_db_write(db, options, batch, &err);
			if (err != NULL)
				printf("shannon_db_write put error\n");
			shannon_db_writebatch_clear(batch);
			i = 0;
		}
	}
	if (i != 100) {
		shannon_db_write(db, options, batch, &err);
		if (err != NULL)
			printf("shannon_db_write put error\n");
	}
	shannon_db_writeoptions_destroy(options);
	shannon_db_writebatch_destroy(batch);
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
	StartPhase("test_prefixiter_firstandnext");
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

int test_prefixiter_lastandprev(struct shannon_db_iterator *iter)
{
	StartPhase("test_prefixiter_lastandprev");
	int key_len = 0;
	int value_len = 0;
	int i = 0;
	char value[1024];
	char key[1024];

	for (shannon_db_iter_seek_to_last(iter); shannon_db_iter_valid(iter); \
					shannon_db_iter_move_prev(iter)) {
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

int test_iter_noprefix_reverse_order(struct shannon_db_iterator *iter)
{
	StartPhase("test_iter_noprefix_reverse_order");
	int key_len = 0;
	int value_len = 0;
	int i = 0;
	char value[1024];
	char key[1024];

	for (shannon_db_iter_seek_to_last(iter); shannon_db_iter_valid(iter); \
					shannon_db_iter_move_prev(iter)) {
		key_len = 0;
		value_len = 0;
		shannon_db_iter_cur_key(iter, key, 1024, &key_len);
		shannon_db_iter_cur_value(iter, value, 1024, &value_len);
		key[key_len] = '\0';
		value[value_len] = '\0';
		print_key_value(key, key_len, value, value_len);
		i++;
	}
	printf("count:%d\n", i);
	printf("--------------------\n");
	return 0;
}

int test_iter_noprefix_order(struct shannon_db_iterator *iter)
{
	StartPhase("test_iter_noprefix_order");
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
		i++;
	}
	printf("count:%d\n", i);
	printf("--------------------\n");
	return 0;
}

void test_init_key(void)
{
	int i = 0;

	for (i = 0; i < gkeylen; i++)
		gkey[i] = 'a';
	gkey[i - 1] = gkey[i - 1] - 1;
}

void set_no_interval_key(struct shannon_db *db)
{
	int keylen = 0;

	while (keylen <= 0 || keylen > 5) {
		printf("input key_len(1-5):");
		scanf("%d", &keylen);
	}
	gkeylen = keylen;
	test_init_key();
	batch_add_key_value(db, -1);
	printf("-----add end-----\n");
}

void set_interval_key(struct shannon_db *db)
{
	int keylen = 0;
	int position = 0;

	while (keylen <= 0 || keylen > 5) {
		printf("input key_len(1-5):");
		scanf("%d", &keylen);
	}
	while (position <= 0 || position > keylen) {
		printf("input position(1-%d):", keylen);
		scanf("%d", &position);
	}
	gkeylen = keylen;
	test_init_key();
	batch_add_key_value(db, position - 1);
	printf("-----add end-----\n");
}

int main(int argc, char **argv)
{
	char *err = NULL;
	struct shannon_db *db = NULL;
	struct db_options *options;
	struct db_readoptions *roptions;
	struct db_writeoptions *woptions;
	struct shannon_db_iterator *iter = NULL;
	struct shannon_db_iterator *iter1 = NULL;
	struct shannon_db_iterator *prefixiter = NULL;
	char pre[255];
	int i = 0;
	int opt = 1;
	char prefix[256];
	char *dbname = "testdb";

	prefix[0] = '\0';
	StartPhase("create_objects");
	CheckCondition(dbname != NULL);
	options = shannon_db_options_create();
	woptions = shannon_db_writeoptions_create();
	roptions = shannon_db_readoptions_create();
	shannon_db_writeoptions_set_sync(woptions, 1);
	shannon_db_options_set_create_if_missing(options, 1);
	StartPhase("open");
	db = shannon_db_open(options, "/dev/kvdev0", dbname, &err);
	if ((db == NULL) || (err != NULL))
		printf("open db error!\n");
	while (1) {
		printf("[1]set no-intervel key.\n");
		printf("[2]set intervel key.\n");
		printf("[3]get prefix key_value-order.\n");
		printf("[4]get prefix key_value-reverse order.\n");
		printf("[5]get key_value order.\n");
		printf("[6]get key_value-reverse order.\n");
		printf("[7]exit.\n");
		printf("input index:");
		scanf("%d", &opt);
		if (opt == 7)
			break;
		switch (opt) {
		case 1:
			shannon_destroy_db(db, &err);
			if (err != NULL)
				printf("case 1 destroy_db error!!!\n");
			db = shannon_db_open(options, "/dev/kvdev0", dbname, &err);
			if ((db == NULL) || (err != NULL))
				printf("case 1 open db error!\n");
			set_no_interval_key(db);
			break;
		case 2:
			shannon_destroy_db(db, &err);
			if (err != NULL)
				printf("case 2 destroy_db error!!!\n");
			db = shannon_db_open(options, "/dev/kvdev0", dbname, &err);
			if ((db == NULL) || (err != NULL))
				printf("case 2 open db error!\n");
			set_interval_key(db);
			break;
		case 3:
			printf("input prefix:");
			scanf("%s", prefix);
			printf("----------\n");
			prefixiter = shannon_db_create_prefix_iterator(db, roptions, prefix, strlen(prefix));
			test_prefixiter_firstandnext(prefixiter);
			break;
		case 4:
			printf("input prefix:");
			scanf("%s", prefix);
			printf("----------\n");
			prefixiter = shannon_db_create_prefix_iterator(db, roptions, prefix, strlen(prefix));
			test_prefixiter_lastandprev(prefixiter);
			break;
		case 5:
			iter = shannon_db_create_iterator(db, roptions);
			printf("----------\n");
			test_iter_noprefix_order(iter);
			break;
		case 6:
			iter = shannon_db_create_iterator(db, roptions);
			printf("----------\n");
			test_iter_noprefix_reverse_order(iter);
			break;
		default:
			break;
		}
	}
	StartPhase("cleanup");
	{
		shannon_db_iter_destroy(iter);
		shannon_db_iter_destroy(prefixiter);
		shannon_db_options_destroy(options);
		shannon_db_readoptions_destroy(roptions);
		shannon_db_writeoptions_destroy(woptions);
		shannon_db_close(db);
	}
	fprintf(stderr, "PASS\n");
	return 0;
}

