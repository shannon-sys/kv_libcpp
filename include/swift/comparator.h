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
  virtual int Compare(const Slice& a, const Slice& b) const = 0;
  virtual bool Equal(const Slice& a, const Slice& b) const {
      return Compare(a, b) == 0;
  }
  virtual const char* Name() const = 0;
  virtual void FindShortestSeparator(
          std::string* start,
          const Slice& limit ) const = 0;
  virtual void FindShortSuccessor(std::string* key) const = 0;
  virtual const Comparator* GetRootComparator() const { return this; }
  virtual bool IsSameLengthImmediateSuccessor(const Slice&,
                                              const Slice&) const {
      return false;
  }
  virtual bool CanKeysWithDifferentByteContentsBeEqual() const { return true; }
};

extern const Comparator* BytewiseComparator();

}  // namespace shannon

#endif  // COMPARATOR_H_
