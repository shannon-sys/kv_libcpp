#include <shannon_db.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

char *phase = "";
#define BUF_LEN 128
/* for test_put_quarter_kbytes():
 * VALUE_LEN = 1k - prefix_len - crc_len - key_len = 1024 - (24 + 104) - 8 - 24
 * key format:   "key+num-KKKK..."
 * value format: "value+num-VVVV..." */
#define KEY_LEN (24 + 104)
#define VALUE_LEN (968 - 104)
#define KEY_PAD_CHAR 'K'
#define VALUE_PAD_CHAR 'V'

static void start_phase(char *name)
{
	printf("=== Test %s\n", name);
	phase = name;
}

static void check_equal(char *expected, const char *v, size_t n)
{
	if (expected == NULL && v == NULL) {
	} else if (expected != NULL && v != NULL && n == strlen(expected) && memcmp(expected, v, n) == 0) {
		return;
	} else {
		fprintf(stderr, "%s: expected '%s', got '%s'\n", phase, (expected ? expected : "(null)"), (v ? v : "(null"));
		abort();
	}
}

static unsigned short ccitt_table[256] = {
	0x0001, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
	0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
	0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
	0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
	0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
	0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
	0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
	0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
	0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
	0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
	0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
	0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
	0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
	0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
	0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
	0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
	0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
	0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
	0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
	0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
	0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
	0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
	0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

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

int test_readbatch_multi_get(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int times = 100;
	int i = 0, ret = 0;
	int failed_cmd_count = 0;
	char keybuf[100];
	char valbuf[100];
	char buffer[100];
	char *err = NULL;
	char **value = (char **)malloc(times * sizeof(char *));
	unsigned int *value_len_addr = malloc(times * sizeof(unsigned int));
	unsigned int *status = malloc(times * sizeof(unsigned int));

	db_readbatch_t *rb = shannon_db_readbatch_create();
	for (i = 0; i < times; i++) {
		memset(keybuf, 0 , 100);
		memset(valbuf, 0 , 100);
		memset(buffer, 0 , 100);
		snprintf(keybuf, sizeof(keybuf), "key%d", i);
		value[i] = malloc(BUF_LEN);
		memset(value[i], 0, BUF_LEN);
		shannon_db_readbatch_get(rb, keybuf, strlen(keybuf), value[i], 100,
				value_len_addr + i, status + i, &err);
		if (err) {
			printf("shannon_db_readbatch_get ioctl failed\n");
			exit(-1);
		}
	}

	shannon_db_read(db, roptions, rb, &failed_cmd_count, &err);
	if (err) {
		printf("shannon_db_read ioctl failed :%s\n", err);
		exit(-1);
	}
	for (i = 0; i < times; i++) {
		if (memcmp((char *)&ccitt_table[i], value[i], 2)) {
			printf("line=%d: dismatch index = %d\n", __LINE__, i);
			exit(-1);
		}
		free(value[i]);
	}

	free(value);
	shannon_db_readbatch_destroy(rb);
	return 0;
}

int test_readbatch_single_get(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int times = 255;
	char keybuf[100];
	char valbuf[100];
	char buffer[100];
	int i = 0, ret = 0;
	int failed_cmd_count = 0;
	unsigned int status, value_len;
	char *err = NULL;

	for (i = 0; i < times; i++) {
		memset(keybuf, 0 , 100);
		memset(valbuf, 0 , 100);
		memset(buffer, 0 , 100);
		db_readbatch_t *rb = shannon_db_readbatch_create();
		snprintf(keybuf, sizeof(keybuf), "key%d", i);
		shannon_db_readbatch_get(rb, keybuf, strlen(keybuf), buffer, 100, &value_len, &status, &err);
		if (err) {
			printf("shannon_db_readbatch_get  ioctl failed\n");
			exit(-1);
		}
		shannon_db_read(db, roptions, rb, &failed_cmd_count, &err);
		if (err) {
			printf("shannon_db_read ioctl failed :%s\n", err);
			exit(-1);
		}
		if (memcmp((char *)&ccitt_table[i], buffer, value_len)) {
			printf("line=%d: dismatch index = %d\n", __LINE__, i);
			exit(-1);
		}
		shannon_db_readbatch_destroy(rb);
	}

	return 0;
}

int test_put_byte(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int i;
	int ret = 0;
	unsigned int value_len;
	int n = 255;
	char keybuf[100];
	char valbuf[100];
	char buffer[100];
	char *err = NULL;

	for (i = 0; i < n; i++) {
		memset(keybuf, 0 , 100);
		memset(valbuf, 0 , 100);
		memset(buffer, 0 , 100);
		snprintf(keybuf, sizeof(keybuf), "key%d", i);
		shannon_db_put(db, woptions, keybuf, strlen(keybuf), (char *)&ccitt_table[i], 2, &err);
		if (err) {
			printf("add ioctl failed\n");
			exit(-1);
		}
		shannon_db_get(db, roptions, keybuf, strlen(keybuf), buffer, 100, &value_len, &err);
		if (err) {
			printf("get ioctl failed\n");
			exit(-1);
		}
		if (memcmp((char *)&ccitt_table[i], buffer, value_len)) {
			printf("line=%d: dismatch index = %d\n", __LINE__, i);
			exit(-1);
		}
	}

	return ret;
}

int test_put_char(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int times = 100;
	int i = 0, ret = 0;
	int failed_cmd_count = 0;
	char key[BUF_LEN];
	char buffer[BUF_LEN];
	char *err = NULL;
	char **value = (char **)malloc(times * sizeof(char *));
	unsigned int *value_len_addr = malloc(times * sizeof(unsigned int));
	unsigned int *status = malloc(times * sizeof(unsigned int));

	for (i = 0; i < times; i++) {
		memset(key, 0, BUF_LEN);
		snprintf(key, BUF_LEN, "%d", i);
		value[i] = randomstr();
		shannon_db_put(db, woptions, key, strlen(key), value[i], strlen(value[i]), &err);
		if (err) {
			printf("shannon_db_writebatch_put ioctl failed\n");
			exit(-1);
		}
	}

	for (i = 0; i < times; i++) {
		memset(key, 0, BUF_LEN);
		memset(buffer, 0, BUF_LEN);
		snprintf(key, BUF_LEN, "%d", i);
		shannon_db_get(db, roptions, key, strlen(key), buffer, BUF_LEN, &failed_cmd_count, &err);
		if (err) {
			printf("test_writebatch_case1 get failed\n");
			exit(-1);
		}
		if (strcmp(value[i], buffer)) {
			printf("line=%d: dismatch value=%s, buffer=%s\n", __LINE__, value[i], buffer);
			exit(-1);
		}
		free(value[i]);
	}

	free(value);
	return 0;
}

int test_readbatch_empty(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int total_value_len = 1;
	int ret;
	char *err = NULL;

	db_readbatch_t *rb = shannon_db_readbatch_create();
	shannon_db_read(db, roptions, rb, &total_value_len, &err);
	if (err) {
		printf("line=%d: total_value_len=%d\n", __LINE__, total_value_len);
		printf("line=%d: shannon_db_writebatch_put ioctl failed, err=%s\n", __LINE__, err);
	} else {
		printf("test readbatch empty should fail, but not!\n");
		exit(-1);
	}
	shannon_db_readbatch_destroy(rb);

	return 0;
}

int test_readbatch_case1(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int times = 100;
	int i;
	int ret;
	unsigned int value_len, status, failed_cmd_count;
	char key[BUF_LEN];
	char buffer[BUF_LEN];
	char *value;
	char *err = NULL;

	for (i = 0; i < times; i++) {
		db_readbatch_t *rb = shannon_db_readbatch_create();
		memset(key, 0, BUF_LEN);
		snprintf(key, BUF_LEN, "%d", i);
		shannon_db_readbatch_get(rb, key, strlen(key), buffer, BUF_LEN, &value_len, &status, &err);
		if (err) {
			printf("shannon_db_readbatch_get() ioctl failed\n");
			exit(-1);
		}
		shannon_db_read(db, roptions, rb, &failed_cmd_count, &err);
		if (err) {
			printf("test_readbatch_case1() failed\n");
			exit(-1);
		}
		memset(buffer, 0 , BUF_LEN);
		shannon_db_readbatch_destroy(rb);
	}

	return 0;
}

int test_put_kbytes(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int i;
	int ret = 0;
	int value_len = 0;
	int print_time = 32 * 1024;
	int start_num = 0, max_num = 64 * 1024;
	char keybuf[KEY_LEN];
	char valuebuf[VALUE_LEN];
	char buffer[VALUE_LEN];
	char *err = NULL;

	fprintf(stderr, "put keynum from %d to %d.\n", start_num, max_num);
	fprintf(stderr, "fixed key_len = %d, fixed value_len = %d.\n", KEY_LEN - 1, VALUE_LEN - 1);
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
		shannon_db_put(db, woptions, keybuf, strlen(keybuf), valuebuf, strlen(valuebuf), &err);
		if (err) {
			printf("line=%d: add ioctl failed\n", __LINE__);
			exit(-1);
		}

		if (i % print_time == 0) {
			shannon_db_get(db, roptions, keybuf, strlen(keybuf), buffer, VALUE_LEN, &value_len, &err);
			if (err) {
				printf("line=%d: get ioctl failed\n", __LINE__);
				exit(-1);
			}
			if (strcmp(valuebuf, buffer)) {
				printf("line=%d: dismatch value: %s \n!, buffer: %s!\n", __LINE__, valuebuf, buffer);
				exit(-1);
			}
		}
	}

	return ret;
}

