#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "swift/shannon_db.h"

#define handle_error_en(en, msg) \
  do {                           \
    errno = en;                  \
    perror(msg);                 \
    exit(EXIT_FAILURE);          \
  } while (0)
using namespace std;
using namespace shannon;

const std::string DB_FILE_PATH = "db";
const std::string dbpath = "/dev/kvdev0";
std::string kDBPath;
atomic_int call_case;
atomic_int cmd_cases;
atomic_int des_cases;
struct kv {
  string key;
  string val;
};
atomic<bool> is_close;
atomic_int get_cases;
atomic_int put_cases;
atomic_int ttt;
atomic_int time_sp;
mutex lk_kv;
vector<kv> que_kv;
unordered_map<string, int> kvmp;
int tot = 0;
class GetData {
 public:
  GetData(shannon::DB *db, string str) : db_(db) { str_ = str; }
  string str_;
  shannon::DB *db_;
};

class Callback : public shannon::CallBackPtr {
 public:
  explicit Callback(string a) : CallBackPtr() {
    cmd_cases++;
    key = a;
  }
  ~Callback() { des_cases++; }
  void call_ptr(const shannon::Status &s) override {
    put_cases++;
    if (!s.ok()) {
      cout << "fail key:" << key << endl << s.ToString() << endl;
    }
    delete this;
  }
  std::string key;
};

class Getback : public shannon::CallBackPtr {
 public:
  explicit Getback(string a) : CallBackPtr() {
    cmd_cases++;
    key = a;
  }
  ~Getback() { des_cases++; }
  void call_ptr(const shannon::Status &s) override {
    get_cases++;
    std::unique_lock<std::mutex> lck(lock_);
    wait_fg = true;
    cond.notify_all();
  }
  void wait() {
    std::unique_lock<std::mutex> lck(lock_);
    if (!wait_fg) {
      cond.wait(lck);
      wait_fg = false;
    }
  }
  mutex lock_;
  bool wait_fg = false;
  std::condition_variable cond;
  std::string key;
};

void KVopen(const Options &options, DB **db, int mark) {
  Status s;
  s = DB::Open(options, kDBPath, dbpath, db);
  assert(s.ok());
}

void check(string key, string value, bool fg = false) {
  lk_kv.lock();
  unordered_map<string, int>::const_iterator got = kvmp.find(key);
  if (got == kvmp.end()) {
    cout << "map error " << endl;
    cout << "key:" << key;
    cout << "  val:" << value << endl;
    lk_kv.unlock();
    return;
  }
  int right = got->second;
  if (value != que_kv[right].val) {
    cout << endl
         << "val fauilt--------------- " << endl
         << "key : " << key << endl
         << "val right:" << que_kv[right].val << endl
         << "val get  :" << value << endl
         << que_kv[right].val.length() << endl
         << "-------------------------" << endl;
  } else if (fg) {
    cout << "success-- ";
    cout << "key:" << key;
    cout << "  val:" << value << endl;
  }
  lk_kv.unlock();
}

void KVget_ancy(GetData data) {
  Status s;
  shannon::DB *db = data.db_;
  string key = data.str_;
  Getback cb1(key);
  char b[500];
  int32_t vallen = -1;
  s = db->GetAsync(shannon::ReadOptions(), key, b, 500, &vallen, &cb1);
  if (!s.ok()) {
    return;
  }
  while (vallen == -1) {
    cb1.wait();
  }
  string value("");
  value.assign(b, vallen);
  check(key, value);
}

void KVput_ancy(DB *db, const string &key, const string &value) {
  Status s;
  Callback *cb = new Callback(string(key));
  s = db->PutAsync(shannon::WriteOptions(), key, value, cb);
  if (!s.ok()) {
    abort();
  }
}

