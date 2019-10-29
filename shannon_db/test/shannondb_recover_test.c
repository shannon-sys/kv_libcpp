/*******************************************************
 * File: shannondb_recover_test.c
 * Describe: first sequence write data, restart SSD
 *           then read data and check
 * Athor: chao.chen <chao.chen@shannon-sys.com>
 * Create time: 2018-11-05
 * Change time:
 *******************************************************/
#include <shannon_db.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *phase = "";
/* VALUE_LEN = 1k - prefix_len - crc_len - key_len = 1024 - 24 - 8 - 24
 * key format:   "key+num-KKKKK..."
 * value format: "value+num-VVVVV..." */
#define KEY_LEN 24
#define VALUE_LEN 968
#define KEY_PAD_CHAR 'K'
#define VALUE_PAD_CHAR 'V'

static void StartPhase(char *name)
{
	fprintf(stderr, "======== Test %s\n", name);
	phase = name;
}

#define CheckCondition(cond) \
	({ if (!(cond)) { \
		fprintf(stderr, "%s: %d: %s: %s\n", __FILE__, __LINE__, phase, #cond); \
		abort(); \
	} })


#define CheckError(err) \
	({ if ((err) != 0) { \
		fprintf(stderr, "%s: %d: %s: %s\n", __FILE__, __LINE__, phase, (err)); \
		abort(); \
	} })

int test_shannondb_write(shannon_db_t *db, shannon_cf_handle_t *cf,
		db_writeoptions_t *woptions, db_readoptions_t *roptions, int start_num, int max_num)
{
	int i;
	int ret = 0;
	int value_len = 0;
	char keybuf[KEY_LEN];
	char valuebuf[VALUE_LEN];
	char buffer[VALUE_LEN];
	int print_time = 64 * 1024;
	char *err = NULL;

	fprintf(stderr, "We will put keynum form %d to %d.\n", start_num, max_num);
	fprintf(stderr, "fixed key_len = %d, fixed value_len = %d\n", KEY_LEN - 1, VALUE_LEN - 1);
	for (i = start_num; i < max_num; i++) {
		memset(keybuf, KEY_PAD_CHAR, KEY_LEN);
		memset(valuebuf, VALUE_PAD_CHAR, VALUE_LEN);
		memset(buffer, 0, VALUE_LEN);
		keybuf[KEY_LEN-1] = '\0';
		valuebuf[VALUE_LEN-1] = '\0';

		snprintf(keybuf, sizeof(keybuf), "key+%d-", i);
		keybuf[strlen(keybuf)] = KEY_PAD_CHAR;
		snprintf(valuebuf, sizeof(valuebuf), "value+%d-", i);
		valuebuf[strlen(valuebuf)] = VALUE_PAD_CHAR;
		if (i % print_time == 0) {
			fprintf(stderr, "Now, put keynum: %d * %d\n", i / print_time, print_time);
			fprintf(stderr, "key is: %s!\n", keybuf);
		}
		if (cf)
			shannon_db_put_cf(db, woptions, cf, keybuf, strlen(keybuf), valuebuf, strlen(valuebuf), &err);
		else
			shannon_db_put(db, woptions, keybuf, strlen(keybuf), valuebuf, strlen(valuebuf), &err);
		if (err) {
			printf("add ioctl failed\n");
			exit(-1);
		}

		if (i % print_time == 0) {
			if (cf)
				shannon_db_get_cf(db, roptions, cf, keybuf, strlen(keybuf), buffer, VALUE_LEN, &value_len, &err);
			else
				shannon_db_get(db, roptions, keybuf, strlen(keybuf), buffer, VALUE_LEN, &value_len, &err);
			if (err) {
				printf("get ioctl failed\n");
			}
			if (strcmp(valuebuf, buffer)) {
				printf("dismatch value:%s \n!, buffer:%s!\n", valuebuf, buffer);
				exit(-1);
			}
		}
	}
	return ret;
}

int test_shannondb_read_and_check(shannon_db_t *db, shannon_cf_handle_t *cf,
		db_writeoptions_t *woptions, db_readoptions_t *roptions, int start_num, int max_num)
{
	int i, j;
	int ret = 0;
	int value_len = 0;
	char keybuf[KEY_LEN];
	char valuebuf[VALUE_LEN];
	int print_time = 64 * 1024;
	char *err = NULL;

	int key_num_start = strlen("key+");
	int value_num_start = strlen("value+");
	int num_len = 0;

	fprintf(stderr, "We will read keynum form %d to %d, then check value\n", start_num, max_num);
	for (i = start_num; i < max_num; i++) {
		memset(keybuf, KEY_PAD_CHAR, KEY_LEN);
		memset(valuebuf, 0, VALUE_LEN);
		keybuf[KEY_LEN-1] = '\0';

		snprintf(keybuf, sizeof(keybuf), "key+%d-", i);
		num_len = strlen(keybuf) - key_num_start - 1;
		keybuf[strlen(keybuf)] = KEY_PAD_CHAR;
		if (i % print_time == 0) {
			fprintf(stderr, "Now, read and check keynum: %d * %d\n", i / print_time, print_time);
			fprintf(stderr, "key is: %s!\n", keybuf);
		}
		if (cf)
			shannon_db_get_cf(db, roptions, cf, keybuf, strlen(keybuf), valuebuf, VALUE_LEN, &value_len, &err);
		else
			shannon_db_get(db, roptions, keybuf, strlen(keybuf), valuebuf, VALUE_LEN, &value_len, &err);
		if (err) {
			printf("get ioctl failed\n");
			exit(-1);
		}

		// start check value
		if ((value_len != VALUE_LEN - 1)) {
			printf("value_len is fault! number = %d, value_len = %d\n", i, value_len);
			exit(-1);
		}
		if (strncmp(valuebuf, "value+", 6)) {
			printf("prefix is fault! number = %d, value:%s\n", i, valuebuf);
			exit(-1);
		}
		if (strncmp(valuebuf + value_num_start, keybuf + key_num_start, num_len + 1)) {
			printf("number is fault! number = %d, value:%s\n", i, valuebuf);
			exit(-1);
		}
		for (j = value_num_start + num_len + 1; j < value_len; j++) {
			if (valuebuf[j] != VALUE_PAD_CHAR) {
				printf("value_pad_char is fault! number = %d, value:%s\n", i, valuebuf);
			}
		}
	}
	return ret;
}

void usage() {
	printf("recover_test usage: ./recover_test write start_num end_num columnfamily\n");
	printf("    write:        1 write key and value; 0 read and check value\n");
	printf("    start_num:    key index will start with: (start_num * 1024)\n");
	printf("    end_num:      key index will end with: (end_num * 1024 - 1)\n");
	printf("    columnfamily: kv wile write to this cf\n\n");

	printf("README: every (key + value) size is close to 1KB\n\n");

	printf("Example: if you want to write key/value index range: 0 ~ 256*1024, use test.cf\n");
	printf("         ./recover_test 1 0 256 test.cf\n");
	printf("Example: if you want to write key/value index range: 0 ~ 256*1024, use default cf\n");
	printf("         ./recover_test 1 0 256\n");
	printf("Example: use default range: 0 ~ 1024*1024*4, use default cf\n");
	printf("         ./recover_test 1\n");
}

static shannon_cf_handle_t *open_cf(shannon_db_t *db, db_options_t* option, char *cf_name, char **err)
{
	shannon_cf_handle_t *cf_handle;

	cf_handle = shannon_db_open_column_family(db, option, cf_name, err);
	if (*err && option->create_if_missing) {
		printf("try to create cf=%s\n", cf_name);
		*err = NULL;
		cf_handle = shannon_db_create_column_family(db, option, cf_name, err);
	}
	return cf_handle;
}

int main(int argc, char **argv)
{
	struct shannon_db *db;
	struct shannon_cf_handle *cf = NULL;
	struct db_options *options;
	struct db_readoptions *roptions;
	struct db_writeoptions *woptions;
	char *err = NULL;
	char *dbname = "test_recover.db";
	char *cfname = NULL;

	int default_start_num = 0;
	int default_write_nums = (1024 * 1024) * 4;
	int write;
	int start_num;
	int max_num;

	if (argc != 2 && argc != 4 && argc != 5) {
		usage();
		return -1;
	}
	write = atoi(argv[1]);
	if (argc >= 4) {
		start_num = atoi(argv[2]) * 1024;
		max_num = atoi(argv[3]) * 1024;
		if (start_num < 0) {
			start_num = default_start_num;
		}
		if (max_num < start_num) {
			max_num = start_num + default_write_nums;
		}
		if (argc == 5)
			cfname = argv[4];
	} else {
		start_num = default_start_num;
		max_num = default_write_nums;
	}

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
	db = shannon_db_open(options, "/dev/kvdev0", dbname, &err);
	if (cfname != NULL) {
		cf = open_cf(db, options, cfname, &err);
		if (err) {
			fprintf(stderr, "open cf=%s fail, err=%s\n", cfname, err);
			exit(-1);
		}
	}

	if (write) {
		test_shannondb_write(db, cf, woptions, roptions, start_num, max_num);
	} else {
		test_shannondb_read_and_check(db, cf, woptions, roptions, start_num, max_num);
	}
	fprintf(stderr, "PASS\n");

	return 0;
}
