#include <shannon_db.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>
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
		fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, phase, (err)); \
		abort(); \
	} })
#define WRITE_BATCH_BIT 0
#define PUT_BIT 1
#define zero_bit(var) { var = 0; }
#define set_bit(var, bit) { var = (var | (1 << bit)); }
#define unset_bit(var, bit) { var = (var & (~(1 << bit))) }
#define get_bit(var, bit) ((var >> bit) & 1)


static void CheckEqual(char *expected, const char *v, size_t n)
{
	if (expected == NULL && v == NULL) {
	} else if (expected != NULL && v != NULL && n == strlen(expected) && memcmp(expected, v, n) == 0) {
		return;
	} else {
		fprintf(stderr, "%s: expected '%s', got '%s'\n", phase, (expected ? expected : "(null)"), (v ? v : "(null)"));
		abort();
	}
}

int is_digit(char *buf)
{
	return (strspn(buf, "0123456789") == strlen(buf));
}

unsigned long get_current_time(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

int numbers_of_write = 1000000;
int numbers_of_threads = 10;
int count = 1000;
int limit = 0;
int write_mode;

void run_write_batch(struct shannon_db *db, struct db_writeoptions *write_options, struct db_readoptions *read_options, db_writebatch_t *write_batch, char *db_name)
{
	char **values;
	int key_len, value_len;
	int i, j, k, ret, batch_count = 100;
	unsigned long start_time, end_time;
	unsigned long temp_start_time, temp_end_time;
	long write_data_size = 0, total_write_data_size = 0, temp_data_size;
	long read_data_size = 0, total_read_data_size = 0;
	long write_time = 0, total_write_time = 0, read_time = 0, total_read_time = 0;
	char key[128], value[1024], temp_value[1024];
	char *err = NULL;

	for (j = 0; j < numbers_of_write / count; j++) {
		temp_start_time = get_current_time();
		for (i = 0; i < count / batch_count; i++) {
			temp_data_size = 0;
			values = (char **)malloc(sizeof(char *)*batch_count);
			CheckCondition(values != NULL);
			for (k = 0; k < batch_count; k++) {
				sprintf(key, "key%d", j * count + i * batch_count + k);
				*(values+k) = (char *)malloc(sizeof(char)*(strlen(key)+3));
				CheckCondition(*(values+k) != NULL);
				sprintf(*(values+k), "value%d", j * count + i * batch_count + k);
				shannon_db_writebatch_put(write_batch, key, strlen(key), *(values+k), strlen(*(values+k)), &err);
				CheckNoError(err);
				temp_data_size += strlen(*(values+k));
			}
			shannon_db_write(db, write_options, write_batch, &err);
			CheckNoError(err);
			shannon_db_writebatch_clear(write_batch);
			for (k = 0; k < batch_count; k++)
				free(*(values+k));
			free(values);
			write_data_size += temp_data_size;
		}
		temp_end_time = get_current_time();
		write_time += (temp_end_time - temp_start_time);

		temp_start_time = get_current_time();
		/* read and check data */
		for (k = 0; k < count; k++) {
			sprintf(key, "key%d", j * batch_count + k);
			shannon_db_get(db, read_options, key, strlen(key), value, BUF_LEN, &value_len, &err);
			CheckNoError(err);
			sprintf(temp_value, "value%d", j * batch_count + k);
			CheckCondition(value_len == strlen(temp_value));
			value[value_len] = 0;
			CheckEqual(value, temp_value, value_len);
			read_data_size += value_len;
		}
		temp_end_time = get_current_time();
		end_time = get_current_time();
		read_time += (temp_end_time - temp_start_time);

		if (end_time >= start_time + 1000000) {
			total_write_data_size += write_data_size;
			total_read_data_size += read_data_size;
			total_write_time += write_time;
			total_read_time += read_time;
			printf("db: %s | Real-time write: %.2lfMB/s read: %.2lfMB/s | Average write: %.2lfMB/s read: %.2lfMB/s\n",
			db_name,
			(double)write_data_size*1000000.0/(double)write_time/((double)1024.0*1024.0),
			(double)read_data_size*1000000.0/(double)read_time/((double)1024.0*1024.0),
			(double)total_write_data_size*1000000.0/((double)(total_write_time))/(double)(1024.0*1024.0),
			(double)total_read_data_size*1000000.0/((double)(total_read_time))/(double)(1024.0*1024.0));
			write_data_size = 0;
			read_data_size = 0;
			read_time = 0;
			write_time = 0;
			start_time = end_time;
		}
	}
}

void run_put(struct shannon_db *db, struct db_writeoptions *write_options, struct db_readoptions *read_options, char *db_name)
{
	char **values;
	int key_len, value_len;
	int i, j, k, ret;
	unsigned long start_time, end_time;
	long temp_start_time, temp_end_time;
	long write_data_size = 0, total_write_data_size = 0, temp_data_size;
	long read_data_size = 0, total_read_data_size = 0;
	long write_time = 0, total_write_time = 0, read_time = 0, total_read_time = 0;
	char key[128], value[1024], temp_value[1024];
	char *err = NULL;

	start_time = get_current_time();
	for (j = 0; j < numbers_of_write / count; j++) {
		/* write data */
		temp_start_time = get_current_time();
		for (k = 0; k < count; k++) {
			sprintf(key, "key%d", j * count + k);
			sprintf(value, "value%d", j * count + k);
			shannon_db_put(db, write_options, key, strlen(key), value, strlen(value), &err);
			CheckNoError(err);
			write_data_size += strlen(value);
		}
		temp_end_time = get_current_time();
		write_time += (temp_end_time - temp_start_time);

		temp_start_time = get_current_time();
		/* read data */
		for (k = 0; k < count; k++) {
			sprintf(key, "key%d", j * count + k);
			shannon_db_get(db, read_options, key, strlen(key), value, BUF_LEN, &value_len, &err);
			CheckNoError(err);
			sprintf(temp_value, "value%d", j * count + k);
			CheckCondition(value_len == strlen(temp_value));
			value[value_len] = 0;
			CheckEqual(value, temp_value, value_len);
			read_data_size += value_len;
		}
		temp_end_time = get_current_time();
		read_time += (temp_end_time - temp_start_time);

		end_time = get_current_time();
		if (end_time >= start_time + 1000000) {
			total_write_data_size += write_data_size;
			total_read_data_size += read_data_size;
			total_read_time += read_time;
			total_write_time += write_time;
			printf("db: %s | Real-time write: %.2lfMB/s read: %.2lfMB/s | Average write: %.2lfMB/s read: %.2lfMB/s\n",
				db_name,
				(double)write_data_size*1000000.0/(double)write_time/((double)(1024.0*1024.0)),
				(double)read_data_size*1000000.0/(double)read_time/((double)(1024.0*1024.0)),
				(double)total_write_data_size*1000000.0/((double)total_write_time)/(double)(1024.0*1024.0),
				(double)total_read_data_size*1000000.0/((double)total_read_time)/(double)(1024.0*1024.0));
				write_data_size = 0;
				read_data_size = 0;
				write_time = 0;
				read_time = 0;
				start_time = end_time;
		}
	}
}

void *pthread_delete_database(void *arg)
{
	struct shannon_db *db;
	struct db_writeoptions *write_options;
	struct db_readoptions *read_options;
	struct db_options *options = NULL;
	struct db_iterator *iterator = NULL;
	int run_time = 0;
	char db_name[128];
	char *err = NULL;
	db_writebatch_t *write_batch = NULL;

	sprintf(db_name, "db%d", *((int *)arg));
	options = shannon_db_options_create();
	if (options == NULL) {
		printf("options create failed!!!\n");
		return NULL;
	}
	write_options = shannon_db_writeoptions_create();
	if (write_options == NULL) {
		printf("write options create failed!!!\n");
		shannon_db_options_destroy(options);
		return NULL;
	}
	read_options = shannon_db_readoptions_create();
	if (read_options == NULL) {
		printf("read options create failed!!!\n");
		shannon_db_writeoptions_destroy(write_options);
		shannon_db_options_destroy(options);
		return NULL;
	}
	write_batch = shannon_db_writebatch_create();
	if (write_batch == NULL) {
		printf("write options create failed!!!\n");
		shannon_db_readoptions_destroy(read_options);
		shannon_db_writeoptions_destroy(write_options);
		shannon_db_options_destroy(options);
		return NULL;
	}
	shannon_db_options_set_create_if_missing(options, 1);
	while (limit == 0 || run_time < limit) {
		db = shannon_db_open(options, "/dev/kvdev0", db_name, &err);
		CheckNoError(err);
		if (db == NULL) {
			printf("%s open failed!!!\n", db_name);
			shannon_db_options_destroy(options);
			return NULL;
		}
		if (get_bit(write_mode, WRITE_BATCH_BIT))
			run_write_batch(db, write_options, read_options, write_batch, db_name);
		else if (get_bit(write_mode, PUT_BIT))
			run_put(db, write_options, read_options, db_name);

		shannon_destroy_db(db, &err);
		CheckNoError(err);
		shannon_db_close(db);
		run_time ++;
	}
	shannon_db_writebatch_destroy(write_batch);
	shannon_db_readoptions_destroy(read_options);
	shannon_db_writeoptions_destroy(write_options);
	shannon_db_options_destroy(options);
}

void print_help_info(char *info)
{
	if (info == NULL) {
		printf("usage: deletedatabase_test <command> [<args>]\n");
		printf("<command>\n");
		printf("\twb: write to the device using write_batch.\n");
		printf("\tput: write to the device using put.\n");
		printf("[<args>]\n");
		printf("\t-n: The number of data to be written before deleting the database.\n");
		printf("\t-m: Number of threads.\n");
		printf("\t-c: The number of units of reading and writing.\n");printf("\t-l: Limit run times.\n\n");
	} else if (strcmp(info, "-n") == 0) {
		printf("Incorrect parameters!!!\n");
		printf("usage:\n");
		printf("\tfor example: -n 10000\n");
	} else if (strcmp(info, "-m") == 0) {
		printf("Incorrect parameters!!!\n");
		printf("usage:\n");
		printf("\tfor example: -m 10\n");
	} else if (strcmp(info, "-c") == 0) {
		printf("Incorrect parameters!!!\n");
		printf("usage:\n");
		printf("\tfor example: -c 100\n");
	} else if (strcmp(info, "-l") == 0) {
		printf("Incorrect parameters!!!\n");
		printf("usage:\n");
		printf("\tfor example: -l 10\n");
	}
}

void parse_parameter(int argc, char **argv)
{
	int i, j;

	zero_bit(write_mode);
	if (argc < 2) {
		print_help_info(NULL);
		exit(EXIT_FAILURE);
	}
	if (strcmp(argv[1], "wb") == 0) {
		set_bit(write_mode, WRITE_BATCH_BIT);
	} else if (strcmp(argv[1], "put") == 0) {
		set_bit(write_mode, PUT_BIT);
	}

	if (write_mode == 0) {
		print_help_info(NULL);
		exit(EXIT_FAILURE);
	}
	for (i = 2; i < argc; i++) {
		if (strcmp(argv[i], "help") == 0 || strcmp(argv[i], "--help") == 0) {
			print_help_info(NULL);
			exit(EXIT_FAILURE);
		} else if (strcmp(argv[i], "-n") == 0) {
			if (i + 1 < argc && is_digit(argv[i+1])) {
				numbers_of_write = atoi(argv[i+1]);
				int n = numbers_of_write;
				CheckCondition(n >= 10000);
				i++;
			} else {
				print_help_info("-n");
				exit(EXIT_FAILURE);
			}
		} else if (strcmp(argv[i], "-m") == 0) {
			if (i + 1 < argc && is_digit(argv[i+1])) {
				numbers_of_threads = atoi(argv[i+1]);
				int m = numbers_of_threads;
				CheckCondition(m > 0 && m < 256);
			} else {
				print_help_info("-m");
				exit(EXIT_FAILURE);
			}
		} else if (strcmp(argv[i], "-c") == 0) {
			if (i + 1 < argc && is_digit(argv[i+1])) {
				count = atoi(argv[i+1]);
				int c = count;
				CheckCondition(c >= 200 && c <= 100000);
			} else {
				print_help_info("-c");
				exit(EXIT_FAILURE);
			}
		} else if (strcmp(argv[i], "-l") == 0) {
			if (i + 1 < argc && is_digit(argv[i+1])) {
				limit = atoi(argv[i+1]);
				int l = limit;
				CheckCondition(l >= 0);
			} else {
				print_help_info("-l");
				exit(EXIT_FAILURE);
			}
		}
	}
}

int main(int argc, char **argv)
{
	int ret, i;
	int db_ids[256];
	void *status;
	pthread_t pids[256];

	parse_parameter(argc, argv);
	StartPhase("delete database");
	for (i = 0; i < numbers_of_threads; i++) {
		db_ids[i] = i;
		pthread_create(&pids[i], NULL, pthread_delete_database, &db_ids[i]);
	}
	for (i = 0; i < numbers_of_threads; i++) {
		ret = pthread_join(pids[i], &status);
		if (ret) {
			printf("thread_join failed: %s\n", strerror(ret));
			return EXIT_FAILURE;
		}
	}
	return 0;
}
