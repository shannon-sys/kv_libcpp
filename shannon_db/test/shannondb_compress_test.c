#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "shannon_db.h"

#define MAX_COUNT 72000000
#define __random(x) (rand()%x)
#define random() __random(MAX_COUNT)

#define KEY_LEN 24
#define VALUE_LEN (968 + 1024)
#define PAD_KEY 'K'
#define PAD_VALUE 'V'

void generate_key(char *key, int num)
{
	sprintf(key, "%016d", num);
	key[16] = PAD_KEY;
}

void generate_value(char *value, int num)
{
	sprintf(value, "%016d", num);
	value[16] = PAD_VALUE;
}

int value_is_valid(char *value, int value_len, int num)
{
	int i;
	char tmp[17];

	if (value_len != VALUE_LEN) {
		printf("value len is fault!\n");
		return 0;
	}
	sprintf(tmp, "%016d", num);
	if (strncmp(value, tmp, 16) != 0) {
		printf("value num is fault!\n");
		return 0;
	}
	for (i = 16; i < VALUE_LEN; i++) {
		if (value[i] != PAD_VALUE) {
			printf("pad value is fault, i=%d.\n", i);
			return 0;
		}
	}
	return 1;
}

void write_random_kvs(struct shannon_db *db, struct db_writeoptions *wop)
{
	char key[KEY_LEN+1];
	char value[VALUE_LEN+1];
	int num, i, max = 100;
	char *err = NULL;

	memset(key, PAD_KEY, KEY_LEN);
	memset(value, PAD_VALUE, VALUE_LEN);
	key[KEY_LEN] = 0;
	value[VALUE_LEN] = 0;
	db_writebatch_t *wb = shannon_db_writebatch_create();
	for (i = 0; i < max; ++i) {
		num = random();
		generate_key(key, num);
		generate_value(value, num);
		shannon_db_writebatch_put(wb, key, KEY_LEN, value, VALUE_LEN, &err);
		if (err)
			exit(-1);
	}
	shannon_db_write(db, wop, wb, &err);
	if (err)
		exit(-1);
	shannon_db_writebatch_destroy(wb);
}

void get_random_kv(struct shannon_db *db, struct db_readoptions *rop)
{
	char key[KEY_LEN+1];
	char value[VALUE_LEN+1];
	int num, val_len;
	char *err = NULL;

	memset(key, PAD_KEY, KEY_LEN);
	memset(value, 0, VALUE_LEN+1);
	key[KEY_LEN] = 0;
	num = random();
	generate_key(key, num);
	shannon_db_get(db, rop, key, KEY_LEN, value, VALUE_LEN, &val_len, &err);
	if (err || value_is_valid(value, val_len, num) == 0) {
		printf("===== ioctl get kv failed! num=%d, err=%s.\n", num, err);
		exit(-1);
	}
}

void get_random_kvs(struct shannon_db *db, struct db_readoptions *rop)
{
	char key[KEY_LEN+1];
	char value[VALUE_LEN+1];
	int num, val_len;
	char *err = NULL;
	int count = 0, max = MAX_COUNT / 20;
	int print_time = 500000;

	printf("====== start get random kvs.\n");
	memset(key, PAD_KEY, KEY_LEN);
	memset(value, 0, VALUE_LEN+1);
	key[KEY_LEN] = 0;
	while (count < max) {
		count++;
		if (count % print_time == 0) {
			printf("===== get random kvs, nums=(%d * %d).\n", count / print_time, print_time);
		}
		num = random();
		generate_key(key, num);
		shannon_db_get(db, rop, key, KEY_LEN, value, VALUE_LEN, &val_len, &err);
		if (err || value_is_valid(value, val_len, num) == 0) {
			printf("===== ioctl get kv failed! num=%d, err=%s.\n", num, err);
			exit(-1);
		}
	}
	printf("====== end get random kvs.\n");
}

void get_fix_kvs(struct shannon_db *db, struct db_readoptions *rop, int start, int end)
{
	char key[KEY_LEN+1];
	char value[VALUE_LEN+1];
	int num, val_len;
	char *err = NULL;
	int i, print_time = 100 * 1000;

	printf("====== start get fix kvs at [%d, %d).\n", start, end);
	memset(key, PAD_KEY, KEY_LEN);
	memset(value, 0, VALUE_LEN+1);
	key[KEY_LEN] = 0;
	for (i = start; i < end; i++) {
		generate_key(key, i);
		shannon_db_get(db, rop, key, KEY_LEN, value, VALUE_LEN, &val_len, &err);
		if (err || value_is_valid(value, val_len, i) == 0) {
			printf("===== ioctl get kv failed! num=%d, err=%s.\n", i, err);
			exit(-1);
		}
		if (i % print_time == 0)
			printf("====== start get key_num=(%d * %d).\n", i / print_time, print_time);
	}
	printf("====== end get fix kvs.\n");
}

