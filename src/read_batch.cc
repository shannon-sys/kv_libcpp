// Copyright (c) 2018 Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
#include "swift/shannon_db.h"
#include "swift/read_batch.h"
#include "src/read_batch_internal.h"
#include "util/coding.h"
#include "util/util.h"
#include "src/venice_kv.h"

namespace shannon {

extern const size_t kHeaderReadBatch = sizeof(struct read_batch_header);

ReadBatch::ReadBatch() {
  Clear();
}
// read_batch_with_index
ReadBatch::ReadBatch(size_t reserved_bytes, size_t max_bytes) {
  Clear();
  rep_.reserve((reserved_bytes > kHeaderReadBatch) ?
    reserved_bytes :kHeaderReadBatch);
  rep_.resize(kHeaderReadBatch);
}

ReadBatch::~ReadBatch() {
  rep_.clear();
  for (auto value : values_) {
    delete value;
  }
  values_.clear();
}

ReadBatch::Handler::~Handler() { }

void ReadBatch::Clear() {
  rep_.clear();
  rep_.resize(kHeaderReadBatch);
  for (auto value : values_) {
    delete value;
  }
  values_.clear();
}

Status ReadBatch::Iterate(Handler* handler) const {
  Slice input(rep_);
  if (input.size() < kHeaderReadBatch) {
    return Status::Corruption("malformed ReadBatch (too small)");
  }

  input.remove_prefix(kHeaderReadBatch);
  Slice key, value;
  int found = 0;
  while (!input.empty()) {
    found++;
    struct readbatch_cmd *cmd = (struct readbatch_cmd *)input.data();
    if (cmd->watermark != CMD_START_MARK)
      return Status::Corruption("bad cmd start mark");
    switch (cmd->cmd_type) {
      case kTypeValue:
        key = Slice(cmd->key, cmd->key_len);
        value = Slice(cmd->value, *cmd->value_len_addr);
        handler->Put(key, value);
        break;
      case kTypeDeletion:
        key = Slice(cmd->key, cmd->key_len);
        handler->Delete(key);
        break;
      default:
        return Status::Corruption("unknown ReadBatch type");
    }
    input.remove_prefix(sizeof(*cmd));
  }
  if (found != ReadBatchInternal::Count(this)) {
    return Status::Corruption("ReadBatch has wrong count");
  } else {
    return Status::OK();
  }
}

void ReadBatchInternal::SetCount(ReadBatch* b, int n) {
  EncodeFixed32(&b->rep_[OFFSET(read_batch_header, count)], n);
}

void ReadBatchInternal::SetSize(ReadBatch* b, size_t n) {
  EncodeFixed64(&b->rep_[OFFSET(read_batch_header, size)], n);
}

int ReadBatchInternal::Valid(const ReadBatch* batch) {
  size_t byte_size = ReadBatchInternal::ByteSize(batch);
  int count = ReadBatchInternal::Count(batch);
  if ((byte_size <= sizeof(struct read_batch_header)) ||
      (count > MAX_BATCH_COUNT) || (count <= 0))
    return 0;
  else
    return 1;
}

void ReadBatchInternal::SetHandle(ReadBatch* b, int n) {
  EncodeFixed32(&b->rep_[OFFSET(read_batch_header, db_index)], n);
}

void ReadBatchInternal::SetFillCache(ReadBatch* b, int n) {
  EncodeFixed32(&b->rep_[OFFSET(read_batch_header, fill_cache)], n);
}

void ReadBatchInternal::SetFailedCmdCount(ReadBatch* b, unsigned int *n) {
  if (sizeof(size_t) == 4) {
    EncodeFixed32(&b->rep_[OFFSET(read_batch_header, failed_cmd_count)], (size_t)n);
  } else {
    EncodeFixed64(&b->rep_[OFFSET(read_batch_header, failed_cmd_count)], (size_t)n);
  }
}

void ReadBatchInternal::SetSnapshot(ReadBatch* b, uint64_t timestamp) {
  EncodeFixed64(&b->rep_[OFFSET(read_batch_header, snapshot)], timestamp);
}

int ReadBatch::Count() const {
  return DecodeFixed32(rep_.data() + OFFSET(read_batch_header, count));
}
int ReadBatchInternal::Count(const ReadBatch* b) {
  return DecodeFixed32(b->rep_.data() + OFFSET(read_batch_header, count));
}

Status ReadBatch::Get(ColumnFamilyHandle* column_family, const Slice& key) {
    if (column_family == NULL) {
      return Status::InvalidArgument("column family is not null!");
    }
    return Get(static_cast<int>((
        reinterpret_cast<const ColumnFamilyHandle *>(column_family))->GetID()), key);
}

size_t ReadBatch::GetDataSize() const {
    return ReadBatchInternal::ByteSize(this);
}

Status ReadBatch::Get(const Slice& key) {
  return Get(0, key);
}

Status ReadBatch::Get(int column_family_id, const Slice& key, int value_buf_size) {
  size_t byte_size = ReadBatchInternal::ByteSize(this);
  int count = ReadBatchInternal::Count(this);
  if (key.data() == NULL || key.size() <= 0)
    return Status::Corruption("format error: key , value or column family.");
  if ((byte_size + sizeof(struct readbatch_cmd) + key.size() > MAX_BATCH_SIZE) ||
      (count >= MAX_BATCH_COUNT)) {
    return Status::BatchFull();
  }
  char *value = new char[value_buf_size + TYPE_SIZE(unsigned int, 3)];
  EncodeFixed32(value, rep_.size());                                    // save current cmd position

  ReadBatchInternal::SetCount(this, ReadBatchInternal::Count(this) + 1);
  PutFixed64(&rep_, static_cast<uint64_t>(CMD_START_MARK));
  PutFixed32(&rep_, static_cast<uint64_t>(GET_TYPE));
  PutFixed32(&rep_, column_family_id);
  PutFixed32(&rep_, value_buf_size);
  PutFixed32(&rep_, key.size());
  // append a element
  values_.push_back(value);
  // copy value address
  PutFixedAlign(&rep_, values_[values_.size() - 1] + TYPE_SIZE(unsigned int, 3)); // values
  PutFixedAlign(&rep_, values_[values_.size() - 1] + TYPE_SIZE(unsigned int, 2)); // value_len_addrs
  PutFixedAlign(&rep_, values_[values_.size() - 1] + TYPE_SIZE(unsigned int, 1)); // return_status
  PutSliceData(&rep_, key);
  ReadBatchInternal::SetSize(this, ReadBatchInternal::ByteSize(this));
  return Status::OK();
}


}  // namespace shannon
