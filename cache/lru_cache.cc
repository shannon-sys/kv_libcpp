// Copyright (c) 2018 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include "cache/lru_cache.h"
#include "swift/cache.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>

namespace shannon {

std::shared_ptr<Cache> NewLRUCache(size_t capacity, int num_shard_bits,
                bool strict_capacity_limit,
                double high_pri_pool_ratio) {
    return std::make_shared<LRUCache>(capacity, num_shard_bits,
                                    strict_capacity_limit, high_pri_pool_ratio);
}

LRUCache::LRUCache(size_t capacity, int num_shard_bits,
                   bool strict_capacity_limit, double high_pri_pool_ratio)
    : ShardedCache(capacity, num_shard_bits, strict_capacity_limit) {
    }

}  // namespace shannon