int test_check_kbytes_value(int key_num, char *value, int value_len)
{
	int ret = 0, num_len = 0, i;
	int key_num_start = strlen("key+");
	int value_num_start = strlen("value+");
	char keybuf[KEY_LEN];

	memset(keybuf, KEY_PAD_CHAR, KEY_LEN);
	keybuf[KEY_LEN-1] = '\0';
	snprintf(keybuf, sizeof(keybuf), "key+%d-", key_num);
	num_len = strlen(keybuf) - key_num_start - 1;
	keybuf[strlen(keybuf)] = KEY_PAD_CHAR;

	if (value_len != VALUE_LEN - 1) {
		printf("value_len is fault! key_num=%d, value_len=%d\n", key_num, value_len);
		exit(-1);
	}
	if (strncmp(value, "value+", 6)) {
		printf("prefix is faule! key_num=%d, value: %s!\n", key_num, value);
		exit(-1);
	}
	if (strncmp(value + value_num_start, keybuf + key_num_start, num_len + 1)) {
		printf("number is fault! key_num = %d, value: %s!\n", key_num, value);
		exit(-1);
	}
	for (i = value_num_start + num_len + 1; i < value_len; i++) {
		if (value[i] != VALUE_PAD_CHAR) {
			printf("value_pad_char is fault! key_num = %d, value: %s!\n", key_num, value);
			exit(-1);
		}
	}

	return ret;
}

