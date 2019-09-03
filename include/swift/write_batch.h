// Copyright (c) 2018 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//

#ifndef SHANNON_DB_INCLUDE_WRITE_BATCH_H_
#define SHANNON_DB_INCLUDE_WRITE_BATCH_H_

#include <string>
#include <vector>
#include <linux/types.h>
#include <list>
#include "status.h"

namespace shannon {
class Slice;
class ColumnFamilyHandle;
class WriteBatch {
 public:
  WriteBatch();
  WriteBatch(size_t reserved_bytes, size_t max_bytes);
  ~WriteBatch();

  Status Put(ColumnFamilyHandle* column_family, const Slice& key,
          const Slice& value);
  // Store the mapping "key->value" in the database.
  Status Put(const Slice& key, const Slice& value);

  Status Delete(ColumnFamilyHandle* column_family, const Slice& key);
  // If the database contains a mapping for "key", erase it.  Else do nothing.
  Status Delete(const Slice& key);
  size_t GetDataSize() const;
  int Count() const;
  WriteBatch* GetWriteBatch(){ return this; }
  const std::string& Data() const { return rep_; }
  // Clear all updates buffered in this batch.
  void Clear();

  // Support for iterating over the contents of a batch.
  class Handler {
   public:
    virtual ~Handler();
    virtual void Put(const Slice& key, const Slice& value) = 0;
    virtual void Delete(const Slice& key) = 0;
  };
  Status Iterate(Handler* handler) const;

  // SetOffset because string address will change when append
  void SetOffset();
 private:
  friend class WriteBatchInternal;

  std::string rep_;  // See comment in write_batch.cc for the format of rep_
  std::string value_;
  std::vector<std::pair<int64_t,int64_t> > offset_;
};

class WriteBatchNonatomic {
 public:
  WriteBatchNonatomic();
  ~WriteBatchNonatomic();

  Status Put(ColumnFamilyHandle* column_family, const Slice& key,
          const Slice& value);
  Status Put(ColumnFamilyHandle* column_family, const Slice& key,
          const Slice& value, __u64 timestamp);
  // Store the mapping "key->value" in the database.
  Status Put(const Slice& key, const Slice& value);
  Status Put(const Slice& key, const Slice& value, __u64 timestamp);

  Status Delete(ColumnFamilyHandle* column_family, const Slice& key);
  Status Delete(ColumnFamilyHandle* column_family, const Slice& key,
          __u64 timestamp);
  // If the database contains a mapping for "key", erase it.  Else do nothing.
  Status Delete(const Slice& key);
  Status Delete(const Slice& key, __u64 timestamp);

  // Clear all updates buffered in this batch.
  void Clear();

  // Support for iterating over the contents of a batch.
  class Handler {
   public:
    virtual ~Handler();
    virtual void Put(const Slice& key, const Slice& value) = 0;
    virtual void Delete(const Slice& key) = 0;
  };
  Status Iterate(Handler* handler) const;
  void SetOffset();
 private:
  friend class WriteBatchInternalNonatomic;

  std::string rep_;  // See comment in write_batch.cc for the format of rep_
  std::string value_;
  std::vector<std::pair<int64_t,int64_t> > offset_;
};
}
#endif  // SHANNON_DB_INCLUDE_WRITE_BATCH_H_
