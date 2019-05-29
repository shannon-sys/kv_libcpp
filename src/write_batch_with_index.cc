#ifndef SHANNON_LITE

#include <memory>
#include <util/skiplist.h>
#include "swift/write_batch.h"
#include "src/column_family.h"
#include "util/likely.h"
#include "util/coding.h"
#include "swift/write_batch_with_index.h"
#include "src/venice_kv.h"
namespace shannon {

bool ReadKeyFromWriteBatchEntry(Slice *input, Slice *key, bool cf_record) {
  assert(input != nullptr && key != nullptr);
  struct writebatch_cmd *cmd = (struct writebatch_cmd *)input->data();
  if (cmd->watermark != CMD_START_MARK)
    return false;
  switch (cmd->cmd_type) {
  case kTypeValue:
    *key = Slice(cmd->key, cmd->key_len);
    break;
  case kTypeDeletion:
    *key = Slice(cmd->key, cmd->key_len);
    break;
  default:
    return false;
  }
  return true;
}

Status ReadRecordFromWriteBatch(Slice *input, WriteType *tag, Slice *key,
                                Slice *value, Slice *blob, Slice *xid) {
  assert(key != nullptr && value != nullptr);
  struct writebatch_cmd *cmd = (struct writebatch_cmd *)input->data();
  if (cmd->watermark != CMD_START_MARK)
    return Status::Corruption("unknown WriteBatch watermark ");
  switch (cmd->cmd_type) {
  case kTypeValue:
    *key = Slice(cmd->key, cmd->key_len);
    *value = Slice(cmd->value, cmd->value_len);
    *tag = kPutRecord;
    break;
  case kTypeDeletion:
    *key = Slice(cmd->key, cmd->key_len);
    *value = Slice("", 0);
    *tag = kDeleteRecord;
    break;
  default:
    return Status::Corruption("unknown WriteBatch cmd_type ");
  }
  return Status::OK();
}
class BaseDeltaIterator : public Iterator {
public:
  BaseDeltaIterator(Iterator *base_iterator, WBWIIterator *delta_iterator,
                    const Comparator *comparator)
      : forward_(true), current_at_base_(true), equal_keys_(false),
        status_(Status::OK()), base_iterator_(base_iterator),
        delta_iterator_(delta_iterator), comparator_(comparator) {}

  virtual ~BaseDeltaIterator() {}

  bool Valid() const override {
    return current_at_base_ ? BaseValid() : DeltaValid();
  }

  void SeekToFirst() override {
    forward_ = true;
    base_iterator_->SeekToFirst();
    delta_iterator_->SeekToFirst();
    UpdateCurrent();
  }

  void SeekToLast() override {
    forward_ = false;
    base_iterator_->SeekToLast();
    delta_iterator_->SeekToLast();
    UpdateCurrent();
  }

  void Seek(const Slice &k) override {
    forward_ = true;
    base_iterator_->Seek(k);
    delta_iterator_->Seek(k);
    UpdateCurrent();
  }

  void SeekForPrev(const Slice &k) override {
    forward_ = false;
    base_iterator_->SeekForPrev(k);
    delta_iterator_->SeekForPrev(k);
    UpdateCurrent();
  }

  void Next() override {
    if (!Valid()) {
      status_ = Status::NotSupported("Next() on invalid iterator");
      return;
    }

    if (!forward_) {
      // Need to change direction
      // if our direction was backward and we're not equal, we have two states:
      // * both iterators are valid: we're already in a good state (current
      // shows to smaller)
      // * only one iterator is valid: we need to advance that iterator
      forward_ = true;
      equal_keys_ = false;
      if (!BaseValid()) {
        assert(DeltaValid());
        base_iterator_->SeekToFirst();
      } else if (!DeltaValid()) {
        delta_iterator_->SeekToFirst();
      } else if (current_at_base_) {
        // Change delta from larger than base to smaller
        AdvanceDelta();
      } else {
        // Change base from larger than delta to smaller
        AdvanceBase();
      }
      if (DeltaValid() && BaseValid()) {
        if (comparator_->Equal(delta_iterator_->Entry().key,
                               base_iterator_->key())) {
          equal_keys_ = true;
        }
      }
    }
    Advance();
  }

