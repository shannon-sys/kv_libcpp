#pragma once

#ifndef SHANNON_LITE

#include <memory>
#include <string>
#include <string>
#include <linux/types.h>
#include <list>
#include "swift/comparator.h"
#include "swift/iterator.h"
#include "swift/slice.h"
#include "swift/status.h"
#include "swift/write_batch.h"

using shannon::ColumnFamilyHandle;
namespace shannon {

class ColumnFamilyHandle;
class Comparator;
class DB;
class WriteBatch;
class ReadCallback;
struct ReadOptions;
struct DBOptions;

enum WriteType {
  kDeleteRecord = 2,
  kPutRecord = 3,
};

// an entry for Put, Merge, Delete, or SingleDelete entry for write batches.
// Used in WBWIIterator.
struct WriteEntry {
  WriteType type;
  Slice key;
  Slice value;
};

// Iterator of one column family out of a WriteBatchWithIndex.
class WBWIIterator {
 public:
  virtual ~WBWIIterator() {}

  virtual bool Valid() const = 0;

  virtual void SeekToFirst() = 0;

  virtual void SeekToLast() = 0;

  virtual void Seek(const Slice& key) = 0;

  virtual void SeekForPrev(const Slice& key) = 0;

  virtual void Next() = 0;

  virtual void Prev() = 0;

  // the return WriteEntry is only valid until the next mutation of
  // WriteBatchWithIndex
  virtual WriteEntry Entry() const = 0;

  virtual Status status() const = 0;
 private:
   Status status_;
};

class WriteBatchWithIndex : public WriteBatch {
 public:
  explicit WriteBatchWithIndex(
      const Comparator* backup_index_comparator = BytewiseComparator(),
      size_t reserved_bytes = 0, bool overwrite_key = false,
      size_t max_bytes = 0);

 // ~WriteBatchWithIndex() override;
  ~WriteBatchWithIndex();

  using WriteBatch::Put;
  Status Put(ColumnFamilyHandle* column_family, const Slice& key,
             const Slice& value);

  Status Put(const Slice& key, const Slice& value);
  using WriteBatch::GetDataSize;
  size_t GetDataSize() const;
  using WriteBatch::GetWriteBatch;
  WriteBatch* GetWriteBatch();
  using WriteBatch::Delete;
  Status Delete(ColumnFamilyHandle* column_family, const Slice& key);
  Status Delete(const Slice& key);
  using WriteBatch::Clear;
  void Clear();
  int Count() const;
  WBWIIterator* NewIterator();
  WBWIIterator* NewIterator(ColumnFamilyHandle* column_family);

  Iterator* NewIteratorWithBase(ColumnFamilyHandle* column_family,
                                Iterator* base_iterator);
  // default column family
  Iterator* NewIteratorWithBase(Iterator* base_iterator);
 private:
  size_t SubBatchCnt();
  struct Rep;
  std::unique_ptr<Rep> rep;
};

const size_t kMaxSizet = UINT64_MAX;
struct WriteBatchIndexEntry {
  WriteBatchIndexEntry(size_t o, uint32_t c, size_t ko, size_t ksz)
      : offset(o),
        column_family(c),
        key_offset(ko),
        key_size(ksz),
        search_key(nullptr) {}
  WriteBatchIndexEntry(const Slice* _search_key, uint32_t _column_family,
                       bool is_forward_direction, bool is_seek_to_first)
      : offset(is_forward_direction ? 0 : kMaxSizet),
        column_family(_column_family),
        key_offset(0),
        key_size(is_seek_to_first ? kFlagMinInCf : 0),
        search_key(_search_key) {
    assert(_search_key != nullptr || is_seek_to_first);
  }

  static const size_t kFlagMinInCf = kMaxSizet;

  bool is_min_in_cf() const {
    assert(key_size != kFlagMinInCf ||
           (key_offset == 0 && search_key == nullptr));
    return key_size == kFlagMinInCf;
  }
  size_t offset;
  uint32_t column_family;  // c1olumn family of the entry.
  size_t key_offset;       // offset of the key in write batch's string buffer.
  size_t key_size;         // size of the key. kFlagMinInCf indicates
  const Slice* search_key;  // if not null, instead of reading keys from
};

class ReadableWriteBatch : public WriteBatch {
 public:
  explicit ReadableWriteBatch(size_t reserved_bytes = 0, size_t max_bytes = 0)
      : WriteBatch(reserved_bytes, max_bytes) {}
  Status GetEntryFromDataOffset(size_t data_offset, WriteType* type, Slice* Key,
                                Slice* value, Slice* blob, Slice* xid) const;
};
class WriteBatchEntryComparator {
 public:
  WriteBatchEntryComparator(const Comparator* _default_comparator,
                            WriteBatch* write_batch)
      : default_comparator_(_default_comparator), write_batch_(write_batch) {}
  int operator()(const WriteBatchIndexEntry* entry1,
                 const WriteBatchIndexEntry* entry2) const;

  int CompareKey(uint32_t column_family, const Slice& key1,
                 const Slice& key2) const;

  void SetComparatorForCF(uint32_t column_family_id,
                          const Comparator* comparator) {
    if (column_family_id >= cf_comparators_.size()) {
      cf_comparators_.resize(column_family_id + 1, nullptr);
    }
    cf_comparators_[column_family_id] = comparator;
  }

  const Comparator* default_comparator() { return default_comparator_; }

 private:
  const Comparator* default_comparator_;
  std::vector<const Comparator*> cf_comparators_;
  const WriteBatch* write_batch_;
};
extern Status ReadEntryFromDataOffset(Slice* input, WriteType *type, Slice *key,
                                                  Slice *value, Slice *blob,
                                                  Slice *xid);
extern bool ReadKeyFromWriteBatchEntry(Slice* input, Slice* key,
                                       bool cf_record);

}  // namespace SHANNON

#endif  // !SHANNON_LITE
