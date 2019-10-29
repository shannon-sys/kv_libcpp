#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "shannon_db.h"
#define random(x) (rand()%x)

int main(int argc, char **argv)
{
	char *err = NULL;
	struct shannon_db *db = NULL;
	struct db_options *options;
	struct db_readoptions *roptions;
	struct db_writeoptions *woptions;
	struct shannon_db_iterator *iter = NULL;
	struct shannon_db_iterator *prefixiter = NULL;
	int i = 0, num = 0, ret = 0, count;
	int j = 10000, key_len;
	char key[256], value[4096], seek_key[256];
	char *dbname = "testdb";

	srand((int)time(0));
	options = shannon_db_options_create();
	woptions = shannon_db_writeoptions_create();
	roptions = shannon_db_readoptions_create();
	shannon_db_writeoptions_set_sync(woptions, 1);
	shannon_db_options_set_create_if_missing(options, 1);
	db = shannon_db_open(options, "/dev/kvdev0", dbname, &err);
	shannon_destroy_db(db, &err);
	shannon_db_close(db);
	db = shannon_db_open(options, "/dev/kvdev0", dbname, &err);
	printf("wait!\n");
	while (j--) {
		for (i = 1; i < 100; i++) {
			sprintf(key, "%8d\0", i);
			shannon_db_put(db, woptions, key, 8, value, 4096, &err);
		}
	}
	iter = shannon_db_create_iterator(db, roptions);
	shannon_db_iter_seek_to_first(iter);
	shannon_db_iter_move_prev(iter);
	if (iter->valid) {
		printf("seek_to_first is err!\n");
		ret = -1;
	}
	shannon_db_iter_seek_to_last(iter);
	shannon_db_iter_move_next(iter);
	if (iter->valid) {
		printf("seek_to_last is err!\n");
		ret = -1;
	}

	num = random(99) + 1;
	sprintf(seek_key, "%8d\0", num);
	count = 0;
	printf("seek_for_prev%s\n", seek_key);
	shannon_db_iter_seek_for_prev(iter, seek_key, 8);
	for (; iter->valid; shannon_db_iter_move_next(iter)) {
		shannon_db_iter_cur_key(iter, key, 256, &key_len);
		if (key_len != 8 || memcmp(key, seek_key, key_len) < 0) {
			printf("seek_for_prev error!\n");
			ret = -1;
		}
		count++;
		key[key_len] = 0;
		printf("key-%s\n", key);
	}
	if (count != (100 - num)) {
		printf("seek_for_prev error!\n");
		ret = -1;
	}

	num = random(99) + 1;
	sprintf(seek_key, "%8d", num);
	count = 0;
	printf("---------------------------\n");
	printf("seek%s\n", seek_key);
	shannon_db_iter_seek(iter, seek_key, 8);
	for (; iter->valid; shannon_db_iter_move_prev(iter)) {
		shannon_db_iter_cur_key(iter, key, 256, &key_len);
		if (key_len != 8 || memcmp(key, seek_key, key_len) > 0) {
			printf("seek error!\n");
			ret = -1;
		}
		count++;
		key[key_len] = 0;
		printf("key-%s\n", key);
	}
	if (count != num) {
		printf("seek error!\n");
		ret = -1;
	}
	if (ret == -1)
		printf("iter_direction_test is error!");
	else
		printf("test pass!\n");
	shannon_db_iter_destroy(iter);
	shannon_destroy_db(db, &err);
	shannon_db_close(db);
	return ret;
}