  void Prev() override {
    if (!Valid()) {
      status_ = Status::NotSupported("Prev() on invalid iterator");
      return;
    }

    if (forward_) {
      // Need to change direction
      // if our direction was backward and we're not equal, we have two states:
      // * both iterators are valid: we're already in a good state (current
      // shows to smaller)
      // * only one iterator is valid: we need to advance that iterator
      forward_ = false;
      equal_keys_ = false;
      if (!BaseValid()) {
        assert(DeltaValid());
        base_iterator_->SeekToLast();
      } else if (!DeltaValid()) {
        delta_iterator_->SeekToLast();
      } else if (current_at_base_) {
        // Change delta from less advanced than base to more advanced
        AdvanceDelta();
      } else {
        // Change base from less advanced than delta to more advanced
        AdvanceBase();
      }
      if (DeltaValid() && BaseValid()) {
        if (comparator_->Equal(delta_iterator_->Entry().key,
                               base_iterator_->key())) {
          equal_keys_ = true;
        }
      }
    }

    Advance();
  }

  Slice key() {
    return current_at_base_ ? base_iterator_->key()
                            : delta_iterator_->Entry().key;
  }

  uint64_t timestamp() { return 0; }
  void SetPrefix(const Slice &prefix) {}

  Slice value() {
    return current_at_base_ ? base_iterator_->value()
                            : delta_iterator_->Entry().value;
  }

  Status status() const override {
    if (!status_.ok()) {
      return status_;
    }
    if (!base_iterator_->status().ok()) {
      return base_iterator_->status();
    }
    return delta_iterator_->status();
  }

private:
  void AssertInvariants() {
#ifndef NDEBUG
    bool not_ok = false;
    if (!base_iterator_->status().ok()) {
      assert(!base_iterator_->Valid());
      not_ok = true;
    }
    if (!delta_iterator_->status().ok()) {
      assert(!delta_iterator_->Valid());
      not_ok = true;
    }
    if (not_ok) {
      assert(!Valid());
      assert(!status().ok());
      return;
    }

    if (!Valid()) {
      return;
    }
    if (!BaseValid()) {
      assert(!current_at_base_ && delta_iterator_->Valid());
      return;
    }
    if (!DeltaValid()) {
      assert(current_at_base_ && base_iterator_->Valid());
      return;
    }
    // we don't support those yet
    int compare = comparator_->Compare(delta_iterator_->Entry().key,
                                       base_iterator_->key());
    if (forward_) {
      // current_at_base -> compare < 0
      assert(!current_at_base_ || compare < 0);
      // !current_at_base -> compare <= 0
      assert(current_at_base_ && compare >= 0);
    } else {
      // current_at_base -> compare > 0
      assert(!current_at_base_ || compare > 0);
      // !current_at_base -> compare <= 0
      assert(current_at_base_ && compare <= 0);
    }
    // equal_keys_ <=> compare == 0
    assert((equal_keys_ || compare != 0) && (!equal_keys_ || compare == 0));
#endif
  }

  void Advance() {
    if (equal_keys_) {
      assert(BaseValid() && DeltaValid());
      AdvanceBase();
      AdvanceDelta();
    } else {
      if (current_at_base_) {
        assert(BaseValid());
        AdvanceBase();
      } else {
        assert(DeltaValid());
        AdvanceDelta();
      }
    }
    UpdateCurrent();
  }

