#include <iostream>
#include <swift/shannon_db.h>
#include <string>
#include <sstream>
#include <swift/log_iter.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

using namespace std;
using namespace shannon;
using shannon::Status;
using shannon::Slice;
using shannon::LogIterator;

#define DEBUG(format, arg...) \
  printf("%s(), line=%d: " format, __FUNCTION__, __LINE__, ##arg)
#define KEY_LEN 32
#define VALUE_LEN 256
#define KEY_PAD_CHAR 'K'
#define VALUE_PAD_CHAR 'V'
#define LOG_ITER_MAX 32

#define MAX_THREAD_WRITE 16
#define MIN_THREAD_WRITE 1
unsigned char flag[MAX_THREAD_WRITE] = { 0 };
pthread_mutex_t flag_lock;

int find_first_index_and_set()
{
	int i;
	pthread_mutex_lock(&flag_lock);
	for (i = 0; i < MAX_THREAD_WRITE; ++i) {
		if (flag[i] == 0) {
			flag[i] = 1;
			break;
		}
	}
	pthread_mutex_unlock(&flag_lock);
	return i;
}

void write_kvs(string &device, pthread_t tid, int nums)
{
  DB *db;
  Options option;
  option.create_if_missing = true;
  Status s;
  unsigned int i;
  char key[KEY_LEN];
  char value[VALUE_LEN];
  int print_time = 128 * 1024;
  int tid_index;
  string dbname;
  char name[100];
  int run_times = 1;

  tid_index = find_first_index_and_set();
  memset(name, 0, 100);
  sprintf(name, "testdb%d", tid_index);
  dbname.assign(name, strlen(name));

  DEBUG("======= tid=%lu, tid_index=%d start write.\n", (unsigned long)tid, tid_index);

restart:
  s = DB::Open(option, dbname, device, &db);
  i = 0;
  assert(s.ok());
  while (true) {
    i++;
    memset(key, KEY_PAD_CHAR, KEY_LEN);
    memset(value, VALUE_PAD_CHAR, VALUE_LEN);
    key[KEY_LEN-1] = '\0';
    value[VALUE_LEN-1] = '\0';

    snprintf(key, sizeof(key), "key+%d+%u-", tid_index, i);
    key[strlen(key)] = KEY_PAD_CHAR;
    snprintf(value, sizeof(value), "value+%d+%u-", tid_index, i);
    value[strlen(value)] = VALUE_PAD_CHAR;

    if (i % print_time == 0) {
      //DEBUG("tid_index=%d, keynum: %d*%d.\n", tid_index, i / print_time, print_time);
      usleep(4000 / nums);
    }
    if (i >= (800 * 128 * 1024 / nums)) {
      delete db;
      s = DestroyDB(device, dbname, Options());
      DEBUG("===== tid_index=%d, run_times=%d.\n", tid_index, run_times);
      run_times++;
      goto restart;
    }

    s = db->Put(WriteOptions(), key, value);
    assert(s.ok());
  }
  delete db;
}

struct arg_node {
  int thread_write_nums;
  uint64_t timestamp;
};

void *thread_write_fn(void *arg)
{
  struct arg_node *option = (struct arg_node *)arg;
  string device = "/dev/kvdev0";
  pthread_t tid = pthread_self();

  write_kvs(device, tid, option->thread_write_nums);
}

void iter_data(string &device, uint64_t old_timestamp, uint64_t new_timestamp)
{
  uint64_t start, end, timestamp;
  Slice key;
  LogIterator *iter = NULL;
  Status s;

  start = (old_timestamp > 75) ? old_timestamp - 75 : 0;
  timestamp = start;
  end = new_timestamp + 75;
  s = NewLogIterator(device, start, &iter);
  assert(s.ok());

  do {
    iter->Next();
    if (!iter->Valid() && iter->status().IsNotFound()) {
      DEBUG("iter next fail.\n");
      goto out;
    }
    assert(iter->Valid());

    key = iter->key();
    assert(iter->status().ok());
    timestamp = iter->timestamp();
    printf("timstamp=%lu, key=%s.\n", timestamp, key.data());
  } while (timestamp < end);

out:
  delete iter;
}

void *thread_iter_fn(void *arg)
{
  struct arg_node *option = (struct arg_node *)arg;
  uint64_t old_timestamp = 0, new_timestamp = 0, continue_not_found = 0;
  LogIterator *iter = NULL;
  Status s;
  int32_t db, cf, i;
  Slice key, value;
  string device = "/dev/kvdev0";
  string dbname = "testdb";
  pthread_t tid = pthread_self();
  useconds_t us = 4000 / option->thread_write_nums;

  DEBUG("======= tid=%lu, start thread_iter_fn.\n", (unsigned long)tid);
  // create log iter
create_log_iterator_retry:
  DEBUG("try to create log_iter with timestamp=%lu, us=%u.\n", option->timestamp, us);
  continue_not_found = 0;
  s = NewLogIterator(device, option->timestamp, &iter);
  if (!s.ok()) {
    sleep(1);
    option->timestamp++;
    goto create_log_iterator_retry;
  }

  // start iter epilog
  while (true) {
    iter->Next();
    if (iter->status().IsNotFound()) {
      // now is the last kv
      continue_not_found++;
      if (continue_not_found % 10 == 0) {
        //DEBUG("continue_not_found=%lu.\n", continue_not_found);
        usleep(us);
      }
      continue;
    } else if (iter->status().ok()) {
      // find new kv
      continue_not_found = 0;
      key = iter->key();
      assert(iter->status().ok());
      db = iter->db();
      cf = iter->cf();
      new_timestamp = iter->timestamp();
      if (old_timestamp == 0 ||
        (new_timestamp - old_timestamp == 1)) {
        old_timestamp = new_timestamp;
        continue;
      }
      // err find timestamp hop
      DEBUG("----------------------------------\n");
      DEBUG("old_timestamp=%lu, new_timestamp=%lu.\n", old_timestamp, new_timestamp);
      DEBUG("new_key=%s.\n", key.data());
      DEBUG("----------------------------------\n");
      delete iter;
      iter_data(device, old_timestamp, new_timestamp);
      DEBUG("----------------------------------\n");
      exit(-1);
    } else {
      // log iter is too old
      delete iter;
      s = shannon::GetSequenceNumber(device, &option->timestamp);
      assert(s.ok());
      us = us / 2;
      us = (us < 10) ? 10 : us;
      goto create_log_iterator_retry;
    }
  }
}

void usage() {
  printf("usage: ./log_iter_thread_test [write_thread_nums]");
  printf("       write_thread_nums: default 4; range=[1, 7]");
}

int main(int argc, char * argv[])
{
  string device = "/dev/kvdev0";
  struct arg_node option;
  uint64_t timestamp;
  Status s;
  int i, err;
  pthread_t ntid;
  int nums = 4;
  pthread_t tids[MAX_THREAD_WRITE];

  if (argc > 2) {
    usage();
    return -1;
  }
  if (argc == 2) {
    nums = atoi(argv[1]);
    if (nums > MAX_THREAD_WRITE)
      nums = MAX_THREAD_WRITE;
    if (nums < MIN_THREAD_WRITE)
      nums = MIN_THREAD_WRITE;
  } else {
    nums = 4;
  }

  pthread_mutex_init(&flag_lock, NULL);

  s = shannon::GetSequenceNumber(device, &timestamp);
  assert(s.ok());
  option.timestamp = timestamp;
  option.thread_write_nums = nums;

  printf("===== start log iter pthread test.\n");
  err = pthread_create(&ntid, NULL, thread_iter_fn, (void *)&option);
  assert(err == 0);
  for (i = 0; i < nums; ++i) {
    err = pthread_create(tids + i, NULL, thread_write_fn, (void *)&option);
    assert(err == 0);
  }

  pthread_join(ntid, NULL);
  for (i = 0; i < nums; ++i) {
    pthread_join(tids[i], NULL);
  }

  pthread_mutex_destroy(&flag_lock);
  printf("===== main thread end.\n");
}
