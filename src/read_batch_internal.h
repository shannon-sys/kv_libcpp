// Copyright (c) 2018 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_SHANNON_DB_READ_BATCH_INTERNAL_H_
#define STORAGE_SHANNON_DB_READ_BATCH_INTERNAL_H_

#include "swift/read_batch.h"

namespace shannon {

class ReadBatchInternal {
 public:
  // Return the number of entries in the batch.
  static int Count(const ReadBatch* batch);

  // Set the count for the number of entries in the batch.
  static void SetCount(ReadBatch* batch, int n);

  static void SetSize(ReadBatch* batch, size_t n);

  static size_t ByteSize(const ReadBatch* batch) {
    return batch->rep_.size();
  }

  static const std::vector<char*> GetValues(const ReadBatch* batch) {
    return batch->values_;
  }

  static Slice Contents(const ReadBatch* batch) {
    return Slice(batch->rep_);
  }
  static int Valid(const ReadBatch* batch);

  static void SetHandle(ReadBatch* b, int n);

  static void SetFillCache(ReadBatch* b, int n);

  static void SetFailedCmdCount(ReadBatch* b, unsigned int *n);

  static void SetSnapshot(ReadBatch* b, uint64_t timestamp);
};

}  // namespace shannon


#endif  // STORAGE_SHANNON_DB_READ_BATCH_INTERNAL_H_
