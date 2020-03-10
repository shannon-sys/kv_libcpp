// Copyright (c) 2018, Shannon, Inc.  All rights reserved.
// This source code is licensed under both the GPLv2 (found in the
// COPYING file in the root directory) and Apache 2.0 License
//

#ifndef STORAGE_SHANNONDB_INCLUDE_TYPES_H_
#define STORAGE_SHANNONDB_INCLUDE_TYPES_H_
#include "swift/slice.h"

namespace shannon {

typedef uint64_t SequenceNumber;

class CallBackPtr {
 public:
  virtual ~CallBackPtr() {}
  virtual void call_ptr(const shannon::Status &s) = 0;
};
}  //  namespace shannon

#endif //  STORAGE_SHANNONDB_INCLUDE_TYPES_H_