int test_put_counts(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int ret = 0, i;
	int max_counts = 40 * 1000;
	int print_time = 20 * 1000;
	int value_len = 0;
	char keybuf[100];
	char valuebuf[100];
	char buffer[100];
	char *err = NULL;

	fprintf(stderr, "put keycount from %d to %d.\n", 0, max_counts);
	for (i = 0; i < max_counts; i++) {
		memset(keybuf, 0, 100);
		memset(valuebuf, 0, 100);
		memset(buffer, 0, 100);

		snprintf(keybuf, sizeof(keybuf), "keycount+%d-", i);
		snprintf(valuebuf, sizeof(valuebuf), "valuecount+%d-", i);
		if (i % print_time == 0) {
			fprintf(stderr, "Now, put keynum: %d * %d\n", i / print_time, print_time);
			fprintf(stderr, "key is: %s!\n", keybuf);
		}
		shannon_db_put(db, woptions, keybuf, strlen(keybuf), valuebuf, strlen(valuebuf), &err);
		if (err) {
			printf("line=%d: add ioctl failed\n", __LINE__);
			exit(-1);
		}
		shannon_db_get(db, roptions, keybuf, strlen(keybuf), buffer, 100, &value_len, &err);
		if (err) {
			printf("line=%d: get ioctl failed\n", __LINE__);
			exit(-1);
		}
		if (strcmp(valuebuf, buffer)) {
			printf("line=%d: dismatch value: %s \n!, buffer: %s!\n", __LINE__, valuebuf, buffer);
			exit(-1);
		}
	}

	return ret;
}

