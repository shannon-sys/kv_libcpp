#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "../shannon_db.h"

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

struct io_thread {
	int id;
	pthread_t tid;
	struct io_check *check;
};

struct io_check {
	int nthread;
	struct io_thread *thread;
	struct shannon_db *db;
	int loop;
	int err;
};

void *run_thread(void *args)
{
	struct io_thread *thread = NULL;
	struct io_check *check = NULL;
	struct shannon_db *db = NULL;
	char *err = NULL;
	int i;

	thread = (struct io_thread *)args;
	check  = thread->check;
	db = check->db;

	for (i = 0; i < check->loop; ++i) {
		if (check->err) {
			break;
		}
		__u64 timestamp = shannon_db_create_snapshot(db, &err);
		if (err) {
			fprintf(stderr, "create snapshot failed: %s\n", err);
			check->err = -1;
			break;
		}
		shannon_db_release_snapshot(db, timestamp, &err);
		if (err) {
			fprintf(stderr, "release snapshot failed: %s, timestamp=0x%016llx\n", err, timestamp);
			check->err = -1;
			break;
		}
	}

	return NULL;
}

int test_snapshot_multi_thread(struct shannon_db *db, int thread_num, int loop)
{
	struct io_check *check = NULL;
	struct io_thread *thread = NULL;
	int status;
	int i;
	int ret = -1;

	check = malloc(sizeof(*check));
	if (check == NULL) {
		fprintf(stderr, "create io_check failed.\n");
		return -1;
	}
	memset(check, 0, sizeof(*check));
	check->nthread = thread_num;
	check->loop = loop;
	check->db = db;
	check->err = 0;
	check->thread = malloc(sizeof(*check->thread) * thread_num);
	if (NULL == check->thread) {
		fprintf(stderr, "malloc check->thread failed.\n");
		ret = EXIT_FAILURE;
		goto out;
	}
	for (i = 0; i < thread_num; i++) {
		thread = check->thread + i;
		thread->id = i;
		thread->check = check;
	}

	for (i = 0; i < thread_num; i++) {
		thread = check->thread + i;
		status = pthread_create(&check->thread[i].tid, NULL, run_thread, (void *)(check->thread + i));
		if (status) {
			fprintf(stderr, "pthread_create failed[%d]: %s\n", i, strerror(status));
			ret = EXIT_FAILURE;
			goto out;
		}
	}
	for (i = 0; i < thread_num; i++) {
		status = pthread_join(check->thread[i].tid, NULL);
		if (status) {
			fprintf(stderr, "pthread_join failed[%d]: %s\n", i, strerror(status));
			ret = EXIT_FAILURE;
			goto out;
		}
	}
	ret = check->err;
out:
	if (check) {
		if (check->thread) {
			free(check->thread);
		}
		free(check);
	}
	return ret;
}

