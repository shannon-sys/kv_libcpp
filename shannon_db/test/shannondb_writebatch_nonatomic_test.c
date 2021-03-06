#include <shannon_db.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

char *phase = "";
#define BUF_LEN 128
#define KEY_LEN 12

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

int test_writebatch_delete_nonatomic(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int times = 255;
	int i = 0, ret = 0;
	int value_len;
	char key[BUF_LEN];
	char buffer[BUF_LEN];
	char *err = NULL;
	db_writebatch_t *wb = shannon_db_writebatch_create_nonatomic();
	for (i = 0; i < times; i++) {
		memset(key, 0, BUF_LEN);
		snprintf(key, BUF_LEN, "%d", i);
		shannon_db_writebatch_delete_nonatomic(wb, key, strlen(key), &err);
		if (err) {
			printf("shannon_db_writebatch_delete_nonatomic  ioctl failed\n");
			exit(-1);
		}
	}
	shannon_db_write_nonatomic(db, woptions, wb, &err);
	if (err) {
		printf("test_writebatch_delete_nonatomic write ioctl failed\n");
		exit(-1);
	}
	for (i = 0; i < times; i++) {
		err = NULL;
		memset(key, 0, BUF_LEN);
		memset(buffer, 0, BUF_LEN);
		snprintf(key, BUF_LEN, "%d", i);
		shannon_db_get(db, roptions, key, strlen(key), buffer, BUF_LEN, &value_len, &err);
		if (value_len > 0) {
			printf("test_writebatch_delete %s --%sfailed\n", key, buffer);
			exit(-1);
		}
	}
	shannon_db_writebatch_destroy_nonatomic(wb);
	return 0;

}

int test_writebatch_clear_nonatomic(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int times = 255;
	int i = 0, ret = 0;
	int value_len;
	char key[BUF_LEN];
	char value[BUF_LEN];
	char buffer[BUF_LEN];
	char *err = NULL;
	db_writebatch_t *wb = shannon_db_writebatch_create_nonatomic();
	for (i = 0; i < times; i++) {
		snprintf(key, BUF_LEN, "%d", i);
		snprintf(value, BUF_LEN, "%d", i);
		shannon_db_writebatch_put_nonatomic(wb, key, strlen(key), value, strlen(value), &err);
		if (err) {
			printf("shannon_db_writebatch_put_nonatomic  ioctl failed\n");
			exit(-1);
		}
	}
	shannon_db_writebatch_clear_nonatomic(wb);
	shannon_db_writebatch_destroy_nonatomic(wb);
	return 0;

}

int test_writebatch_put_nonatomic(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int times = 255;
	int i = 0, ret = 0;
	int value_len = 0;
	char key[BUF_LEN];
	char buffer[BUF_LEN];
	char **value = (char **)malloc(times * sizeof(char *));
	char *err = NULL;
	db_writebatch_t *wb = shannon_db_writebatch_create_nonatomic();
	for (i = 0; i < times; i++) {
		memset(key, 0, BUF_LEN);
		snprintf(key, BUF_LEN, "%d", i);
		value[i] = randomstr();
		shannon_db_writebatch_put_nonatomic(wb, key, strlen(key), value[i], strlen(value[i]), &err);
		if (err) {
			printf("shannon_db_writebatch_put_nonatomic ioctl failed\n");
			exit(-1);
		}
	}
	shannon_db_write_nonatomic(db, woptions, wb, &err);
	if (err) {
		printf("shannon_db_writebatch_put_nonatomic  ioctl failed\n");
		exit(-1);
	}
	for (i = 0; i < times; i++) {
		memset(key, 0, BUF_LEN);
		memset(buffer, 0, BUF_LEN);
		snprintf(key, BUF_LEN, "%d", i);
		shannon_db_get(db, roptions, key, strlen(key), buffer, BUF_LEN, &value_len, &err);
		if (err) {
			printf("test_writebatch_put_nonatomic %s failed\n", key);
			exit(-1);
		}
		if (strcmp(value[i], buffer)) {
			printf("dismatch value:%s buffer:%s\n", value[i], buffer);
			exit(-1);
		}
		free(value[i]);
	}
	free(value);
	shannon_db_writebatch_destroy_nonatomic(wb);
	return 0;
}
int test_writebatch_empty_nonatomic(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int ret;
	char *err = NULL;
	db_writebatch_t *wb = shannon_db_writebatch_create_nonatomic();
	shannon_db_write_nonatomic(db, woptions, wb, &err);
	if (err) {
		printf("shannon_db_writebatch_put_nonatomic  ioctl failed\n");
		exit(-1);
	}
	shannon_db_writebatch_destroy_nonatomic(wb);
	return 0;
}

int test_writebatch_case1_nonatomic(int times, struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int i;
	int ret;
	int value_len;
	char key[BUF_LEN];
	char buffer[BUF_LEN];
	char *value;
	char *err = NULL;
	for (i = 0; i < times; i++) {
		db_writebatch_t *wb = shannon_db_writebatch_create_nonatomic();
		memset(key, 0 , BUF_LEN);
		snprintf(key, BUF_LEN, "%d", i);
		value = randomstr();
		shannon_db_writebatch_put_nonatomic(wb, key, strlen(key), value, strlen(value), &err);
		if (err) {
			printf("shannon_db_writebatch_put_nonatomic  ioctl failed\n");
			exit(-1);
		}
		shannon_db_write_nonatomic(db, woptions, wb, &err);
		if (err) {
			printf("test_writebatch_case1_nonatomic failed\n");
			exit(-1);
		}
		memset(buffer, 0 , BUF_LEN);
		shannon_db_get(db, roptions, key, strlen(key), buffer, BUF_LEN, &value_len, &err);
		if (err) {
			printf("test_writebatch_case1_nonatomic get failed\n");
			exit(-1);
		}
		if (strcmp(value, buffer)) {
			printf("dismatch value:%s buffer:%s len=%d--%d\n", value, buffer, value_len, (int)strlen(value));
			exit(-1);
		}
		free(value);
		shannon_db_writebatch_destroy_nonatomic(wb);
	}
}
struct io_thread {
	int id;
	pthread_t tid;
	int nthread;
	int state;
	int write_count;
	struct io_check *check;
	volatile int terminate;
};

