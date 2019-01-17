// Copyright (c) 2018 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef CACHE_H_
#define CACHE_H_

#include <stdint.h>
#include <memory>
#include <string>
#include "swift/slice.h"
#include "swift/status.h"

namespace shannon {

class Cache;
extern std::shared_ptr<Cache> NewLRUCache(size_t capacity,
                                          int num_shard_bits = -1,
                                          bool strict_capacity_limit = false,
                                          double high_pri_pool_ratio = 0.0);

class Cache {
 public:
  Cache(size_t capacity) : cache_size_(capacity) {  }
 private:
  size_t cache_size_;
};

}  // namespace shannon

#endif  // CACHE_H_