int test_snapshot_create(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int loop_times = 100;
	int i;
	int ret;
	int value_len;
	__u64 timestamp;
	char key[BUF_LEN];
	char value1[BUF_LEN];
	char value2[BUF_LEN];
	char buffer[BUF_LEN];
	char *err = NULL;
	for (i = 0; i < loop_times; i++) {
		memset(key, 0, sizeof(key));
		memset(buffer, 0, sizeof(buffer));
		memset(value1, 0, sizeof(value1));
		memset(value2, 0, sizeof(value2));
		snprintf(key, BUF_LEN, "%d", i);
		snprintf(value1, BUF_LEN, "%d", i);
		snprintf(value2, BUF_LEN, "%d", i + 1);
		shannon_db_put(db, woptions, key, strlen(key), value1, strlen(value1), &err);
		if (err != NULL) {
			printf("test_snapshot shannondb_put failed\n");
			exit(-1);
		}
		timestamp = shannon_db_create_snapshot(db, &err);
		if ((timestamp < 0) || (err)) {
			printf("create_snapshot--failed!!!.\n");
			exit(-1);
		}
		shannon_db_put(db, woptions, key, strlen(key), value2, strlen(value2), &err);
		if (err != NULL) {
			printf("test_snapshot shannondb_put failed\n");
			exit(-1);
		}
		shannon_db_readoptions_set_snapshot(roptions, timestamp);
		shannon_db_get(db, roptions, key, strlen(key), buffer, BUF_LEN, &value_len, &err);
		if (err != NULL) {
			printf("test_snapshot_get failed\n");
			exit(-1);
		}
		if (strcmp(value1, buffer)) {
			printf("dismatch value:%s buffer:%s\n", value1, buffer);
			exit(-1);
		}
		shannon_db_release_snapshot(db, timestamp, &err);
		if (err != NULL) {
			printf("test_snapshot_create shannondb_release failed\n");
			exit(-1);
		}
	}
}
int test_snapshot_get(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	__u64 timestamp;
	int times = 300;
	int i = 0, ret = 0;
	int value_len;
	char key[BUF_LEN];
	char value[BUF_LEN];
	char buffer[BUF_LEN];
	char *err = NULL;
	timestamp = shannon_db_create_snapshot(db, &err);
	if ((err != NULL) || (timestamp < 0)) {
		printf("test_snapshot shannondb_db_create_snapshot  failed\n");
		exit(-1);
	}
	for (i = 200; i < times; i++) {
		memset(key, 0, sizeof(key));
		memset(value, 0, sizeof(value));
		snprintf(key, BUF_LEN, "%d", i);
		snprintf(value, BUF_LEN, "%d", i);
		shannon_db_put(db, woptions, key, strlen(key), value, strlen(value), &err);
		if (err != NULL) {
			printf("test_snapshot shannondb_put after create snapshot failed\n");
			exit(-1);
		}
		err = NULL;
	}
	shannon_db_readoptions_set_snapshot(roptions, timestamp);
	for (i = 200; i < times; i++) {
		memset(key, 0, sizeof(key));
		memset(buffer, 0, sizeof(buffer));
		snprintf(key, BUF_LEN, "%d", i);
		shannon_db_get(db, roptions, key, strlen(key), buffer, BUF_LEN, &value_len, &err);
		if (value_len > 0) {
			printf("test_snapshot shannondb_get from snapshot 0 failed\n");
			exit(-1);
		}
		err = NULL;
	}
	shannon_db_readoptions_set_snapshot(roptions, 0);
	for (i = 200; i < times; i++) {
		memset(key, 0, sizeof(key));
		memset(buffer, 0, sizeof(buffer));
		snprintf(key, BUF_LEN, "%d", i);
		shannon_db_get(db, roptions, key, strlen(key), buffer, BUF_LEN, &value_len, &err);
		if (err != NULL) {
			printf("test_snapshot shannondb_get after set snapshot failed\n");
			exit(-1);
		}
		err = NULL;
	}
	shannon_db_release_snapshot(db, timestamp, &err);
	if (err != NULL) {
		printf("test_snapshot_get failed\n");
		exit(-1);
	}
	return 0;
}

