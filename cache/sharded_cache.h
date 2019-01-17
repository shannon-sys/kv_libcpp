// Copyright (c) 2018 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef SHARDED_CACHE_H_
#define SHARDED_CACHE_H_

#include <atomic>
#include <string>
#include "swift/cache.h"
#include "util/hash.h"

namespace shannon {

class ShardedCache : public Cache {
 public:
  ShardedCache(size_t capacity, int num_shard_bits, bool strict_capacity_limit);
};

}  // namespace shannon

#endif  // SHARDED_CACHE_H_
