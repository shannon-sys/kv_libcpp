// Copyright (c) 2018 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//

#ifndef SHANNON_DB_INCLUDE_READ_BATCH_H_
#define SHANNON_DB_INCLUDE_READ_BATCH_H_

#include <string>
#include <linux/types.h>
#include <list>
#include "status.h"

namespace shannon {
class Slice;
class ColumnFamilyHandle;
class ReadBatch {
 public:
  ReadBatch();
  ReadBatch(size_t reserved_bytes, size_t max_bytes);
  ~ReadBatch();

  Status Get(ColumnFamilyHandle* column_family, const Slice& key);
  // Store the mapping "key->value" in the database.
  Status Get(const Slice& key);

  size_t GetDataSize() const;
  int Count() const;
  ReadBatch* GetReadBatch(){ return this; }
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

 private:
  friend class ReadBatchInternal;

  std::string rep_;
  std::vector<char*> values_;
};

}
#endif  // SHANNON_DB_INCLUDE_WRITE_BATCH_H_
