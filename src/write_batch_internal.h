// Copyright (c) 2018 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_SHANNON_DB_WRITE_BATCH_INTERNAL_H_
#define STORAGE_SHANNON_DB_WRITE_BATCH_INTERNAL_H_

#include "swift/write_batch.h"

namespace shannon {

class WriteBatchInternal {
 public:
  // Return the number of entries in the batch.
  static int Count(const WriteBatch* batch);

  // Set the count for the number of entries in the batch.
  static void SetCount(WriteBatch* batch, int n);

  static void SetSize(WriteBatch* batch, size_t n);

  static void SetValueSize(WriteBatch* batch, size_t n);

  static void SetHandle(WriteBatch* batch, int n);

  static void SetFillCache(WriteBatch* batch, int n);

  static Slice Contents(const WriteBatch* batch) {
    return Slice(batch->rep_);
  }

  static size_t ByteSize(const WriteBatch* batch) {
    return batch->rep_.size();
  }

  static size_t GetValueSize(const WriteBatch* batch);

  static void SetContents(WriteBatch* batch, const Slice& contents);


  static void Append(WriteBatch* dst, const WriteBatch* src);

  static int Valid(const WriteBatch* batch);
};

class WriteBatchInternalNonatomic {
 public:
  // Return the number of entries in the batch.
  static int Count(const WriteBatchNonatomic* batch);

  // Set the count for the number of entries in the batch.
  static void SetCount(WriteBatchNonatomic* batch, int n);

  static void SetSize(WriteBatchNonatomic* batch, size_t n);

  static void SetValueSize(WriteBatchNonatomic* batch, size_t n);

  static void SetHandle(WriteBatchNonatomic* batch, int n);

  static void SetFillCache(WriteBatchNonatomic* batch, int n);

  static Slice Contents(const WriteBatchNonatomic* batch) {
    return Slice(batch->rep_);
  }

  static size_t ByteSize(const WriteBatchNonatomic* batch) {
    return batch->rep_.size();
  }

  static size_t GetValueSize(const WriteBatchNonatomic* batch);

  static void SetContents(WriteBatchNonatomic* batch, const Slice& contents);

  static void Append(WriteBatchNonatomic* dst, const WriteBatch* src);

  static int Valid(const WriteBatchNonatomic* batch);
};

}  // namespace shannon


#endif  // STORAGE_SHANNON_DB_WRITE_BATCH_INTERNAL_H_