int test_snapshot_release(struct shannon_db *db, struct db_readoptions *roptions, struct db_writeoptions *woptions)
{
	int times = 600;
	int i;
	int ret;
	__u64 timestamp1;
	__u64 timestamp2;
	char buffer[BUF_LEN];
	char key[BUF_LEN];
	char value[BUF_LEN];
	int value_len;
	char *err = NULL;
	for (i = 500; i < times; i++) {
		memset(key, 0, sizeof(key));
		memset(buffer, 0, sizeof(buffer));
		snprintf(key, BUF_LEN, "%d", i);
		snprintf(value, BUF_LEN, "a%d", i);
		shannon_db_put(db, woptions, key, strlen(key), value, strlen(value), &err);
		if (err != NULL) {
			printf("test_snapshot shannondb_put failed\n");
			exit(-1);
		}
	}
	timestamp1 = shannon_db_create_snapshot(db, &err);
	if ((err != NULL) || (timestamp1 < 0)) {
		printf("test_snapshot shannondb_db_create_snapshot  failed\n");
		exit(-1);
	}
	for (i = 500; i < times; i++) {
		memset(key, 0, sizeof(key));
		memset(buffer, 0, sizeof(buffer));
		snprintf(key, BUF_LEN, "%d", i);
		snprintf(value, BUF_LEN, "b%d", i);
		shannon_db_put(db, woptions, key, strlen(key), value, strlen(value), &err);
		if (err != NULL) {
			printf("test_snapshot shannondb_put failed\n");
			exit(-1);
		}
	}
	timestamp2 = shannon_db_create_snapshot(db, &err);
	if (err != NULL) {
		printf("test_snapshot shannondb_db_create_snapshot  failed\n");
		exit(-1);
	}
	for (i = 500; i < times; i++) {
		memset(key, 0, sizeof(key));
		memset(buffer, 0, sizeof(buffer));
		snprintf(key, BUF_LEN, "%d", i);
		snprintf(value, BUF_LEN, "c%d", i);
		shannon_db_put(db, woptions, key, strlen(key), value, strlen(value), &err);
		if (err != NULL) {
			printf("test_snapshot shannondb_put failed\n");
			exit(-1);
		}
	}

	shannon_db_readoptions_set_snapshot(roptions, 0);
	for (i = 500; i < times; i++) {
		memset(key, 0, sizeof(key));
		memset(buffer, 0, sizeof(buffer));
		snprintf(key, BUF_LEN, "%d", i);
		snprintf(value, BUF_LEN, "c%d", i);
		shannon_db_get(db, roptions, key, strlen(key), buffer, BUF_LEN, &value_len, &err);
		if (err != NULL) {
			printf("test_snapshot_release get from snaoshot 0\n");
			exit(-1);
		}
		if (strcmp(value, buffer)) {
			printf("snapshot 0 dismatch value:%s buffer:%s\n", value, buffer);
			exit(-1);
		}
	}
	shannon_db_readoptions_set_snapshot(roptions, timestamp2);
	for (i = 500; i < times; i++) {
		memset(key, 0, sizeof(key));
		memset(buffer, 0, sizeof(buffer));
		snprintf(key, BUF_LEN, "%d", i);
		snprintf(value, BUF_LEN, "b%d", i);
		shannon_db_get(db, roptions, key, strlen(key), buffer, BUF_LEN, &value_len, &err);
		if (err != NULL) {
			printf("test_snapshot_release get from snapshot 2\n");
			exit(-1);
		}
		if (strcmp(value, buffer)) {
			printf("snapshot 2 dismatch value:%s buffer:%s\n", value, buffer);
			exit(-1);
		}
	}
	shannon_db_readoptions_set_snapshot(roptions, timestamp1);
	for (i = 500; i < times; i++) {
		memset(key, 0, sizeof(key));
		memset(buffer, 0, sizeof(buffer));
		snprintf(key, BUF_LEN, "%d", i);
		snprintf(value, BUF_LEN, "a%d", i);
		shannon_db_get(db, roptions, key, strlen(key), buffer, BUF_LEN, &value_len, &err);
		if (err != NULL) {
			printf("test_snapshot_release get from snapshot 1\n");
			exit(-1);
		}
		if (strcmp(value, buffer)) {
			printf("snapshot 1 dismatch value:%s buffer:%s\n", value, buffer);
			exit(-1);
		}
	}
	shannon_db_release_snapshot(db, timestamp1, &err);
	if (err != NULL) {
		printf("test_snapshot shannondb_db_release_snapshot  failed\n");
		exit(-1);
	}
	shannon_db_release_snapshot(db, timestamp2, &err);
	if (err != NULL) {
		printf("test_snapshot shannondb_db_release_snapshot  failed\n");
		exit(-1);
	}
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
	int ret;
	int i;

	StartPhase("create_objects");
	CheckCondition(dbname != NULL);
	options = shannon_db_options_create();
	woptions = shannon_db_writeoptions_create();
	roptions = shannon_db_readoptions_create();
	shannon_db_writeoptions_set_sync(woptions, 1);
	shannon_db_options_set_create_if_missing(options, 1);

	StartPhase("open");
	db = shannon_db_open(options, "/dev/kvdev0", "test.db", &err);
	if (err != NULL) {
		printf("test_snapshot shannondb_put failed\n");
		exit(-1);
	}
	StartPhase("snapshot-multi-thread");
	{
		for (i = 0; i < 10; i++) {
			ret = test_snapshot_multi_thread(db, 16, 1000000);
			if (ret) {
				fprintf(stderr, "test_snapshot_multi_thread failed\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	StartPhase("snapshot");
	{
		StartPhase("test_snapshot_create multi times");
		test_snapshot_create(db, roptions, woptions);
		StartPhase("test_snapshot_get check snapshot ");
		test_snapshot_get(db, roptions, woptions);
		StartPhase("test_snapshot_release check snapshot release");
		test_snapshot_release(db, roptions, woptions);
	}

	StartPhase("cleanup");
	{
		shannon_destroy_db(db, &err);
		if (err != NULL) {
			printf("test_snapshot shannondb_destroy failed\n");
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
