// Copyright (c) 2018 The Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef COMPARATOR_H_
#define COMPARATOR_H_

#include <string>

namespace shannon {

class Slice;

class Comparator {
 public:
  virtual ~Comparator() {}
};

extern const Comparator* BytewiseComparator();

}  // namespace shannon

#endif  // COMPARATOR_H_
