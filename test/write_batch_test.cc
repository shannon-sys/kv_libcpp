#include <iostream>

#include "../util/random.h"
#include "swift/comparator.h"
#include "../util/skiplist.h"
#include "../util/arena.h"
#include <set>
#include <map>

#include <swift/shannon_db.h>
#include "swift/write_batch_with_index.h"
using namespace shannon;
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
using namespace std;
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

struct Entry {
  std::string key;
  std::string value;
  WriteType type;
};

void AssertIter(Iterator *iter, const std::string &key,
                const std::string &value) {
  CheckCondition(iter->Valid());
  CheckEqual(key.data(), iter->key().data(), iter->key().size());
  CheckEqual(value.data(), iter->value().data(), iter->value().size());
}
static std::string PrintContents(WriteBatchWithIndex *batch,
                                 ColumnFamilyHandle *column_family) {
  std::string result;

  WBWIIterator *iter;
  if (column_family == nullptr) {
    iter = batch->NewIterator();
  } else {
    iter = batch->NewIterator(column_family);
  }

  iter->SeekToFirst();
  while (iter->Valid()) {
    WriteEntry e = iter->Entry();
    if (e.type == kPutRecord) {
      result.append("PUT(");
      result.append(e.key.ToString());
      result.append("):");
      result.append(e.value.ToString());
    } else {
      result.append("DEL(");
      result.append(e.key.ToString());
      result.append(")");
    }

    result.append(",");
    iter->Next();
  }

  delete iter;
  return result;
}

typedef std::map<std::string, std::string> KVMap;

class KVIter : public Iterator {
public:
  KVIter(const KVMap *map) : map_(map), iter_(map_->end()) {}
  virtual bool Valid() const { return iter_ != map_->end(); }
  virtual void SeekToFirst() { iter_ = map_->begin(); }
  virtual void SeekToLast() {
    if (map_->empty()) {
      iter_ = map_->end();
    } else {
      iter_ = map_->find(map_->rbegin()->first);
    }
  }
  virtual void Seek(const Slice &k) { iter_ = map_->lower_bound(k.ToString()); }
  virtual void SeekForPrev(const Slice &k) {
    iter_ = map_->upper_bound(k.ToString());
    Prev();
  }
  virtual void Next() { ++iter_; }
  virtual void Prev() {
    if (iter_ == map_->begin()) {
      iter_ = map_->end();
      return;
    }
    --iter_;
  }

  virtual Slice key();
  virtual Slice value();
  virtual uint64_t timestamp();
  virtual Status status() const { return Status::OK(); }
  virtual void SetPrefix(const Slice &prefix);

private:
  const KVMap *const map_;
  KVMap::const_iterator iter_;
};

Slice KVIter::key() { return iter_->first; }
Slice KVIter::value() { return iter_->second; }
uint64_t KVIter::timestamp() {
  return 0;
};
void KVIter::SetPrefix(const Slice &prefix) {}

