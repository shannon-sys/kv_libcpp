#include <iostream>
#include <swift/shannon_db.h>
#include <string>
#include <sstream>
#include <swift/log_iter.h>
#include <stdio.h>
#include <unistd.h>

using namespace std;
using namespace shannon;
using shannon::Status;
using shannon::Slice;
using shannon::LogIterator;

#define DEBUG(format, arg...) \
  printf("%s(), line=%d: " format, __FUNCTION__, __LINE__, ##arg)
#define KEY_LEN 24
#define VALUE_LEN 4096
#define KEY_PAD_CHAR 'K'
#define VALUE_PAD_CHAR 'V'
#define LOG_ITER_MAX 32

void test_iter_max_count(string &device)
{
  int count = 0, j = 0;
  uint64_t timestamp;
  LogIterator *log_iters[LOG_ITER_MAX];
  LogIterator *tmp_iter = NULL;
  Status s;

  s = shannon::GetSequenceNumber(device, &timestamp);
  assert(s.ok());
  DEBUG("get dev timestamp=%lu\n", timestamp);

  for (count = 0; count < LOG_ITER_MAX; ++count) {
    log_iters[count] = NULL;
    s = NewLogIterator(device, timestamp, log_iters + count);
    if (!s.ok()) {
      for (j = 0; j < count; ++j)
        delete log_iters[j];
        DEBUG("error: create log iterator, count=%d.\n", count);
        exit(-1);
      }
    }

  // new log_iter=33, should fail
  s = NewLogIterator(device, timestamp, &tmp_iter);
  for (j = 0; j < LOG_ITER_MAX; ++j)
    delete log_iters[j];
  assert(!s.ok());
}

void write_kvs(string &device, string &dbname, uint32_t start, uint32_t offset, int32_t *db_idx)
{
  DB *db;
  Options option;
  option.create_if_missing = true;
  Status s;
  int i, max;
  char key[KEY_LEN];
  char value[VALUE_LEN];
  int print_time = 128 * 1024;

  s = DB::Open(option, dbname, device, &db);
  assert(s.ok());
  *db_idx = db->GetIndex();

  DEBUG("start write kv start=%d, offset=%d to db=%s.\n", start, offset, dbname.c_str());
  max = start + offset;
  for (i = start; i < max; ++i) {
    memset(key, KEY_PAD_CHAR, KEY_LEN);
    memset(value, VALUE_PAD_CHAR, VALUE_LEN);
    key[KEY_LEN-1] = '\0';
    value[VALUE_LEN-1] = '\0';

    snprintf(key, sizeof(key), "key+%d-", i);
    key[strlen(key)] = KEY_PAD_CHAR;
    snprintf(value, sizeof(value), "value+%d-", i);
    value[strlen(value)] = VALUE_PAD_CHAR;

    if (i % print_time == 1)
      DEBUG("put keynum: %d*%d+1, key=%s.\n", i / print_time, print_time, key);

    s = db->Put(WriteOptions(), key, value);
    assert(s.ok());
  }
  delete db;
}

void delete_kvs(string &device, string &dbname, uint32_t start, uint32_t offset, int32_t *db_idx)
{
  DB *db;
  Options option;
  option.create_if_missing = true;
  Status s;
  int i, max;
  char key[KEY_LEN];
  int print_time = 256 * 1024;

  s = DB::Open(option, dbname, device, &db);
  assert(s.ok());
  *db_idx = db->GetIndex();

  DEBUG("start delete kv start=%d, offset=%d to db=%s.\n", start, offset, dbname.c_str());
  max = start + offset;
  for (i = start; i < max; ++i) {
    memset(key, KEY_PAD_CHAR, KEY_LEN);
    key[KEY_LEN-1] = '\0';
    snprintf(key, sizeof(key), "key+%d-", i);
    key[strlen(key)] = KEY_PAD_CHAR;
    if (i % print_time == 1)
      DEBUG("delete keynum: %d*%d+1, key=%s.\n", i / print_time, print_time, key);
    s = db->Delete(WriteOptions(), key);
    assert(s.ok());
  }
  delete db;
}

void test_iter_expired(string &device)
{
  uint64_t timestamp;
  LogIterator *iter = NULL;
  Status s;
  int32_t db_idx;
  string dbname = "testdb";

  // get sdev->cur_timestamp and new log_iter
  s = shannon::GetSequenceNumber(device, &timestamp);
  assert(s.ok());
  DEBUG("get dev timestamp=%lu.\n", timestamp);
  s = NewLogIterator(device, timestamp, &iter);
  assert(s.ok());

  // write kv to make 4 superblock full, about 12G
  write_kvs(device, dbname, 1, 3 * 1024 * 1024, &db_idx);

  // wait a moment, iter will be destroyed by merge_thread
  sleep(1);
  iter->Next();
  assert(!iter->Valid() && iter->status().IsCorruption());
  delete iter;

  // test expired(invalid) timestamp, create log iterator should fail
  s = NewLogIterator(device, 0, &iter);
  assert(!s.ok());
}

int match_key(Slice &key, uint32_t num)
{
  char keybuf[KEY_LEN];

  if (key.size() != (KEY_LEN -1)) {
    DEBUG("mismatch key=%s.\n", key.data());
    return 1;
  }
  memset(keybuf, KEY_PAD_CHAR, KEY_LEN);
  keybuf[KEY_LEN-1] = '\0';
  snprintf(keybuf, sizeof(keybuf), "key+%d-", num);
  keybuf[strlen(keybuf)] = KEY_PAD_CHAR;
  if (strncmp(keybuf, key.data(), KEY_LEN - 1) == 0)
    return 0;
  else {
    DEBUG("mismatch key=%s.\n", key.data());
    return 1;
  }
}

int match_value(Slice &value, uint32_t num)
{
  char valuebuf[KEY_LEN];

  if (value.size() != (VALUE_LEN -1)) {
    DEBUG("mismatch value=%s.\n", value.data());
    return 1;
  }
  memset(valuebuf, VALUE_PAD_CHAR, VALUE_LEN);
  valuebuf[VALUE_LEN-1] = '\0';
  snprintf(valuebuf, sizeof(valuebuf), "value+%d-", num);
  valuebuf[strlen(valuebuf)] = VALUE_PAD_CHAR;
  if (strncmp(valuebuf, value.data(), VALUE_LEN - 1) == 0)
    return 0;
  else {
    DEBUG("mismatch value=%s.\n", value.data());
    return 1;
  }
}

// check key+i-KKK...KK, i of [start, start + offset)
void check_iter(LogIterator *iter, LogOpType type, int db_idx, uint64_t start_ts, uint32_t start, uint32_t offset)
{
  int32_t cf, i, max;
  Slice key, value;

  DEBUG("start check iter: LogOpType=%d, db=%d, timestamp=%lu, start=%d, offset=%d.\n", type, db_idx, start_ts, start, offset);
  max = start + offset;
  for (i = start; i < max; ++i) {
    ++start_ts;
    iter->Next();
    assert(iter->Valid());

    key = iter->key();
    assert(iter->status().ok());
    assert(match_key(key, i) == 0);

    assert(iter->optype() == type);
    assert(iter->db() == db_idx);
    cf = iter->cf();
    assert(iter->timestamp() == start_ts);

    value = iter->value();
    assert(iter->status().ok());
    switch (type) {
    KEY_DELETE:
      assert(value.size() == 0); break;
    KEY_ADD:
      assert(match_value(value, i) == 0); break;
    DEFAULT:
      DEBUG("unknown optype=%d.\n", iter->optype());
    }
  }
}

struct record {
  string db_name;
#define TYPE_CREATE 0
#define TYPE_REMOVE 1
  int type;
};

LogIterator *create_log_iterator(string device, uint64_t &timestamp)
{
  LogIterator *iter = NULL;
  Status s;

  s = shannon::GetSequenceNumber(device, &timestamp);
  assert(s.ok());
  DEBUG("get dev timestamp=%lu\n", timestamp);
  s = NewLogIterator(device, timestamp, &iter);
  assert(s.ok());

  // the last kv, can not find new kv;
  iter->Next();
  assert(!iter->Valid() && iter->status().IsNotFound());

  return iter;
}

void destroy_log_iterator(LogIterator *iter)
{
  delete iter;
}

/**
 * @brief Create a db only if it doesn't exist.
 *
 * @param device
 * @param db_name
 * @return bool
 */
bool create_new_db(string device, string db_name)
{
  DB *db;
  Options option;
  option.create_if_missing = false;
  Status s;

  s = DB::Open(option, db_name, device, &db);
  if (s.ok()) {
    // already exists
    delete db;
    return false;
  }

  option.create_if_missing = true;
  s = DB::Open(option, db_name, device, &db);
  delete db;
  return s.ok();
}

bool remove_exist_db(string device, string db_name)
{
  Status s;
  s = DestroyDB(device, db_name, Options());
  return s.ok();
}

void test_create_remove_db(string device)
{
  vector<record> history;  // only record suss
  uint64_t timestamp;
  LogIterator *iter = NULL;
  Status s;
  const int DB_COUNT = 20;
  int succ;

  srand (time(NULL));

  iter = create_log_iterator(device, timestamp);

  for (int i = 0; i < 100; i++) {
    struct record rec;
    int idx = rand() % DB_COUNT;
    char buf[512];

    snprintf(buf, sizeof(buf), "log%d", idx);
    string db_name = buf;
    int type = rand() % 2;

    if (type == TYPE_CREATE) {
      succ = create_new_db(device, db_name);
    } else {
      succ = remove_exist_db(device, db_name);
    }

    if (succ) {
      rec.db_name = db_name;
      rec.type = type;
      history.push_back(rec);
    }
  }

  for (int i = 0; i < history.size(); i++) {
    struct record rec = history[i];
    ++timestamp;
    iter->Next();
    assert(iter->Valid());
    assert(iter->timestamp() == timestamp);

    if (rec.type == TYPE_CREATE)
      assert(iter->optype() == DB_CREATE);
    else
      assert(iter->optype() == DB_DELETE);

    {
      auto value = iter->value();
      assert(iter->status().ok());
      assert(rec.db_name == value.ToString());
    }
  }

  for (int i = 0; i < DB_COUNT; i++) {
    char buf[512];
    snprintf(buf, sizeof(buf), "log%d", i);
    string db_name = buf;
    remove_exist_db(device, db_name);
  }

  destroy_log_iterator(iter);
}

int test_iter_data(string &device)
{
  uint64_t timestamp;
  LogIterator *iter = NULL;
  Status s;
  int32_t db_idx, i;
  uint32_t offset = 128 * 1024, start = 1;
  string dbname = "testdb";
  string dbname2 = "testdb2";

  // get sdev->cur_timestamp and new log_iter
  s = shannon::GetSequenceNumber(device, &timestamp);
  assert(s.ok());
  DEBUG("get dev timestamp=%lu\n", timestamp);
  s = NewLogIterator(device, timestamp, &iter);
  assert(s.ok());

  // the last kv, can not find new kv;
  iter->Next();
  assert(!iter->Valid() && iter->status().IsNotFound());

  // check write
  write_kvs(device, dbname, 1, 100, &db_idx);
  check_iter(iter, KEY_ADD, db_idx, timestamp, 1, 100);
  timestamp += 100;

  // check delete
  delete_kvs(device, dbname, 2, 50, &db_idx);
  check_iter(iter, KEY_DELETE, db_idx, timestamp, 2, 50);
  timestamp += 50;

  // check switch super block, write 1G every time
  for (i = 0; i < 4; ++i) {
    write_kvs(device, dbname2, start, offset, &db_idx);
    check_iter(iter, KEY_ADD, db_idx, timestamp, start, offset);
    timestamp += offset;
    start += offset;
  }

  iter->Next();
  assert(!iter->Valid() && iter->status().IsNotFound());
  delete iter;
}

int main(int argc,char * argv[])
{
  string device = "/dev/kvdev0";

  printf("===== start log iter test\n");
  {
    printf("===== start create_remove_db test\n");
    test_create_remove_db(device);
    printf("===== start test_iter_data\n");
    test_iter_data(device);
    printf("===== start test expired log iter\n");
    test_iter_expired(device);
    printf("===== start test_iter_max_count\n");
    test_iter_max_count(device);
  }
  printf("===== PASS!\n");
}