  void AdvanceDelta() {
    if (forward_) {
      delta_iterator_->Next();
    } else {
      delta_iterator_->Prev();
    }
  }
  void AdvanceBase() {
    if (forward_) {
      base_iterator_->Next();
    } else {
      base_iterator_->Prev();
    }
  }
  bool BaseValid() const { return base_iterator_->Valid(); }
  bool DeltaValid() const { return delta_iterator_->Valid(); }
  void UpdateCurrent() {
// Suppress false positive clang analyzer warnings.
#ifndef __clang_analyzer__
    status_ = Status::OK();
    while (true) {
      WriteEntry delta_entry;
      if (DeltaValid()) {
        assert(delta_iterator_->status().ok());
        delta_entry = delta_iterator_->Entry();
      } else if (!delta_iterator_->status().ok()) {
        // Expose the error status and stop.
        current_at_base_ = false;
        return;
      }
      equal_keys_ = false;
      if (!BaseValid()) {
        if (!base_iterator_->status().ok()) {
          // Expose the error status and stop.
          current_at_base_ = true;
          return;
        }

        // Base has finished.
        if (!DeltaValid()) {
          // Finished
          return;
        }
        if (delta_entry.type == kDeleteRecord) {
          AdvanceDelta();
        } else {
          current_at_base_ = false;
          return;
        }
      } else if (!DeltaValid()) {
        // Delta has finished.
        current_at_base_ = true;
        return;
      } else {
        int compare =
            (forward_ ? 1 : -1) *
            comparator_->Compare(delta_entry.key, base_iterator_->key());
        if (compare <= 0) { // delta bigger or equal
          if (compare == 0) {
            equal_keys_ = true;
          }
          if (delta_entry.type != kDeleteRecord) {
            current_at_base_ = false;
            return;
          }
          // Delta is less advanced and is delete.
          AdvanceDelta();
          if (equal_keys_) {
            AdvanceBase();
          }
        } else {
          current_at_base_ = true;
          return;
        }
      }
    }

    AssertInvariants();
#endif // __clang_analyzer__
  }

  bool forward_;
  bool current_at_base_;
  bool equal_keys_;
  Status status_;
  std::unique_ptr<Iterator> base_iterator_;
  std::unique_ptr<WBWIIterator> delta_iterator_;
  const Comparator *comparator_; // not owned
};

typedef SkipList<WriteBatchIndexEntry *, const WriteBatchEntryComparator &>
WriteBatchEntrySkipList;

class WBWIIteratorImpl : public WBWIIterator {
public:
  WBWIIteratorImpl(uint32_t column_family_id,
                   WriteBatchEntrySkipList *skip_list,
                   const WriteBatch *write_batch)
      : column_family_id_(column_family_id), skip_list_iter_(skip_list),
        write_batch_(write_batch) {}

  virtual ~WBWIIteratorImpl() {}

  virtual bool Valid() const override {
    if (!skip_list_iter_.Valid()) {
      return false;
    }
    const WriteBatchIndexEntry *iter_entry = skip_list_iter_.key();
    return (iter_entry != nullptr &&
            iter_entry->column_family == column_family_id_);
  }

  virtual void SeekToFirst() override {
    WriteBatchIndexEntry search_entry(
        nullptr /* search_key */, column_family_id_,
        true /* is_forward_direction */, true /* is_seek_to_first */);
    skip_list_iter_.Seek(&search_entry);
  }

  virtual void SeekToLast() override {
    WriteBatchIndexEntry search_entry(
        nullptr /* search_key */, column_family_id_ + 1,
        true /* is_forward_direction */, true /* is_seek_to_first */);
    skip_list_iter_.Seek(&search_entry);
    if (!skip_list_iter_.Valid()) {
      skip_list_iter_.SeekToLast();
    } else {
      skip_list_iter_.Prev();
    }
  }

  virtual void Seek(const Slice &key) override {
    WriteBatchIndexEntry search_entry(&key, column_family_id_,
                                      true /* is_forward_direction */,
                                      false /* is_seek_to_first */);
    skip_list_iter_.Seek(&search_entry);
  }

