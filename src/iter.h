// Copyright (c) 2018 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_SHANNONDB_INCLUDE_ITER_H_
#define STORAGE_SHANNONDB_INCLUDE_ITER_H_

#include <stdint.h>

namespace shannon {

class KVImpl;

// Return a new iterator that converts internal keys (yielded by
// "*internal_iter") that were live at the specified "sequence" number
// into appropriate user keys.
extern Iterator* NewDBIterator(
    KVImpl* db, int iter_index, int cf_index, uint64_t timestamp);

}  // namespace shannon

#endif  // STORAGE_SHANNONDB_INCLUDE_ITER_H_
