// Copyright (c) 2018 Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
#ifndef SHANNON_DB_INCLUDE_SNAPSHOT_H_
#define SHANNON_DB_INCLUDE_SNAPSHOT_H_

#include "swift/shannon_db.h"

namespace shannon {

class SnapshotImpl : public Snapshot {
 public:
  uint64_t timestamp_;  // const after creation

  void Delete(const SnapshotImpl* s) {
    delete s;
  }

};

}  // namespace shannon

#endif  // SHANNON_DB_INCLUDE_SNAPSHOT_H_