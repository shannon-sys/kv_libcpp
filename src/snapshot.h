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
  void Delete(const SnapshotImpl* s) {
    delete s;
  }

  virtual SequenceNumber GetSequenceNumber() const {
    return snapshot_id_;
  }

  virtual void SetSequenceNumber(SequenceNumber seq) {
    snapshot_id_ = seq;
  }

 private:
  SequenceNumber snapshot_id_;  // const after creation
};

}  // namespace shannon

#endif  // SHANNON_DB_INCLUDE_SNAPSHOT_H_