void init_kv() {
  tot = 0;
  que_kv.clear();
  for (int i = 0; i < 100000; i++) {
    stringstream fs_key;
    stringstream fs_val;
    fs_key << "key";
    fs_val << "val";
    fs_key << i;
    fs_val << i;
    que_kv.push_back({fs_key.str(), fs_val.str()});
    if (que_kv.back().val.length() == 0)
      cout << "length:" << que_kv.back().val.length() << endl;
    kvmp.insert({fs_key.str(), que_kv.size() - 1});
    tot++;
  }
}

void poll_get_stop(DB *db) {
  Status ss;
  int cases = 0;
  uint64_t timeout = 100000;
  while (!is_close) {
    int32_t num = 0;
    ss = db->PollCompletion(&num, timeout);
    if (ss.ok()) {
      cases += num;
    }
  }
  cout << "complete cases " << cases << " cmd cases : " << cmd_cases
       << " del case " << des_cases << " pg cases : " << put_cases + get_cases
       << endl
       << "ALL OK!" << endl;
  assert(cases == put_cases + get_cases);
  assert(cmd_cases == des_cases);
}

void Test_write(DB *db, const string &val) {
  int len = (rand() % 20) + 3;
  stringstream fs_key;
  fs_key << "key";
  fs_key << (rand() * 1333309) % 233333;
  fs_key << len;
  string key = fs_key.str();
  KVput_ancy(db, key, val);
}

int keyput(DB *db, string key, string value, bool fg = true) {
  Status s;
  Getback cb(key);
  s = db->PutAsync(shannon::WriteOptions(), key, value, &cb);
  if (!s.ok()) {
    cmd_cases--;
    des_cases--;
    return -1;
  }
  if (fg) {
    que_kv.push_back({key, value});
    if (que_kv.back().val.length() == 0) {
      cout << "length:" << que_kv.back().val.length() << endl;
    }
    kvmp.insert({key, que_kv.size() - 1});
  }
  cb.wait();
  return 1;
}

int keyget(DB *db, string key, string value = "") {
  Status s;
  Getback cb1(key);
  char b[500];
  int32_t vallen = -1;
  s = db->GetAsync(shannon::ReadOptions(), key, b, 500, &vallen, &cb1);
  if (!s.ok()) {
    cmd_cases--;
    des_cases--;
    return -1;
  }
  while (vallen == -1) {
    cb1.wait();
  }
  string val;
  val.assign(b, vallen);
  if (value != "") {
    return value == val ? 1 : -1;
  }
  cout << key << " : " << val << endl;
  return 1;
}

int keydel(DB *db, string key) {
  Status s;
  Getback cb(key);
  shannon::WriteOptions writeoptions;
  s = db->DeleteAsync(writeoptions, key, &cb);
  if (!s.ok()) {
    cmd_cases--;
    des_cases--;
    return -1;
  }
  cb.wait();
  return 1;
}

void get_kv(DB *db) {
  for (auto kv : que_kv) {
    if (kv.val.length() == 0) cout << "length: " << kv.val.length() << endl;
    call_case++;
    KVget_ancy(GetData(db, kv.key));
  }
}

int Test_get_mul(DB *db) {
  Status s;
  for (auto kv : que_kv) {
    if (kv.val.length() == 0) {
      cout << "length: " << kv.val.length() << endl;
    }
    call_case++;
    keyput(db, kv.key, kv.val, false);
  }
  usleep(5000000);
  thread *th2 = new thread(get_kv, db);
  thread *th3 = new thread(get_kv, db);
  thread *th4 = new thread(get_kv, db);
  thread *th5 = new thread(get_kv, db);
  thread *th6 = new thread(get_kv, db);
  th2->join();
  th3->join();
  th4->join();
  th5->join();
  th6->join();
  cout << "get " << tot << " ok!" << endl;
  return 1;
}

int64_t timeget() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}

void Test_mul(DB *db) {
  string val(3900, 'c');
  int casess = 100000000;
  while (casess--) {
    Test_write(db, val);
  }
}