int test_check_counts_value(int key_num, char *value, int value_len)
{
	int ret = 0, num_len = 0, i;
	int key_num_start = strlen("keycount+");
	int value_num_start = strlen("valuecount+");
	char keybuf[100];

	memset(keybuf, 0, 100);
	snprintf(keybuf, sizeof(keybuf), "keycount+%d-", key_num);
	num_len = strlen(keybuf) - key_num_start - 1;

	if (value_len != value_num_start + num_len + 1) {
		printf("value_len is fault! key_num=%d, value_len=%d\n", key_num, value_len);
		exit(-1);
	}
	if (strncmp(value, "valuecount+", value_num_start)) {
		printf("prefix is faule! key_num=%d, value: %s!\n", key_num, value);
		exit(-1);
	}
	if (strncmp(value + value_num_start, keybuf + key_num_start, num_len + 1)) {
		printf("number is fault! key_num = %d, value: %s!\n", key_num, value);
		exit(-1);
	}
	return ret;
}

int __test_readbatch_get_maxbatchsize(struct shannon_db *db, struct db_readoptions *roptions,
		struct db_writeoptions *woptions, int times)
{
	int i = 0, ret = 0;
	int failed_cmd_count = 0;
	char keybuf[KEY_LEN];
	char *err = NULL;
	char **value = (char **)malloc(times * sizeof(char *));
	unsigned int *value_len_addr = malloc(times * sizeof(unsigned int));
	unsigned int *status = malloc(times * sizeof(unsigned int));

	db_readbatch_t *rb = shannon_db_readbatch_create();
	for (i = 0; i < times; i++) {
		memset(keybuf, KEY_PAD_CHAR, KEY_LEN);
		keybuf[KEY_LEN-1] = '\0';
		snprintf(keybuf, sizeof(keybuf), "key+%d-", i);
		keybuf[strlen(keybuf)] = KEY_PAD_CHAR;

		value[i] = malloc(VALUE_LEN);
		memset(value[i], 0, VALUE_LEN);
		shannon_db_readbatch_get(rb, keybuf, strlen(keybuf), value[i], VALUE_LEN,
				value_len_addr + i, status + i, &err);
		if (err) {
			fprintf(stderr, "line=%d: shannon_db_readbatch_get ioctl failed: %s\n", __LINE__, err);
			ret = -1;
			goto out;
		}
	}

	shannon_db_read(db, roptions, rb, &failed_cmd_count, &err);
	if (err) {
		fprintf(stderr, "line=%d: shannon_db_read() ioctl failed: %s\n", __LINE__, err);
		ret = -1;
		goto out;
	}
	for (i = 0; i < times; i++) {
		test_check_kbytes_value(i, value[i], strlen(value[i]));
	}

out:
	for (i = 0; i < times; i++) {
		if (value[i])
			free(value[i]);
	}
	free(value);
	shannon_db_readbatch_destroy(rb);
	return ret;
}

int __test_readbatch_get_maxcount(struct shannon_db *db, struct db_readoptions *roptions,
		struct db_writeoptions *woptions, int times)
{
	int i = 0, ret = 0;
	int failed_cmd_count = 0;
	char keybuf[100];
	char *err = NULL;
	char **value = (char **)malloc(times * sizeof(char *));
	unsigned int *value_len_addr = malloc(times * sizeof(unsigned int));
	unsigned int *status = malloc(times * sizeof(unsigned int));

	db_readbatch_t *rb = shannon_db_readbatch_create();
	for (i = 0; i < times; i++) {
		memset(keybuf, 0 , 100);
		snprintf(keybuf, sizeof(keybuf), "keycount+%d-", i);
		value[i] = malloc(100);
		memset(value[i], 0, 100);
		shannon_db_readbatch_get(rb, keybuf, strlen(keybuf), value[i], VALUE_LEN,
				value_len_addr + i, status + i, &err);
		if (err) {
			fprintf(stderr, "line=%d: shannon_db_readbatch_get ioctl failed: %s\n", __LINE__, err);
			ret = -1;
			goto out;
		}
	}

	shannon_db_read(db, roptions, rb, &failed_cmd_count, &err);
	if (err) {
		fprintf(stderr, "line=%d: shannon_db_read() ioctl failed: %s\n", __LINE__, err);
		ret = -1;
		goto out;
	}
	for (i = 0; i < times; i++) {
		test_check_counts_value(i, value[i], strlen(value[i]));
	}

out:
	for (i = 0; i < times; i++) {
		if (value[i])
			free(value[i]);
	}
	free(value);
	shannon_db_readbatch_destroy(rb);
	return ret;
}

