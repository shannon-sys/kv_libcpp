// Copyright (c) 2018 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "swift/filter_policy.h"
#include "swift/slice.h"
#include "util/coding.h"

namespace shannon {

const FilterPolicy* NewBloomFilterPolicy(int bits_per_key,
                                         bool use_block_based_builder) {
  return nullptr;
}

}  // namespace shannon