  virtual void SeekForPrev(const Slice &key) override {
    WriteBatchIndexEntry search_entry(&key, column_family_id_,
                                      false /* is_forward_direction */,
                                      false /* is_seek_to_first */);
    skip_list_iter_.SeekForPrev(&search_entry);
  }

  virtual void Next() override { skip_list_iter_.Next(); }

  virtual void Prev() override { skip_list_iter_.Prev(); }

  virtual WriteEntry Entry() const override {
    WriteEntry ret;
    Slice blob, xid;
    const WriteBatchIndexEntry *iter_entry = skip_list_iter_.key();
    // this is guaranteed with Valid()
    assert(iter_entry != nullptr &&
           iter_entry->column_family == column_family_id_);
    const std::string &wb_data = write_batch_->Data();
    size_t entry_offset = iter_entry->offset;

    Slice entry_ptr =
        Slice(wb_data.data() + entry_offset, wb_data.size() - entry_offset);
    auto s = ReadRecordFromWriteBatch(&entry_ptr, &ret.type, &ret.key,
                                      &ret.value, &blob, &xid);
    assert(s.ok());
    assert(ret.type == kPutRecord || ret.type == kDeleteRecord);
    return ret;
  }

  virtual Status status() const override {
    // this is in-memory data structure, so the only way status can be non-ok is
    // through memory corruption
    return Status::OK();
  }

  const WriteBatchIndexEntry *GetRawEntry() const {
    return skip_list_iter_.key();
  }

private:
  uint32_t column_family_id_;
  WriteBatchEntrySkipList::Iterator skip_list_iter_;
  const WriteBatch *write_batch_;
};

struct WriteBatchWithIndex::Rep {
  explicit Rep(const Comparator *index_comparator, size_t reserved_bytes = 0,
               size_t max_bytes = 0, bool _overwrite_key = false)
      : write_batch(reserved_bytes, max_bytes),
        comparator(index_comparator, &write_batch),
        skip_list(comparator, &arena), overwrite_key(_overwrite_key),
        last_entry_offset(0), last_sub_batch_offset(0), sub_batch_cnt(1) {}
  WriteBatch write_batch;
  WriteBatchEntryComparator comparator;
  Arena arena;
  WriteBatchEntrySkipList skip_list;
  bool overwrite_key;
  size_t last_entry_offset;
  // The starting offset of the last sub-batch. A sub-batch starts right before
  // inserting a key that is a duplicate of a key in the last sub-batch. Zero,
  // the default, means that no duplicate key is detected so far.
  size_t last_sub_batch_offset;
  // Total number of sub-batches in the write batch. Default is 1.
  size_t sub_batch_cnt;

  // Remember current offset of internal write batch, which is used as
  // the starting offset of the next record.
  void SetLastEntryOffset() { last_entry_offset = write_batch.GetDataSize(); }

  // In overwrite mode, find the existing entry for the same key and update it
  // to point to the current entry.
  // Return true if the key is found and updated.
  bool UpdateExistingEntry(ColumnFamilyHandle *column_family, const Slice &key);
  bool UpdateExistingEntryWithCfId(uint32_t column_family_id, const Slice &key);

  // Add the recent entry to the update.
  // In overwrite mode, if key already exists in the index, update it.
  void AddOrUpdateIndex(ColumnFamilyHandle *column_family, const Slice &key);
  void AddOrUpdateIndex(const Slice &key);

  // Allocate an index entry pointing to the last entry in the write batch and
  // put it to skip list.
  void AddNewEntry(uint32_t column_family_id);