void random_write_and_get(struct shannon_db *db, struct db_readoptions *rop, struct db_writeoptions *wop)
{
	int count_interval = 10000;
	unsigned long count = 0, max;

	max = (unsigned long)MAX_COUNT * 4;
	printf("===== start random_write_and_get test.\n");
	while (1) {
		write_random_kvs(db, wop);
		get_random_kv(db, rop);
		count++;
		if (count % count_interval == 0) {
			sleep(1);
			printf("==== write random kv: count = %lu * (%d * 100).\n", count / count_interval, count_interval);
		}
		if (count > max)
			break;
	}
	printf("===== end random_write_and_get test.\n");
}

int write_sequence_kvs(struct shannon_db *db, struct db_writeoptions *wop, int start_num)
{
	char key[KEY_LEN+1];
	char value[VALUE_LEN+1];
	int num, end_num = start_num + 1000;
	char *err = NULL;

	memset(key, PAD_KEY, KEY_LEN);
	memset(value, PAD_VALUE, VALUE_LEN);
	key[KEY_LEN] = 0;
	value[VALUE_LEN] = 0;
	db_writebatch_t *wb = shannon_db_writebatch_create();
	for (num = start_num; num < end_num; ++num) {
		generate_key(key, num);
		generate_value(value, num);
		shannon_db_writebatch_put(wb, key, KEY_LEN, value, VALUE_LEN, &err);
		if (err)
			exit(-1);
	}
	shannon_db_write(db, wop, wb, &err);
	if (err)
		exit(-1);
	shannon_db_writebatch_destroy(wb);

	return end_num;
}

void write_all_sequence_kvs(struct shannon_db *db, struct db_writeoptions *wop)
{
	int print_time = 1000, count = 0;
	int start_num = 0;

	printf("==== start writebatch kvs to ssd, num: 0 ~ %d.\n", MAX_COUNT);
	while (start_num < MAX_COUNT) {
		start_num = write_sequence_kvs(db, wop, start_num);
		if (count % print_time == 0)
			fprintf(stderr, "==== write start_num: %d * (%d * 1000).\n", count / print_time, print_time);
		count++;
	}
	printf("==== end writebatch kvs to ssd, num: 0 ~ %d.\n", MAX_COUNT);
}

enum type_num {
	SEQ_READ              = 1,
	SEQ_WRITE             = 2,
	RANDOM_WRITE_AND_GET  = 3,
	RANDOM_GET            = 4,
	GET_FIX               = 5,
};

void usage()
{
	printf("compress_test usage: ./compress_test type_number [start_num, end_num].\n");
	printf("1. type_number=%d: just read all keys at [0, %d).\n", SEQ_READ, MAX_COUNT);
	printf("2. type_number=%d: just write all keys at [0 ~ %d).\n", SEQ_WRITE, MAX_COUNT);
	printf("3. type_number=%d: random write 100 keys and random read 1 key.\n", RANDOM_WRITE_AND_GET);
	printf("4. type_number=%d: random get key.\n", RANDOM_GET);
	printf("5. type_number=%d: get fix key, need 2 number.\n", GET_FIX);
	printf("\n");
}

int main(int argc, char **argv)
{
	char *err = NULL;
	struct shannon_db *db = NULL;
	struct db_options *options;
	struct db_readoptions *roptions;
	struct db_writeoptions *woptions;
	char *dbname = "testdb";
	int test_type, start_num, end_num;

	if (argc != 2 && argc != 4) {
		usage();
		return -1;
	}
	test_type = atoi(argv[1]);
	if (test_type == GET_FIX && argc != 4) {
		usage();
		return -1;
	}
	if (argc >= 4) {
		start_num = atoi(argv[2]);
		end_num = atoi(argv[3]);
		if (start_num < 0)
			start_num = 0;
		if (end_num < start_num)
			end_num = start_num + 100;
	}

	srand((int)time(0));
	options = shannon_db_options_create();
	woptions = shannon_db_writeoptions_create();
	roptions = shannon_db_readoptions_create();
	shannon_db_writeoptions_set_sync(woptions, 1);
	shannon_db_options_set_create_if_missing(options, 1);
	db = shannon_db_open(options, "/dev/kvdev0", dbname, &err);

	switch(test_type) {
	case SEQ_READ:
		// TODO: change get kv to read_batch
		start_num = 0;
		end_num = MAX_COUNT;
		get_fix_kvs(db, roptions, start_num, end_num);
		break;
	case SEQ_WRITE:
		write_all_sequence_kvs(db, woptions);
		break;
	case RANDOM_WRITE_AND_GET:
		random_write_and_get(db, roptions, woptions);
		break;
	case RANDOM_GET:
		get_random_kvs(db, roptions);
		break;
	case GET_FIX:
		get_fix_kvs(db, roptions, start_num, end_num);
		break;
	default:
		usage();
		break;
	}

//	shannon_destroy_db(db, &err);
	shannon_db_close(db);

	return 0;
}
