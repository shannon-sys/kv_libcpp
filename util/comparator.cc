// Copyright (c) 2011 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <algorithm>
#include <memory>
#include <stdint.h>
#include "swift/comparator.h"
#include "swift/slice.h"

namespace shannon {

class BytewiseComparatorImpl : public Comparator {
 public:
  BytewiseComparatorImpl() { }
};

const Comparator* BytewiseComparator() {
  static BytewiseComparatorImpl bytewise;
  return &bytewise;
}

}  // namespace shannon