int test_readbatch_max(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int ret = 0;
	int max_count = 20 * 1000;
	/* FIXME key_len_max = 128 Bytes, max_count = 20000,
	 * (128 * 20000) / 1024 = 2500 K == 2.44M,
	 * so can not test case: readbatch > MAX_BATCH_SIZE */
	int max_size = 20 * 1000 - 1; // maxsize > MAX_BATCH_SIZE(8M)
	int small_size = 1024;

	printf("1,__test_readbatch_get_maxbatchsize() will pass\n");
	if (__test_readbatch_get_maxbatchsize(db, roptions, woptions, small_size) != 0) {
		printf("test_readbatch_get_maxbatchsize small size fail\n");
		exit(-1);
	}
	/*
	printf("2,__test_readbatch_get_maxbatchsize() will fail\n");
	if (__test_readbatch_get_maxbatchsize(db, roptions, woptions, max_size) == 0) {
		printf("test_readbatch_get_maxbatchsize max_size should fail, but not!\n");
	}
	*/
	printf("1,__test_readbatch_get_maxcount will pass, count=%d\n", max_count - 1);
	if (__test_readbatch_get_maxcount(db, roptions, woptions, max_count - 1) != 0) {
		printf("test_readbatch get maxcount=%d read fail\n", max_count - 1);
		exit(-1);
	}
	printf("2,__test_readbatch_get_maxcount will pass, count=%d\n", max_count);
	if (__test_readbatch_get_maxcount(db, roptions, woptions, max_count) != 0) {
		printf("test_readbatch get maxcount=%d read fail\n", max_count);
		exit(-1);
	}
	printf("3,__test_readbatch_get_maxcount will fail, count=%d\n", max_count + 1);
	if (__test_readbatch_get_maxcount(db, roptions, woptions, max_count + 1) == 0) {
		printf("test readbatch get maxcount=%d should fail, but not!", max_count + 1);
		exit(-1);
	}
	printf("test readbatch max end!\n");

	return ret;
}

int test_readbatch_clear(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int times = 1024;
	int i, ret = 0;
	int failed_cmd_count = 0;
	char keybuf[100];
	char *err = NULL;
	char **value = (char **)malloc(times * sizeof(char *));
	unsigned int *value_len_addr = malloc(times * sizeof(unsigned int));
	unsigned int *status = malloc(times * sizeof(unsigned int));

	db_readbatch_t *rb = shannon_db_readbatch_create();
	for (i = 0; i < times; i++) {
		memset(keybuf, 0 , 100);
		snprintf(keybuf, sizeof(keybuf), "keycount+%d-", i);
		value[i] = malloc(100);
		memset(value[i], 0, 100);
		shannon_db_readbatch_get(rb, keybuf, strlen(keybuf), value[i], VALUE_LEN,
				value_len_addr + i, status + i, &err);
		if (err) {
			printf("line=%d: shannon_db_readbatch_get ioctl failed: %s\n", __LINE__, err);
			exit(-1);
		}
	}

	printf("1,read before shannon_db_readbatch_clear, will pass\n");
	shannon_db_read(db, roptions, rb, &failed_cmd_count, &err);
	if (err) {
		printf("line=%d: shannon_db_read() ioctl failed: %s\n", __LINE__, err);
		exit(-1);
	}
	for (i = 0; i < times; i++) {
		test_check_counts_value(i, value[i], strlen(value[i]));
	}

	err = NULL;
	shannon_db_readbatch_clear(rb);
	printf("2,read after shannon_db_readbatch_clear, will fail\n");
	shannon_db_read(db, roptions, rb, &failed_cmd_count, &err);
	if (failed_cmd_count != times) {
		printf("line=%d: failed_cmd_count=%d\n", __LINE__, failed_cmd_count);
		printf("line=%d: shannon_db_readbatch_get ioctl failed, err=%s\n", __LINE__, err);
		goto out;
	} else {
		printf("shannon_db_readbatch_clear should fail, but not!\n");
		exit(-1);
	}

out:
	for (i = 0; i < times; i++) {
		if (value[i])
			free(value[i]);
	}
	free(value);
	shannon_db_readbatch_destroy(rb);
	return ret;
}

