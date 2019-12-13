#include <iostream>
#include <swift/shannon_db.h>
using namespace shannon;
using namespace std;
using shannon::Status;
using shannon::Options;
using shannon::Slice;
using shannon::WriteOptions;
using shannon::WriteBatch;
using shannon::WriteBatchNonatomic;
using shannon::WriteBatchWithIndex;
using shannon::ReadOptions;
using shannon::Snapshot;
using shannon::Iterator;
using shannon::ColumnFamilyHandle;
using shannon::ColumnFamilyDescriptor;
using shannon::ColumnFamilyDescriptor;
const char *phase = "";
#define CheckCondition(cond)                                                   \
  if (!(cond)) {                                                               \
    fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, phase, #cond);      \
    abort();                                                                   \
  }

static void CheckEqual(const char *expected, const char *v, size_t n) {
  if (expected == NULL && v == NULL) {
    // ok
  } else if (expected != NULL && v != NULL && n == strlen(expected) &&
             memcmp(expected, v, n) == 0) {
    // ok
    return;
  } else {
    fprintf(stderr, "%s: expected '%s', got '%s'\n", phase,
            (expected ? expected : "(null)"), (v ? v : "(null"));
    abort();
  }
}

int main() {
  shannon::DB *db;
  shannon::Options options;
  shannon::DBOptions db_options;
  shannon::Status status;
  std::string dbname = "test_readbatch.db";
  std::string device = "/dev/kvdev0";
  std::vector<ColumnFamilyHandle *> handles;
  std::vector<ColumnFamilyDescriptor> cf_des;
  ColumnFamilyOptions cf_option;
  ColumnFamilyHandle *cf1;
  ColumnFamilyHandle *cf2;
  options.create_if_missing = true;
  db = NULL;
  status = shannon::DB::Open(options, dbname, device, &db);
  if (status.ok()) {
    status = db->CreateColumnFamily(cf_option, "sst_cf1", &cf1);
    assert(status.ok());
    delete cf1;
    delete db;
  }
  cf_des.push_back(ColumnFamilyDescriptor("default", cf_option));
  cf_des.push_back(ColumnFamilyDescriptor("sst_cf1", cf_option));
  status = shannon::DB::Open(db_options, dbname, device, cf_des, &handles, &db);
  assert(status.ok());
  cf1 = handles[0];
  cf2 = handles[1];
  char key[20], value[20];
  shannon::WriteBatch batch;
  for (int i = 0; i < 900; i ++) {
    sprintf(key, "key:%d", i);
    sprintf(value, "value:%d", i);
    if (i % 2 == 0)
      batch.Put(cf1, key, value);
    else
      batch.Put(cf2, key, value);
  }
  batch.Put(cf1, "zero_value", "");
  Status s = db->Write(shannon::WriteOptions(), &batch);
  assert(s.ok());
  batch.Clear();

  shannon::ReadBatch read_batch;
  std::vector<std::pair<shannon::Status, std::string> > values;
  for (int i = 0; i < 900; i ++) {
    sprintf(key, "key:%d", i);
    if (i % 2 == 0)
      read_batch.Get(cf1, key);
    else
      read_batch.Get(cf2, key);
  }
  read_batch.Get(cf2, "not_exists_key");
  read_batch.Get(cf1, "zero_value");

  status = db->Read(shannon::ReadOptions(), &read_batch, &values);
  assert(status.ok());
  for (int i = 0; i < values.size() - 2; i ++) {
    assert(values[i].first.ok());
    sprintf(value, "value:%d", i);
    CheckCondition(values[i].second.size() == strlen(value));
    CheckEqual(values[i].second.data(), value, values[i].second.size());
  }
  assert(values[values.size() - 2].first.IsNotFound());
  assert(values[values.size() - 1].first.ok());
  CheckCondition(values[values.size() - 1].second.size() == 0);
  delete cf2;
  delete cf1;
  delete db;
  DestroyDB(device, dbname, shannon::Options());
  std::cout<<"PASS"<<std::endl;
  return 0;
}