struct io_check {
	pthread_t tid;
	int nthread;
	__u64 num;
	int ready;
	int complete_nthread;
	pthread_cond_t ready_cond;
	pthread_cond_t ready_mutex;
	struct io_thread *thread;
	struct db_writeoptions *write_options;
	struct db_readoptions *read_options;
	struct shannon_db *db;
};

void *run_thread(void *args)
{
	struct io_thread *thread = NULL;
	struct io_check *check = NULL;
	char key[KEY_LEN];
	char value[BUF_LEN];
	int ret;
	__u64  seq_key = 0;
	char *err = NULL;
	thread = (struct io_thread *)args;
	check  = thread->check;
	struct db_writeoptions *write_options = check->write_options;
	struct db_readoptions *read_options = check->read_options;
	struct shannon_db *db = check->db;
	if (!db) {
		printf("write_batch_multi_threads db is null!!!\n");
		exit(-1);
	}
	if (!write_options) {
		printf("write_batch_multi_threads write_options is null!!!\n");
		exit(-1);
	}
	if (!read_options) {
		printf("write_batch_multi_threads read_options is null!!!\n");
		exit(-1);
	}
	db_writebatch_t *wb = shannon_db_writebatch_create_nonatomic();
	while (!thread->terminate) {
		char *err1 = NULL;
		seq_key++;
		snprintf(key, KEY_LEN, "%lld", seq_key + check->num*thread->id);
		snprintf(value, BUF_LEN, "%lld", seq_key);
		shannon_db_writebatch_put_nonatomic(wb, key, strlen(key), value, strlen(value), &err1);
		if (err1) {
			printf("test_writebatch_multi_threads put failed %s\n", err1);
		}
		thread->write_count++;
		if (check->num && (thread->write_count >= check->num))
			thread->terminate = 1;
	}
	shannon_db_write_nonatomic(db, write_options, wb, &err);
	if (err) {
		printf("test_writebatch_multi_threads  test_writebatch ioctl  failed\n");
		exit(-1);
	}
	shannon_db_writebatch_destroy_nonatomic(wb);
	return NULL;
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
int main()
{
	struct shannon_db *db;
	struct io_thread *thread;
	struct io_check *check;
	struct db_options *options;
	struct db_readoptions *roptions;
	struct db_writeoptions *woptions;
	int status;
	int total_count;
	struct shannon_db_iterator *iter;
	int thread_num;
	int n = 0;
	char *err = NULL;
	StartPhase("create_objects");
	{
		options = shannon_db_options_create();
		woptions = shannon_db_writeoptions_create();
		roptions = shannon_db_readoptions_create();
		shannon_db_writeoptions_set_sync(woptions, 1);
		shannon_db_options_set_create_if_missing(options, 1);
		StartPhase("open");
		db = shannon_db_open(options, "/dev/kvdev0", "test.db", &err);
		if (err) {
			printf("test_writebatch_multi_threads  open_db ioctl  failed\n");
			exit(-1);
		}
	}
	StartPhase("testcase");
	{
		StartPhase("test_writebatch_case1_nonatomic write writebatch times frequently");
		test_writebatch_case1_nonatomic(100, db, roptions, woptions);
		StartPhase("test_writebatch_put_nonatomic write writebatch times");
		test_writebatch_put_nonatomic(db, roptions, woptions);
		StartPhase("test_writebatch_delete_nonatomic delete kv");
		test_writebatch_delete_nonatomic(db, roptions, woptions);
		StartPhase("test_writebatch_clear_nonatomic memset writebatch");
		test_writebatch_clear_nonatomic(db, roptions, woptions);
	}
	StartPhase("multi threads");
	{
		thread_num = 2;
		check = malloc(sizeof(*check));
		memset(check, 0x00, sizeof(*check));
		if (!check) {
			printf("write_batch_multi_threads malloc check failed!!!\n");
			exit(-1);
		}
		check->db = db;
		check->num = 255;
		check->write_options = woptions;
		check->read_options = roptions;
		check->thread = malloc(sizeof(*check->thread) * thread_num);
		if (NULL == check->thread) {
			printf("malloc check->thread failed!!!\n");
			return -1;
		}
		memset(check->thread, 0x00, sizeof(*check->thread) * thread_num);
		for (n = 0; n < thread_num; n++) {
			thread = check->thread + n;
			thread->id = n;
			thread->check = check;
			thread->write_count = 0;
		}
		for (n = 0; n < thread_num; n++) {
			thread = check->thread + n;
			status = pthread_create(&check->thread[n].tid, NULL, run_thread, (void *)(check->thread + n));
		}
		for (n = 0; n < thread_num; n++) {
			status = pthread_join(check->thread[n].tid, (void **)&thread);
			if (status) {
				printf("pthread_join failed: %s\n", strerror(status));
				return EXIT_FAILURE;
			}
		}
		iter = shannon_db_create_iterator(db, roptions);
		assert (510 == test_iter_get_count(iter));
	}
	StartPhase("cleanup");
	{
		free(check->thread);
		free(check);
		shannon_db_iter_destroy(iter);
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
	fprintf(stderr, "PASS\n");
	return 0;
}