  // Clear all updates buffered in this batch.
  void Clear();
  void ClearIndex();
};

bool
WriteBatchWithIndex::Rep::UpdateExistingEntry(ColumnFamilyHandle *column_family,
                                              const Slice &key) {
  uint32_t cf_id = column_family->GetID();
  return UpdateExistingEntryWithCfId(cf_id, key);
}

bool
WriteBatchWithIndex::Rep::UpdateExistingEntryWithCfId(uint32_t column_family_id,
                                                      const Slice &key) {
  if (!overwrite_key) {
    return false;
  }

  WBWIIteratorImpl iter(column_family_id, &skip_list, &write_batch);
  iter.Seek(key);
  if (!iter.Valid()) {
    return false;
  }
  if (comparator.CompareKey(column_family_id, key, iter.Entry().key) != 0) {
    return false;
  }
  WriteBatchIndexEntry *non_const_entry =
      const_cast<WriteBatchIndexEntry *>(iter.GetRawEntry());
  if (LIKELY(last_sub_batch_offset <= non_const_entry->offset)) {
    last_sub_batch_offset = last_entry_offset;
    sub_batch_cnt++;
  }
  non_const_entry->offset = last_entry_offset;
  return true;
}

void
WriteBatchWithIndex::Rep::AddOrUpdateIndex(ColumnFamilyHandle *column_family,
                                           const Slice &key) {
  if (!UpdateExistingEntry(column_family, key)) {
    uint32_t cf_id = column_family->GetID();
    /*    const auto* cf_cmp = GetColumnFamilyUserComparator(column_family);
        if (cf_cmp != nullptr) {
          comparator.SetComparatorForCF(cf_id, cf_cmp);
        }
    */
    AddNewEntry(cf_id);
  }
}

void WriteBatchWithIndex::Rep::AddOrUpdateIndex(const Slice &key) {
  if (!UpdateExistingEntryWithCfId(0, key)) {
    AddNewEntry(0);
  }
}

void WriteBatchWithIndex::Rep::AddNewEntry(uint32_t column_family_id) {
  const std::string &wb_data = write_batch.Data();
  Slice entry_ptr = Slice(wb_data.data() + last_entry_offset,
                          wb_data.size() - last_entry_offset);
  // Extract key
  Slice key;
  bool success __attribute__((__unused__));
  success = ReadKeyFromWriteBatchEntry(&entry_ptr, &key, column_family_id != 0);
  assert(success);

  auto *mem = arena.Allocate(sizeof(WriteBatchIndexEntry));
  auto *index_entry =
      new (mem) WriteBatchIndexEntry(last_entry_offset, column_family_id,
                                     key.data() - wb_data.data(), key.size());
  skip_list.Insert(index_entry);
}

void WriteBatchWithIndex::Rep::Clear() {
  write_batch.Clear();
  ClearIndex();
}

void WriteBatchWithIndex::Rep::ClearIndex() {
  skip_list.~WriteBatchEntrySkipList();
  arena.~Arena();
  new (&arena) Arena();
  new (&skip_list) WriteBatchEntrySkipList(comparator, &arena);
  last_entry_offset = 0;
  last_sub_batch_offset = 0;
  sub_batch_cnt = 1;
}

WriteBatchWithIndex::WriteBatchWithIndex(
    const Comparator *default_index_comparator, size_t reserved_bytes,
    bool overwrite_key, size_t max_bytes)
    : rep(new Rep(default_index_comparator, reserved_bytes, max_bytes,
                  overwrite_key)) {}

WriteBatchWithIndex::~WriteBatchWithIndex() {}

WriteBatch *WriteBatchWithIndex::GetWriteBatch() { return &rep->write_batch; }

size_t WriteBatchWithIndex::SubBatchCnt() { return rep->sub_batch_cnt; }

WBWIIterator *WriteBatchWithIndex::NewIterator() {
  return new WBWIIteratorImpl(0, &(rep->skip_list), &rep->write_batch);
}

WBWIIterator *
WriteBatchWithIndex::NewIterator(ColumnFamilyHandle *column_family) {
  return new WBWIIteratorImpl(column_family->GetID(), &(rep->skip_list),
                              &rep->write_batch);
}
Status WriteBatchWithIndex::Put(ColumnFamilyHandle *column_family,
                                const Slice &key, const Slice &value) {
  rep->SetLastEntryOffset();
  auto s = rep->write_batch.Put(column_family, key, value);
  if (s.ok()) {
    rep->AddOrUpdateIndex(column_family, key);
  }
  return s;
}

Status WriteBatchWithIndex::Put(const Slice &key, const Slice &value) {
  rep->SetLastEntryOffset();
  auto s = rep->write_batch.Put(key, value);
  if (s.ok()) {
    rep->AddOrUpdateIndex(key);
  }
  return s;
}

Status WriteBatchWithIndex::Delete(ColumnFamilyHandle *column_family,
                                   const Slice &key) {
  rep->SetLastEntryOffset();
  auto s = rep->write_batch.Delete(column_family, key);
  if (s.ok()) {
    rep->AddOrUpdateIndex(column_family, key);
  }
  return s;
}

Status WriteBatchWithIndex::Delete(const Slice &key) {
  rep->SetLastEntryOffset();
  auto s = rep->write_batch.Delete(key);
  if (s.ok()) {
    rep->AddOrUpdateIndex(key);
  }
  return s;
}
void WriteBatchWithIndex::Clear() { rep->Clear(); }

int WriteBatchWithIndex::Count() const { return rep->write_batch.Count(); }

size_t WriteBatchWithIndex::GetDataSize() const {
  return rep->write_batch.GetDataSize();
}
Iterator *
WriteBatchWithIndex::NewIteratorWithBase(ColumnFamilyHandle *column_family,
                                         Iterator *base_iterator) {
  if (rep->overwrite_key == false) {
    assert(false);
    return nullptr;
  }
  return new BaseDeltaIterator(base_iterator, NewIterator(column_family),
                               rep->comparator.default_comparator());
}

Iterator *WriteBatchWithIndex::NewIteratorWithBase(Iterator *base_iterator) {
  if (rep->overwrite_key == false) {
    assert(false);
    return nullptr;
  }
  // default column family's comparator
  return new BaseDeltaIterator(base_iterator, NewIterator(),
                               rep->comparator.default_comparator());
}

int WriteBatchEntryComparator::
operator()(const WriteBatchIndexEntry *entry1,
           const WriteBatchIndexEntry *entry2) const {
  if (entry1->column_family > entry2->column_family) {
    return 1;
  } else if (entry1->column_family < entry2->column_family) {
    return -1;
  }

  // Deal with special case of seeking to the beginning of a column family
  if (entry1->is_min_in_cf()) {
    return -1;
  } else if (entry2->is_min_in_cf()) {
    return 1;
  }

  Slice key1, key2;
  if (entry1->search_key == nullptr) {
    key1 = Slice(write_batch_->Data().data() + entry1->key_offset,
                 entry1->key_size);
  } else {
    key1 = *(entry1->search_key);
  }
  if (entry2->search_key == nullptr) {
    key2 = Slice(write_batch_->Data().data() + entry2->key_offset,
                 entry2->key_size);
  } else {
    key2 = *(entry2->search_key);
  }

  int cmp = CompareKey(entry1->column_family, key1, key2);
  if (cmp != 0) {
    return cmp;
  } else if (entry1->offset > entry2->offset) {
    return 1;
  } else if (entry1->offset < entry2->offset) {
    return -1;
  }
  return 0;
}

int WriteBatchEntryComparator::CompareKey(uint32_t column_family,
                                          const Slice &key1,
                                          const Slice &key2) const {
  if (column_family < cf_comparators_.size() &&
      cf_comparators_[column_family] != nullptr) {
    return cf_comparators_[column_family]->Compare(key1, key2);
  } else {
    return default_comparator_->Compare(key1, key2);
  }
}

} // namespace SHANNON
#endif // !SHANNON_LITE
