#include <iostream>
#include <sstream>
#include <string>
#include <swift/shannon_db.h>

using namespace std;
using namespace shannon;
using shannon::Status;
using shannon::Options;
using shannon::Slice;
using shannon::WriteOptions;
using shannon::WriteBatch;
using shannon::WriteBatchNonatomic;
using shannon::ReadOptions;
using shannon::Snapshot;
using shannon::Iterator;
using shannon::ColumnFamilyHandle;
using shannon::ColumnFamilyDescriptor;
using shannon::ColumnFamilyDescriptor;

std::string toString1(int t) {
  stringstream oss;
  oss << t;
  return oss.str();
}

void usage() {
  printf("usage1: ./build_sst_test /path/to/sstfile\n");
  printf("        write kv to cf_name=default\n");
  printf("usage2: ./build_sst_test /path/to/sstfile decode_cfname\n");
  printf("        decode cf_name from sstfile and write kv to it\n");
}

Status s;
void drop_handle(DB *db, vector<ColumnFamilyHandle *> *handles) {
  Status st;
  for (ColumnFamilyHandle *hand : *handles) {
    if (hand->GetName() != "default") {
      s = db->DropColumnFamily(hand);
      if (!s.ok())
        st = s;
    }
  }
  // assert(s.ok());
}

int main(int argc, char *argv[]) {
  shannon::DB *db;
  shannon::Options options;
  shannon::DBOptions db_options;
  shannon::Status status;
  std::string dbname = "test_sst.db";
  std::string device = "/dev/kvdev0";
  int verify = 1;
  int i = 0;
  char keybuf[100];
  char valbuf[100];
  int times = 3;
  int decode_cfname = 0;
  std::vector<ColumnFamilyHandle *> handles;
  std::vector<ColumnFamilyDescriptor> cf_des;
  std::vector<Iterator *> iters;
  ColumnFamilyOptions cf_option;
  char *filename;
  if (argc != 2 && argc != 3) {
    usage();
    return -1;
  }
  filename = argv[1];
  if (argc == 3) {
    if (strcmp(argv[2], "decode_cfname") == 0)
      decode_cfname = 1;
    else {
      usage();
      return -1;
    }
  }

  options.create_if_missing = true;
  status = shannon::DB::Open(options, dbname, device, &db);
  status = DestroyDB(device, dbname, options);
  assert(status.ok());

  cf_des.push_back(ColumnFamilyDescriptor("default", cf_option));
  cf_des.push_back(ColumnFamilyDescriptor("sst_cf1", cf_option));
  status = shannon::DB::Open(options, dbname, device, &db);
  if (status.ok()) {
    ColumnFamilyHandle *cf1;
    status = db->CreateColumnFamily(cf_option, "sst_cf1", &cf1);
    assert(status.ok());
  }
  status = shannon::DB::Open(db_options, dbname, device, cf_des, &handles, &db);
  assert(status.ok());
  for (i = 0; i < times; i++) {
    snprintf(keybuf, sizeof(keybuf), "key+%d-", i);
    snprintf(valbuf, sizeof(valbuf), "value+%d-", i);
    status = db->Put(shannon::WriteOptions(), handles[0], keybuf, valbuf);
  }
  for (i = 1 * times; i < 2 * times; i++) {
    snprintf(keybuf, sizeof(keybuf), "key+%d-", i);
    snprintf(valbuf, sizeof(valbuf), "value+%d-", i);
    status = db->Put(shannon::WriteOptions(), handles[1], keybuf, valbuf);
  }
  status = db->NewIterators(shannon::ReadOptions(), handles, &iters);
  assert(status.ok());
  status = db->BuildTable(filename, handles, iters);
  assert(status.ok());

  drop_handle(db, &handles);
  for (auto handle : handles) {
    delete handle;
  }
  cout << "ok!" << endl;
  delete db;
}