int test_readbatch_not_exist(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int i, ret = 0;
	int times = 100;
	unsigned int failed_cmd_count = 0;
	char keybuf[BUF_LEN];
	char *err = NULL;
	char **value = (char **)malloc(times * sizeof(char *));
	unsigned int *value_len_addr = malloc(times * sizeof(unsigned int));
	unsigned int *status = malloc(times * sizeof(unsigned int));

	memset(keybuf, 0, BUF_LEN);
	snprintf(keybuf, BUF_LEN, "%d", 90);
	shannon_db_delete(db, woptions, keybuf, strlen(keybuf), &err);
	if (err) {
		printf("line=%d:shannon_db_delete failed\n", __LINE__);
		exit(-1);
	}

	db_readbatch_t *rb = shannon_db_readbatch_create();
	for (i = 0; i < times; i++) {
		memset(keybuf, 0 , BUF_LEN);
		snprintf(keybuf, BUF_LEN, "%d", i);
		value[i] = malloc(BUF_LEN);
		memset(value[i], 0, BUF_LEN);
		shannon_db_readbatch_get(rb, keybuf, strlen(keybuf), value[i], BUF_LEN,
				value_len_addr + i, status + i, &err);
		if (err) {
			printf("line=%d: shannon_db_readbatch_get ioctl failed: %s\n", __LINE__, err);
			exit(-1);
		}
	}

	shannon_db_read(db, roptions, rb, &failed_cmd_count, &err);
	if (failed_cmd_count) {
		printf("line=%d: failed_cmd_count=%d\n", __LINE__, failed_cmd_count);
		printf("line=%d: shannon_db_read() ioctl failed: %s\n", __LINE__, err);
		goto out;
	} else {
		printf("test_readbatch_not_exist should fail, but not!\n");
		exit(-1);
	}
out:
	for (i = 0; i < times; i++) {
		if (value[i])
			free(value[i]);
	}
	free(value);
	shannon_db_readbatch_destroy(rb);
	return ret;
}


int main()
{
	struct shannon_db *db;
	struct db_options *options;
	struct db_readoptions *roptions;
	struct db_writeoptions *woptions;
	int status;
	int total_count;
	struct shannon_db_iterator *iter;
	char *err = NULL;
	start_phase("create_objects");
	{
		options = shannon_db_options_create();
		woptions = shannon_db_writeoptions_create();
		roptions = shannon_db_readoptions_create();
		shannon_db_writeoptions_set_sync(woptions, 1);
		shannon_db_options_set_create_if_missing(options, 1);
		start_phase("open");
		db = shannon_db_open(options, "/dev/kvdev0", "test_readbatch.db", &err);
		if (err) {
			printf("test_readbatch open_db ioctl failed\n");
			exit(-1);
		}
	}

	start_phase("testcase");
	{
		start_phase("before test_readbatch, put some data to database");
		test_put_char(db, roptions, woptions);
		test_put_byte(db, roptions, woptions);
		test_put_kbytes(db, roptions, woptions);
		test_put_counts(db, roptions, woptions);
		start_phase("test_readbatch_case1(): readbatch times frequently");
		test_readbatch_case1(db, roptions, woptions);
		start_phase("test_readbatch_empty(): readbatch with empty data, will fail");
		test_readbatch_empty(db, roptions, woptions);
		start_phase("test_writebatch_single_get(): readbatch just one key, readbatch times frequently");
		test_readbatch_single_get(db, roptions, woptions);
		start_phase("test_readbatch_multi_get(): readbatch has multi keys");
		test_readbatch_multi_get(db, roptions, woptions);
		start_phase("test_readbatch_max(): MAX_READBATCH_COUNT(20000) or MAX_BATCH_SIZE(8M)");
		test_readbatch_max(db, roptions, woptions);
		start_phase("test_readbatch_clear(): read twice before and after clear readbatch cmd");
		test_readbatch_clear(db, roptions, woptions);
		start_phase("test_readbatch_not_exist(): remove one key form database, then readbatch");
		test_readbatch_not_exist(db, roptions, woptions);
	}

	start_phase("cleanup");
	{
		shannon_destroy_db(db, &err);
		if (err) {
			printf("test_writebatch_multi_threads destroy_db ioctl  failed\n");
			exit(-1);
		}
		shannon_db_options_destroy(options);
		shannon_db_readoptions_destroy(roptions);
		shannon_db_writeoptions_destroy(woptions);
		shannon_db_close(db);
	}

	printf("PASS\n");
	return 0;
}
