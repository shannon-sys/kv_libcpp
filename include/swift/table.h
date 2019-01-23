// Copyright (c) 2018 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//

#ifndef TABLE_H_
#define TABLE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <iostream>

#include "swift/cache.h"
#include "swift/env.h"
#include "swift/iterator.h"
#include "swift/status.h"
#include "swift/filter_policy.h"

using namespace std;

namespace shannon {

enum ChecksumType : char {
  kNoChecksum = 0x0,
  kCRC32c = 0x1,
  kxxHash = 0x2,
};

// The index type that will be used for this table.
enum IndexType : char {
  // A space efficient index block that is optimized for
  // binary-search-based index.
  kBinarySearch,

  // The hash index, if enabled, will do the hash lookup when
  // Options prefix_extractor is provided.
  kHashSearch,

  // A two-level index implementation. Both levels are binary search indexes.
  kTwoLevelIndexSearch,
};

struct TableFactory {
};

struct BlockBasedTableOptions {
  std::shared_ptr<Cache> block_cache = nullptr;
  std::shared_ptr<const FilterPolicy> filter_policy = nullptr;
  bool no_block_cache = false;
  size_t block_size = 4 * 1024;
  bool cache_index_and_filter_blocks = false;
};

extern TableFactory* NewBlockBasedTableFactory(const BlockBasedTableOptions &
        table_options = BlockBasedTableOptions());
}  // namespace shannon

#endif  // TABLE_H_
