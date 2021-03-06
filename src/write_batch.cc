// Copyright (c) 2018 Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
#include "swift/shannon_db.h"
#include "swift/write_batch.h"
#include "write_batch_internal.h"
#include "util/coding.h"
#include "util/util.h"
#include "src/venice_kv.h"

namespace shannon {

extern const size_t kHeader = sizeof(struct write_batch_header);

WriteBatch::WriteBatch() {
  value_.reserve(MEM_RESET_SIZE);
  Clear();
}
//write_batch_with_index
WriteBatch::WriteBatch(size_t reserved_bytes, size_t max_bytes) {
  value_.reserve(MEM_RESET_SIZE);
  rep_.reserve((reserved_bytes > kHeader) ?
    reserved_bytes :kHeader);
  rep_.resize(kHeader);
}

WriteBatch::~WriteBatch() { }

WriteBatch::Handler::~Handler() { }

void WriteBatch::Clear() {
  rep_.clear();
  rep_.resize(kHeader);
  value_.clear();
  offset_.clear();
}

Status WriteBatch::Iterate(Handler* handler) const {
  Slice input(rep_);
  if (input.size() < kHeader) {
    return Status::Corruption("malformed WriteBatch (too small)");
  }

  input.remove_prefix(kHeader);
  Slice key, value;
  int found = 0;
  while (!input.empty()) {
    found++;
    struct writebatch_cmd *cmd = (struct writebatch_cmd *)input.data();
    if (cmd->watermark != CMD_START_MARK)
      return Status::Corruption("bad cmd start mark");
    switch (cmd->cmd_type) {
      case kTypeValue:
        key = Slice(cmd->key, cmd->key_len);
        value = Slice(cmd->value, cmd->value_len);
        handler->Put(key, value);
        break;
      case kTypeDeletion:
        key = Slice(cmd->key, cmd->key_len);
        handler->Delete(key);
        break;
      default:
        return Status::Corruption("unknown WriteBatch type");
    }
    input.remove_prefix(sizeof(*cmd));
  }
  if (found != WriteBatchInternal::Count(this)) {
    return Status::Corruption("WriteBatch has wrong count");
  } else {
    return Status::OK();
  }
}
int WriteBatch::Count() const {
  return DecodeFixed32(rep_.data() + OFFSET(write_batch_header, count));
}
int WriteBatchInternal::Count(const WriteBatch* b) {
  return DecodeFixed32(b->rep_.data() + OFFSET(write_batch_header, count));
}

void WriteBatchInternal::SetCount(WriteBatch* b, int n) {
  EncodeFixed32(&b->rep_[OFFSET(write_batch_header, count)], n);
}

void WriteBatchInternal::SetSize(WriteBatch* b, size_t n) {
  EncodeFixed64(&b->rep_[OFFSET(write_batch_header, size)], n);
}

void WriteBatchInternal::SetValueSize(WriteBatch* b, size_t n) {
  size_t value_size = DecodeFixed64(b->rep_.data() + OFFSET(write_batch_header, value_size));
  EncodeFixed64(&b->rep_[OFFSET(write_batch_header, value_size)], n + value_size);
}

size_t WriteBatchInternal::GetValueSize(const WriteBatch* b) {
       return (size_t)DecodeFixed64(b->rep_.data() + OFFSET(write_batch_header, value_size));
}

void WriteBatchInternal::SetHandle(WriteBatch* b, int n) {
  EncodeFixed32(&b->rep_[OFFSET(write_batch_header, db_index)], n);
}

void WriteBatchInternal::SetFillCache(WriteBatch* b, int n) {
  EncodeFixed32(&b->rep_[OFFSET(write_batch_header, fill_cache)], n);
}

int WriteBatchInternal::Valid(const WriteBatch* batch) {
  size_t value_size = WriteBatchInternal::GetValueSize(batch);
  size_t byte_size = WriteBatchInternal::ByteSize(batch);
  int count = WriteBatchInternal::Count(batch);
  if ((byte_size <= sizeof(struct write_batch_header)) || (value_size < 0) ||
      (byte_size + value_size > MAX_BATCH_SIZE) ||
      (count > MAX_BATCH_COUNT) || (count <= 0))
    return 0;
  else
    return 1;
}

Status WriteBatch::Put(ColumnFamilyHandle* column_family, const Slice& key,
        const Slice& value) {
    size_t value_size = WriteBatchInternal::GetValueSize(this);
    size_t byte_size = WriteBatchInternal::ByteSize(this);
    int count = WriteBatchInternal::Count(this);
    if (column_family == NULL || key.data() == NULL || key.size() <= 0 || value.data() == NULL || value.size() < 0)
        return Status::Corruption("format error: key , value or column family.");
    if ((byte_size + value_size + sizeof(struct writebatch_cmd) + key.size() + value.size() > MAX_BATCH_SIZE) ||
        (count >= MAX_BATCH_COUNT)) {
        return Status::BatchFull();
    }
    WriteBatchInternal::SetCount(this, WriteBatchInternal::Count(this) + 1);
    PutFixed64(&rep_, static_cast<uint64_t>(CMD_START_MARK));
    PutFixed64(&rep_, static_cast<uint64_t>(0));
    PutFixed32(&rep_, static_cast<int>(kTypeValue));
    PutFixed32(&rep_, static_cast<int>(
              (reinterpret_cast<const ColumnFamilyHandle *>(column_family))->GetID()));
    PutFixed32(&rep_, key.size());
    PutFixed32(&rep_, value.size());
    offset_.push_back({rep_.size(),value_.size()});
    value_.append(value.data(), value.size());
    PutFixedAlign(&rep_, value_.data() + value_.size() - value.size());
    PutSliceData(&rep_, key);
    WriteBatchInternal::SetSize(this, WriteBatchInternal::ByteSize(this));
    WriteBatchInternal::SetValueSize(this, value.size());
    return Status::OK();
}

size_t WriteBatch::GetDataSize() const {
	return WriteBatchInternal::ByteSize(this);
}

Status WriteBatch::Put(const Slice& key, const Slice& value) {
  size_t value_size = WriteBatchInternal::GetValueSize(this);
  size_t byte_size = WriteBatchInternal::ByteSize(this);
  int count = WriteBatchInternal::Count(this);
  if (key.data() == NULL || key.size() <= 0 || value.data() == NULL || value.size() < 0)
    return Status::Corruption("format error: key , value or column family.");
  if ((byte_size + value_size + sizeof(struct writebatch_cmd) + key.size() + value.size() > MAX_BATCH_SIZE) ||
      (count >= MAX_BATCH_COUNT)) {
    return Status::BatchFull();
  }
  WriteBatchInternal::SetCount(this, WriteBatchInternal::Count(this) + 1);
  PutFixed64(&rep_, static_cast<uint64_t>(CMD_START_MARK));
  PutFixed64(&rep_, static_cast<uint64_t>(0));
  PutFixed32(&rep_, static_cast<int>(kTypeValue));
  PutFixed32(&rep_, static_cast<int>(0));
  PutFixed32(&rep_, key.size());
  PutFixed32(&rep_, value.size());
  offset_.push_back({rep_.size(),value_.size()});
  value_.append(value.data(), value.size());
  PutFixedAlign(&rep_, value_.data() + value_.size() - value.size());
  PutSliceData(&rep_, key);
  WriteBatchInternal::SetSize(this, WriteBatchInternal::ByteSize(this));
  WriteBatchInternal::SetValueSize(this, value.size());
  return Status::OK();
}

Status WriteBatch::Delete(ColumnFamilyHandle* column_family, const Slice& key) {
  size_t value_size = WriteBatchInternal::GetValueSize(this);
  size_t byte_size = WriteBatchInternal::ByteSize(this);
  int count = WriteBatchInternal::Count(this);
  if (column_family == NULL || key.data() == NULL || key.size() <= 0)
    return Status::Corruption("format error: key or column family.");
  if ((byte_size + value_size + sizeof(struct writebatch_cmd) + key.size() > MAX_BATCH_SIZE) ||
      (count >= MAX_BATCH_COUNT)) {
    return Status::BatchFull();
  }
  WriteBatchInternal::SetCount(this, WriteBatchInternal::Count(this) + 1);
  PutFixed64(&rep_, static_cast<uint64_t>(CMD_START_MARK));
  PutFixed64(&rep_, static_cast<uint64_t>(0));
  PutFixed32(&rep_, static_cast<int>(kTypeDeletion));
  PutFixed32(&rep_, static_cast<int>(
            (reinterpret_cast<const ColumnFamilyHandle *>(column_family))->GetID()));
  PutFixed32(&rep_, key.size());
  PutFixed32(&rep_, static_cast<int>(0));
  PutFixedAlign(&rep_, 0);
  PutSliceData(&rep_, key);
  WriteBatchInternal::SetSize(this, WriteBatchInternal::ByteSize(this));
  return Status::OK();
}

Status WriteBatch::Delete(const Slice& key) {
  size_t value_size = WriteBatchInternal::GetValueSize(this);
  size_t byte_size = WriteBatchInternal::ByteSize(this);
  int count = WriteBatchInternal::Count(this);
  if (key.data() == NULL || key.size() <= 0)
    return Status::Corruption("format error: key or column family.");
  if ((byte_size + value_size + sizeof(struct writebatch_cmd) + key.size() > MAX_BATCH_SIZE) ||
      (count >= MAX_BATCH_COUNT)) {
    return Status::BatchFull();
  }
  WriteBatchInternal::SetCount(this, WriteBatchInternal::Count(this) + 1);
  PutFixed64(&rep_, static_cast<uint64_t>(CMD_START_MARK));
  PutFixed64(&rep_, static_cast<uint64_t>(0));
  PutFixed32(&rep_, static_cast<int>(kTypeDeletion));
  PutFixed32(&rep_, static_cast<int>(0));
  PutFixed32(&rep_, key.size());
  PutFixed32(&rep_, static_cast<int>(0));
  PutFixedAlign(&rep_, 0);
  PutSliceData(&rep_, key);
  WriteBatchInternal::SetSize(this, WriteBatchInternal::ByteSize(this));
  return Status::OK();
}

void WriteBatch::SetOffset() {
  if (value_.size() < MEM_RESET_SIZE) {
    return ;
  } else {
    for (auto iter: offset_ ) {
      SetFixedAlign(const_cast<char*>(rep_.data()+iter.first),
        const_cast<char*>(value_.data()+iter.second));
    }
  }
}
// WriteBatchNonatomic
WriteBatchNonatomic::WriteBatchNonatomic() {
  value_.reserve(MEM_RESET_SIZE);
  Clear();
}

WriteBatchNonatomic::~WriteBatchNonatomic() { }

WriteBatchNonatomic::Handler::~Handler() { }

void WriteBatchNonatomic::Clear() {
  rep_.clear();
  rep_.resize(kHeader);
  value_.clear();
  offset_.clear();
}

Status WriteBatchNonatomic::Iterate(Handler* handler) const {
  Slice input(rep_);
  if (input.size() < kHeader) {
    return Status::Corruption("malformed WriteBatch (too small)");
  }

  input.remove_prefix(kHeader);
  Slice key, value;
  int found = 0;
  while (!input.empty()) {
    found++;
    struct writebatch_cmd *cmd = (struct writebatch_cmd *)input.data();
    if (cmd->watermark != CMD_START_MARK)
      return Status::Corruption("bad cmd start mark");
    switch (cmd->cmd_type) {
      case kTypeValue:
        key = Slice(cmd->key, cmd->key_len);
        value = Slice(cmd->value, cmd->value_len);
        handler->Put(key, value);
        break;
      case kTypeDeletion:
        key = Slice(cmd->key, cmd->key_len);
        handler->Delete(key);
        break;
      default:
        return Status::Corruption("unknown WriteBatch type");
    }
    input.remove_prefix(sizeof(*cmd));
  }
  if (found != WriteBatchInternalNonatomic::Count(this)) {
    return Status::Corruption("WriteBatch has wrong count");
  } else {
    return Status::OK();
  }
}

int WriteBatchInternalNonatomic::Count(const WriteBatchNonatomic* b) {
  return DecodeFixed32(b->rep_.data() + OFFSET(write_batch_header, count));
}

void WriteBatchInternalNonatomic::SetCount(WriteBatchNonatomic* b, int n) {
  EncodeFixed32(&b->rep_[OFFSET(write_batch_header, count)], n);
}

void WriteBatchInternalNonatomic::SetSize(WriteBatchNonatomic* b, size_t n) {
  EncodeFixed64(&b->rep_[OFFSET(write_batch_header, size)], n);
}

void WriteBatchInternalNonatomic::SetValueSize(WriteBatchNonatomic* b, size_t n) {
  size_t value_size = DecodeFixed64(b->rep_.data() + OFFSET(write_batch_header, value_size));
  EncodeFixed64(&b->rep_[OFFSET(write_batch_header, value_size)], n + value_size);
}

size_t WriteBatchInternalNonatomic::GetValueSize(const WriteBatchNonatomic* b) {
  return (size_t)DecodeFixed64(b->rep_.data() + OFFSET(write_batch_header, value_size));
}

void WriteBatchInternalNonatomic::SetHandle(WriteBatchNonatomic* b, int n) {
  EncodeFixed32(&b->rep_[OFFSET(write_batch_header, db_index)], n);
}

void WriteBatchInternalNonatomic::SetFillCache(WriteBatchNonatomic* b, int n) {
  EncodeFixed32(&b->rep_[OFFSET(write_batch_header, fill_cache)], n);
}

int WriteBatchInternalNonatomic::Valid(const WriteBatchNonatomic* batch) {
  size_t value_size = WriteBatchInternalNonatomic::GetValueSize(batch);
  size_t byte_size = WriteBatchInternalNonatomic::ByteSize(batch);
  int count = WriteBatchInternalNonatomic::Count(batch);
  if ((byte_size <= sizeof(struct write_batch_header)) || (value_size < 0) ||
      (byte_size + value_size > MAX_BATCH_NONATOMIC_SIZE) ||
      (count > MAX_BATCH_NONATOMIC_COUNT) || (count <= 0))
    return 0;
  else
    return 1;
}

#define GENERATE_TIMESTAMP 0
Status WriteBatchNonatomic::Put(ColumnFamilyHandle* column_family, const Slice& key,
        const Slice& value) {
    return this->Put(column_family, key, value, GENERATE_TIMESTAMP);
}

Status WriteBatchNonatomic::Put(ColumnFamilyHandle* column_family, const Slice& key,
        const Slice& value, __u64 timestamp) {
    size_t value_size = WriteBatchInternalNonatomic::GetValueSize(this);
    size_t byte_size = WriteBatchInternalNonatomic::ByteSize(this);
    int count = WriteBatchInternalNonatomic::Count(this);
    if (column_family == NULL || key.data() == NULL || key.size() <= 0 || value.data() == NULL || value.size() < 0)
        return Status::Corruption("format error: key , value or column family.");
    if ((byte_size + value_size + sizeof(struct writebatch_cmd) + key.size() + value.size() > MAX_BATCH_NONATOMIC_SIZE) ||
        (count >= MAX_BATCH_NONATOMIC_COUNT)) {
        return Status::BatchFull();
    }
    WriteBatchInternalNonatomic::SetCount(this, WriteBatchInternalNonatomic::Count(this) + 1);
    PutFixed64(&rep_, static_cast<uint64_t>(CMD_START_MARK));
    PutFixed64(&rep_, static_cast<uint64_t>(timestamp));
    PutFixed32(&rep_, static_cast<int>(kTypeValue));
    PutFixed32(&rep_, static_cast<int>(
              (reinterpret_cast<const ColumnFamilyHandle *>(column_family))->GetID()));
    PutFixed32(&rep_, key.size());
    PutFixed32(&rep_, value.size());
    offset_.push_back({rep_.size(),value_.size()});
    value_.append(value.data(), value.size());
    PutFixedAlign(&rep_, value_.data() + value_.size() - value.size());
    PutSliceData(&rep_, key);
    WriteBatchInternalNonatomic::SetSize(this, WriteBatchInternalNonatomic::ByteSize(this));
    WriteBatchInternalNonatomic::SetValueSize(this, value.size());
    return Status::OK();
}

Status WriteBatchNonatomic::Put(const Slice& key, const Slice& value) {
    return this->Put(key, value, GENERATE_TIMESTAMP);
}

Status WriteBatchNonatomic::Put(const Slice& key, const Slice& value, __u64 timestamp) {
  size_t value_size = WriteBatchInternalNonatomic::GetValueSize(this);
  size_t byte_size = WriteBatchInternalNonatomic::ByteSize(this);
  int count = WriteBatchInternalNonatomic::Count(this);
  if (key.data() == NULL || key.size() <= 0 || value.data() == NULL || value.size() < 0)
    return Status::Corruption("format error: key , value or column family.");
  if ((byte_size + value_size + sizeof(struct writebatch_cmd) + key.size() + value.size() > MAX_BATCH_NONATOMIC_SIZE) ||
      (count >= MAX_BATCH_NONATOMIC_COUNT)) {
    return Status::BatchFull();
  }
  WriteBatchInternalNonatomic::SetCount(this, WriteBatchInternalNonatomic::Count(this) + 1);
  PutFixed64(&rep_, static_cast<uint64_t>(CMD_START_MARK));
  PutFixed64(&rep_, static_cast<uint64_t>(timestamp));
  PutFixed32(&rep_, static_cast<int>(kTypeValue));
  PutFixed32(&rep_, static_cast<int>(0));
  PutFixed32(&rep_, key.size());
  PutFixed32(&rep_, value.size());
  offset_.push_back({rep_.size(),value_.size()});
  value_.append(value.data(), value.size());
  PutFixedAlign(&rep_, value_.data() + value_.size() - value.size());
  PutSliceData(&rep_, key);
  WriteBatchInternalNonatomic::SetSize(this, WriteBatchInternalNonatomic::ByteSize(this));
  WriteBatchInternalNonatomic::SetValueSize(this, value.size());
  return Status::OK();
}

Status WriteBatchNonatomic::Delete(ColumnFamilyHandle* column_family, const Slice& key) {
    return this->Delete(column_family, key, GENERATE_TIMESTAMP);
}

Status WriteBatchNonatomic::Delete(ColumnFamilyHandle* column_family, const Slice& key, __u64 timestamp) {
  size_t value_size = WriteBatchInternalNonatomic::GetValueSize(this);
  size_t byte_size = WriteBatchInternalNonatomic::ByteSize(this);
  int count = WriteBatchInternalNonatomic::Count(this);
  if (column_family == NULL || key.data() == NULL || key.size() <= 0)
    return Status::Corruption("format error: key or column family.");
  if ((byte_size + value_size + sizeof(struct writebatch_cmd) + key.size() > MAX_BATCH_NONATOMIC_SIZE) ||
      (count >= MAX_BATCH_NONATOMIC_COUNT)) {
    return Status::BatchFull();
  }
  WriteBatchInternalNonatomic::SetCount(this, WriteBatchInternalNonatomic::Count(this) + 1);
  PutFixed64(&rep_, static_cast<uint64_t>(CMD_START_MARK));
  PutFixed64(&rep_, static_cast<uint64_t>(timestamp));
  PutFixed32(&rep_, static_cast<int>(kTypeDeletion));
  PutFixed32(&rep_, static_cast<int>(
            (reinterpret_cast<const ColumnFamilyHandle *>(column_family))->GetID()));
  PutFixed32(&rep_, key.size());
  PutFixed32(&rep_, static_cast<int>(0));
  PutFixedAlign(&rep_, 0);
  PutSliceData(&rep_, key);
  WriteBatchInternalNonatomic::SetSize(this, WriteBatchInternalNonatomic::ByteSize(this));
  return Status::OK();
}

Status WriteBatchNonatomic::Delete(const Slice& key) {
    return this->Delete(key, GENERATE_TIMESTAMP);
}

Status WriteBatchNonatomic::Delete(const Slice& key, __u64 timestamp) {
  size_t value_size = WriteBatchInternalNonatomic::GetValueSize(this);
  size_t byte_size = WriteBatchInternalNonatomic::ByteSize(this);
  int count = WriteBatchInternalNonatomic::Count(this);
  if (key.data() == NULL || key.size() <= 0)
    return Status::Corruption("format error: key or column family.");
  if ((byte_size + value_size + sizeof(struct writebatch_cmd) + key.size() > MAX_BATCH_NONATOMIC_SIZE) ||
      (count >= MAX_BATCH_NONATOMIC_COUNT)) {
    return Status::BatchFull();
  }
  WriteBatchInternalNonatomic::SetCount(this, WriteBatchInternalNonatomic::Count(this) + 1);
  PutFixed64(&rep_, static_cast<uint64_t>(CMD_START_MARK));
  PutFixed64(&rep_, static_cast<uint64_t>(timestamp));
  PutFixed32(&rep_, static_cast<int>(kTypeDeletion));
  PutFixed32(&rep_, static_cast<int>(0));
  PutFixed32(&rep_, key.size());
  PutFixed32(&rep_, static_cast<int>(0));
  PutFixedAlign(&rep_, 0);
  PutSliceData(&rep_, key);
  WriteBatchInternalNonatomic::SetSize(this, WriteBatchInternalNonatomic::ByteSize(this));
  return Status::OK();
}

void WriteBatchNonatomic::SetOffset() {
  if (value_.size() < MEM_RESET_SIZE) {
    return ;
  } else {
    for (auto iter: offset_ ) {
      SetFixedAlign(const_cast<char*>(rep_.data()+iter.first),
        const_cast<char*>(value_.data()+iter.second));
    }
  }
}
}