void Test_behave(DB *db) {
  string str1("key1");
  string val1("val1");
  string val2("val2");
  int res = 0;
  keydel(db, str1);
  res = keyput(db, str1, val1, false);
  assert(res > 0);
  res = keyget(db, str1, val1);
  assert(res > 0);
  res = keyput(db, str1, val2, false);
  assert(res > 0);
  res = keyget(db, str1, val2);
  assert(res > 0);
  res = keydel(db, str1);
  assert(res > 0);
  res = keydel(db, str1);
  assert(res < 0);
  res = keyget(db, str1, val1);
  assert(res < 0);
  res = keyput(db, str1, val2, false);
  assert(res > 0);
  res = keyget(db, str1, val2);
  assert(res > 0);
  res = keydel(db, str1);
  assert(res > 0);
}

void check_iterator(DB *db, bool fg);

void KV_Outall() {
  Options options;
  options.create_if_missing = true;
  DB *db;
  KVopen(options, &db, 1);
  lk_kv.lock();
  init_kv();
  lk_kv.unlock();
  thread *th1 = new thread(poll_get_stop, db);
  Test_behave(db);
  cout << "Test_behave before finished!" << endl;
  int res = Test_get_mul(db);
  if (res < 0) {
    cout << "Test_get_mul fail" << endl;
    abort();
  }
  check_iterator(db, false);
  cout << "check_iterator finished!" << endl;
  Test_behave(db);
  cout << "Test_behave after finished!" << endl;
  is_close = true;
  th1->join();
  usleep(5000000);
  delete db;
}

void check_iterator(DB *db, bool fg) {
  shannon::Iterator *iter = db->NewIterator(shannon::ReadOptions());
  for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
    check(iter->key().ToString(), iter->value().ToString(), fg);
  }
  delete iter;
}

void DestoryDB() {
  shannon::Options options;
  shannon::Status s = shannon::DestroyDB(dbpath, kDBPath, options);
  if (!s.ok()) {
    cout << "DestroyDB error:" << s.ToString() << endl;
    abort();
  }
}

int main(int argc, char *argv[]) {
  is_close = false;
  kDBPath = DB_FILE_PATH + "_test_aio";
  ttt = 0;
  time_sp = 0;
  get_cases = 0;
  put_cases = 0;
  call_case = 0;
  cmd_cases = 0;
  des_cases = 0;
  if (argc == 2 && argv[1][0] == '1') {
    cout << "start------- " << endl;
    que_kv.resize(100000);
    KV_Outall();
    cout << "ok!" << endl;
    que_kv.clear();
    DestoryDB();
  } else if (argc == 2 && argv[1][0] == '0') {
    Options options;
    options.create_if_missing = true;
    DB *db;
    KVopen(options, &db, 1);
    thread *th1 = new thread(poll_get_stop, db);
    char op[20];
    char key[200];
    char val[200];
    while (!is_close && scanf("%s", op) != EOF) {
      string key_str;
      string val_str;
      switch (op[0]) {
        case 'p':
          scanf("%s%s", key, val);
          val_str.assign(val, strlen(val));
          key_str.assign(key, strlen(key));
          keyput(db, key_str, val_str);
          break;

        case 'g':
          scanf("%s", key);
          key_str.assign(key, strlen(key));
          keyget(db, key_str);
          break;

        case 'd':
          scanf("%s", key);
          key_str.assign(key, strlen(key));
          keydel(db, key_str);
          break;
        case 'i':
          check_iterator(db, true);
          break;
        case 'e':
          is_close = true;
          usleep(500000);
          break;
        default:
          cout << "no such op" << endl;
          break;
      }
    }
    th1->join();
    que_kv.clear();
    DestoryDB();
  } else {
    cout << endl
         << "run test use such options : " << endl
         << endl
         << "0:test by command input" << endl
         << "  options              command" << endl
         << "    1.put:            \"put key value\"" << endl
         << "    2.get:            \"get key\"" << endl
         << "    3.delete:         \"del key\"" << endl
         << "    4.exit:           \"exit\"" << endl
         << "    5.iter all data:  \"iter\"" << endl
         << "1: run auto general test" << endl
         << endl;
  }
}