int main() {
  shannon::DB *db;
  shannon::Options options;
  shannon::DBOptions db_options;
  shannon::Status status;
  std::string dbname = "test_sst.db";
  std::string device = "/dev/kvdev0";
  int verify = 1;
  char *filename;
  int decode_cfname = 0;
  std::vector<ColumnFamilyHandle *> handles;
  std::vector<ColumnFamilyDescriptor> cf_des;
  std::string value("");
  ColumnFamilyOptions cf_option;
  ColumnFamilyHandle *cf1;
  ColumnFamilyHandle *cf2;
  const Comparator *cmp;
  WriteBatch *wb = NULL;
  options.create_if_missing = true;
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
  cmp = BytewiseComparator();
  WriteBatchWithIndex batch(cmp, 20, true);
  batch.Put(cf1, "aaa", "aaa");
  batch.Put(cf1, "bbb", "bbb");
  batch.Put(cf1, "ccccccc", "ccccccc");
  batch.Put(cf1, "ddddd", "ccccccc");
  batch.Delete(cf1, "eee");
  batch.Delete(cf1, "ffff");
  WBWIIterator *iter = batch.NewIterator(cf1);
  {
    iter->SeekToFirst();
    CheckEqual("aaa", iter->Entry().key.data(), 3);
    CheckEqual("aaa", iter->Entry().value.data(), 3);
    CheckCondition(iter->Valid());
    iter->Next();
    CheckEqual("bbb", iter->Entry().key.data(), 3);
    CheckEqual("bbb", iter->Entry().value.data(), 3);
    CheckCondition(iter->Valid());
    iter->Next();
    CheckEqual("ccccccc", iter->Entry().key.data(), 7);
    CheckEqual("ccccccc", iter->Entry().value.data(), 7);
    CheckCondition(iter->Valid());
    iter->Prev();
    CheckEqual("bbb", iter->Entry().key.data(), 3);
    CheckEqual("bbb", iter->Entry().value.data(), 3);
    CheckCondition(iter->Valid());
    iter->SeekToLast();
    CheckEqual("ffff", iter->Entry().key.data(), 4);
    CheckCondition(iter->Valid());
    iter->Prev();
    CheckEqual("eee", iter->Entry().key.data(), 3);
    CheckCondition(iter->Valid());
  }
  batch.Clear();
  {
    KVMap map;
    map["a"] = "aa";
    map["c"] = "cc";
    map["e"] = "ee";
    std::unique_ptr<Iterator> iter(
        batch.NewIteratorWithBase(cf1, new KVIter(&map)));

    iter->SeekToFirst();
    AssertIter(iter.get(), "a", "aa");
    iter->Next();
    AssertIter(iter.get(), "c", "cc");
    iter->Next();
    AssertIter(iter.get(), "e", "ee");
    iter->Next();
    CheckCondition(!iter->Valid());

    iter->SeekToLast();
    AssertIter(iter.get(), "e", "ee");
    iter->Prev();
    AssertIter(iter.get(), "c", "cc");
    iter->Prev();
    AssertIter(iter.get(), "a", "aa");
    iter->Prev();
    CheckCondition(!iter->Valid());

    iter->Seek("b");
    AssertIter(iter.get(), "c", "cc");

    iter->Prev();
    AssertIter(iter.get(), "a", "aa");

    iter->Seek("a");
    AssertIter(iter.get(), "a", "aa");
  }

  batch.Put(cf2, "zoo", "bar");
  batch.Put(cf1, "a", "aa");
  {
    KVMap empty_map;
    std::unique_ptr<Iterator> iter(
        batch.NewIteratorWithBase(cf1, new KVIter(&empty_map)));

    iter->SeekToFirst();
    AssertIter(iter.get(), "a", "aa");
    iter->Next();
    CheckCondition(!iter->Valid());
  }

  batch.Delete(cf1, "b");
  batch.Put(cf1, "c", "cc");
  batch.Put(cf1, "d", "dd");
  batch.Delete(cf1, "e");

  {
    KVMap map;
    map["b"] = "";
    map["cc"] = "cccc";
    map["f"] = "ff";
    std::unique_ptr<Iterator> iter(
        batch.NewIteratorWithBase(cf1, new KVIter(&map)));

    iter->SeekToFirst();
    AssertIter(iter.get(), "a", "aa");
    iter->Next();
    AssertIter(iter.get(), "c", "cc");
    iter->Next();
    AssertIter(iter.get(), "cc", "cccc");
    iter->Next();
    AssertIter(iter.get(), "d", "dd");
    iter->Next();
    AssertIter(iter.get(), "f", "ff");
    iter->Next();
    CheckCondition(!iter->Valid());

    iter->SeekToLast();
    AssertIter(iter.get(), "f", "ff");
    iter->Prev();
    AssertIter(iter.get(), "d", "dd");
    iter->Prev();
    AssertIter(iter.get(), "cc", "cccc");
    iter->Prev();
    AssertIter(iter.get(), "c", "cc");
    iter->Next();
    AssertIter(iter.get(), "cc", "cccc");
    iter->Prev();
    AssertIter(iter.get(), "c", "cc");
    iter->Prev();
    AssertIter(iter.get(), "a", "aa");
    iter->Prev();
    CheckCondition(!iter->Valid());

    iter->Seek("c");
    AssertIter(iter.get(), "c", "cc");

    iter->Seek("cb");
    AssertIter(iter.get(), "cc", "cccc");

    iter->Seek("cc");
    AssertIter(iter.get(), "cc", "cccc");
    iter->Next();
    AssertIter(iter.get(), "d", "dd");

    iter->Seek("e");
    AssertIter(iter.get(), "f", "ff");

    iter->Prev();
    AssertIter(iter.get(), "d", "dd");

    iter->Next();
    AssertIter(iter.get(), "f", "ff");
  }

  cout << PrintContents(&batch, cf1) << endl;
  cout << batch.Count() << endl;
  return 0;
}
